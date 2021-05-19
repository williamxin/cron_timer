﻿#pragma once
#include <string>
#include <list>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <assert.h>
#include <time.h>
#include <cstring>
#include <atomic>
#include <thread>

/*
	cron_timer::TimerMgr mgr;

	mgr.AddTimer("* * * * * *", [](void) {
		// 每秒钟都会执行
		Log("1 second cron timer hit");
	});

	mgr.AddTimer("0/3 * * * * *", [](void) {
		// 从0秒开始，每3秒钟执行一次
		Log("3 second cron timer hit");
	});

	mgr.AddTimer("0 * * * * *", [](void) {
		// 1分钟执行一次（每次都在0秒的时候执行）的定时器
		Log("1 minute cron timer hit");
	});

	mgr.AddTimer("15;30;33 * * * * *", [](void) {
		// 指定时间点执行的定时器
		Log("cron timer hit at 15s or 30s or 33s");
	});

	mgr.AddTimer("40-50 * * * * *", [](void) {
		// 指定时间段执行的定时器
		Log("cron timer hit between 40s to 50s");
	});

	auto timer = mgr.AddTimer(3, [](void) {
		// 3秒钟之后执行
		Log("3 second delay timer hit");
	});

	// 中途可以取消
	timer->Cancel();

	mgr.AddTimer(
		10,
		[](void) {
			// 10秒钟之后执行
			Log("10 second delay timer hit");
		},
		3);

	while (!_shutDown) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		mgr.Update();
	}
*/

namespace cron_timer {
class Text {
public:
	static size_t SplitStr(std::vector<std::string>& os, const std::string& is, const char c) {
		os.clear();
		std::string::size_type pos_find = is.find_first_of(c, 0);
		std::string::size_type pos_last_find = 0;
		while (std::string::npos != pos_find) {
			std::string::size_type num_char = pos_find - pos_last_find;
			os.emplace_back(is.substr(pos_last_find, num_char));

			pos_last_find = pos_find + 1;
			pos_find = is.find_first_of(c, pos_last_find);
		}

		std::string::size_type num_char = is.length() - pos_last_find;
		if (num_char > is.length())
			num_char = is.length();

		std::string sub = is.substr(pos_last_find, num_char);
		os.emplace_back(sub);
		return os.size();
	}

	static size_t SplitInt(std::vector<int>& number_result, const std::string& is, const char c) {
		std::vector<std::string> string_result;
		SplitStr(string_result, is, c);

		number_result.clear();
		for (size_t i = 0; i < string_result.size(); i++) {
			const std::string& value = string_result[i];
			number_result.emplace_back(atoi(value.data()));
		}

		return number_result.size();
	}
};

class CronExpression {
public:
	enum DATA_TYPE {
		DT_SECOND = 0,
		DT_MINUTE = 1,
		DT_HOUR = 2,
		DT_DAY_OF_MONTH = 3,
		DT_MONTH = 4,
		DT_YEAR = 5,
		DT_MAX,
	};

	// 获得数值枚举
	static bool GetValues(const std::string& input, DATA_TYPE data_type, std::vector<int>& values) {

		//
		// 注意：枚举之前是','，为了能在csv中使用改成了';'
		// 20181226 xinyong
		//

		static const char CRON_SEPERATOR_ENUM = ';';
		static const char CRON_SEPERATOR_RANGE = '-';
		static const char CRON_SEPERATOR_INTERVAL = '/';

		if (input == "*") {
			auto pair_range = GetRangeFromType(data_type);
			for (auto i = pair_range.first; i <= pair_range.second; ++i) {
				values.push_back(i);
			}
		} else if (input.find_first_of(CRON_SEPERATOR_ENUM) != std::string::npos) {
			//枚举
			std::vector<int> v;
			Text::SplitInt(v, input, CRON_SEPERATOR_ENUM);
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			for (auto value : v) {
				if (value < pair_range.first || value > pair_range.second) {
					return false;
				}
				values.push_back(value);
			}
		} else if (input.find_first_of(CRON_SEPERATOR_RANGE) != std::string::npos) {
			//范围
			std::vector<int> v;
			Text::SplitInt(v, input, CRON_SEPERATOR_RANGE);
			if (v.size() != 2) {
				return false;
			}

			int from = v[0];
			int to = v[1];
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			if (from < pair_range.first || to > pair_range.second) {
				return false;
			}

			for (int i = from; i <= to; i++) {
				values.push_back(i);
			}
		} else if (input.find_first_of(CRON_SEPERATOR_INTERVAL) != std::string::npos) {
			//间隔
			std::vector<int> v;
			Text::SplitInt(v, input, CRON_SEPERATOR_INTERVAL);
			if (v.size() != 2) {
				return false;
			}

			int from = v[0];
			int interval = v[1];
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			if (from < pair_range.first || interval < 0) {
				return false;
			}

			for (int i = from; i <= pair_range.second; i += interval) {
				values.push_back(i);
			}
		} else {
			//具体数值
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			int value = atoi(input.data());
			if (value < pair_range.first || value > pair_range.second) {
				return false;
			}

			values.push_back(value);
		}

		assert(values.size() > 0);
		return values.size() > 0;
	}

private:

	// 获得范围
	static std::pair<int, int> GetRangeFromType(DATA_TYPE data_type) {
		int from = 0;
		int to = 0;

		switch (data_type) {
		case CronExpression::DT_SECOND:
		case CronExpression::DT_MINUTE:
			from = 0;
			to = 59;
			break;
		case CronExpression::DT_HOUR:
			from = 0;
			to = 23;
			break;
		case CronExpression::DT_DAY_OF_MONTH:
			from = 1;
			to = 31;
			break;
		case CronExpression::DT_MONTH:
			from = 1;
			to = 12;
			break;
		case CronExpression::DT_YEAR:
			from = 1970;
			to = 2099;
			break;
		case CronExpression::DT_MAX:
			assert(false);
			break;
		}

		return std::make_pair(from, to);
	}
};

struct CronWheel {
	CronWheel()
		: cur_index(0) {}

	//返回值：是否有进位
	bool init(int init_value) {
		for (size_t i = cur_index; i < values.size(); ++i) {
			if (values[i] >= init_value) {
				cur_index = i;
				return false;
			}
		}

		cur_index = 0;
		return true;
	}

	size_t cur_index;
	std::vector<int> values;
};

class TimerMgr;
using FUNC_CALLBACK = std::function<void()>;

class BaseTimer : public std::enable_shared_from_this<BaseTimer> {
public:
	BaseTimer(TimerMgr& owner, const FUNC_CALLBACK& func)
		: owner_(owner)
		, func_(func) {}

	inline void Cancel();
	void SetIt(const std::list<std::shared_ptr<BaseTimer>>::iterator& it) { it_ = it; }
	std::list<std::shared_ptr<BaseTimer>>::iterator& GetIt() { return it_; }

	virtual void DoFunc() = 0;
	virtual time_t GetCurTime() const = 0;

protected:
	TimerMgr& owner_;
	const FUNC_CALLBACK func_;
	std::list<std::shared_ptr<BaseTimer>>::iterator it_;
};

class CronTimer : public BaseTimer {
public:
	CronTimer(TimerMgr& owner, const std::vector<CronWheel>& wheels, const FUNC_CALLBACK& func, int count)
		: BaseTimer(owner, func)
		, wheels_(wheels)
		, over_flowed_(false)
		, count_left_(count) {
		tm local_tm;
		time_t time_now = time(nullptr);

#ifdef _WIN32
		localtime_s(&local_tm, &time_now);
#else
		localtime_r(&time_now, &local_tm);
#endif // _WIN32

		std::vector<int> init_values;
		init_values.push_back(local_tm.tm_sec);
		init_values.push_back(local_tm.tm_min);
		init_values.push_back(local_tm.tm_hour);
		init_values.push_back(local_tm.tm_mday);
		init_values.push_back(local_tm.tm_mon + 1);
		init_values.push_back(local_tm.tm_year + 1900);

		bool addup = false;
		for (int i = 0; i < CronExpression::DT_MAX; i++) {
			auto& wheel = wheels_[i];
			auto init_value = addup ? init_values[i] + 1 : init_values[i];
			addup = wheel.init(init_value);
		}
	}

	inline void DoFunc() override;
	time_t GetCurTime() const override {
		tm next_tm;
		memset(&next_tm, 0, sizeof(next_tm));
		next_tm.tm_sec = GetCurValue(CronExpression::DT_SECOND);
		next_tm.tm_min = GetCurValue(CronExpression::DT_MINUTE);
		next_tm.tm_hour = GetCurValue(CronExpression::DT_HOUR);
		next_tm.tm_mday = GetCurValue(CronExpression::DT_DAY_OF_MONTH);
		next_tm.tm_mon = GetCurValue(CronExpression::DT_MONTH) - 1;
		next_tm.tm_year = GetCurValue(CronExpression::DT_YEAR) - 1900;

		return mktime(&next_tm);
	}

private:

	// 前进到下一格
	void Next(int data_type) {
		if (data_type >= CronExpression::DT_MAX) {
			// 溢出了表明此定时器已经失效，不应该再被执行
			over_flowed_ = true;
			return;
		}

		auto& wheel = wheels_[data_type];
		if (wheel.cur_index == wheel.values.size() - 1) {
			wheel.cur_index = 0;
			Next(data_type + 1);
		} else {
			++wheel.cur_index;
		}
	}

	int GetCurValue(int data_type) const {
		const auto& wheel = wheels_[data_type];
		return wheel.values[wheel.cur_index];
	}

private:
	std::vector<CronWheel> wheels_;
	bool over_flowed_;
	int count_left_;
};

class LaterTimer : public BaseTimer {
public:
	LaterTimer(TimerMgr& owner, int seconds, const FUNC_CALLBACK& func, int count)
		: BaseTimer(owner, func)
		, seconds_(seconds)
		, count_left_(count) {
		cur_time_ = time(nullptr);
		Next();
	}

	inline void DoFunc() override;
	time_t GetCurTime() const override { return cur_time_; }

private:

	//前进到下一格
	void Next() {
		time_t time_now = time(nullptr);
		while (true) {
			cur_time_ += seconds_;
			if (cur_time_ > time_now) {
				break;
			}
		}
	}

private:
	const int seconds_;
	time_t cur_time_;
	int count_left_;
};

class TimerMgr {
public:
	enum {
		RUN_FOREVER = 0,
	};

	TimerMgr() { last_proc_ = time(nullptr); }
	TimerMgr(const FUNC_CALLBACK& func) {
		update_func_ = func;
		last_proc_ = time(nullptr);
		thread_stop_ = false;
		thread_ = std::make_shared<std::thread>([this]() { this->ThreadProc(); });
	}

	~TimerMgr() {
		timers_.clear();
		if (thread_ != nullptr) {
			thread_stop_ = true;
			thread_->join();
			thread_ = nullptr;
		}
	}

	// 新增一个Cron表达式的定时器
	std::shared_ptr<BaseTimer> AddTimer(
		const std::string& timer_string, const FUNC_CALLBACK& func, int count = RUN_FOREVER) {
		std::vector<std::string> v;
		Text::SplitStr(v, timer_string, ' ');
		if (v.size() != CronExpression::DT_MAX) {
			assert(false);
			return nullptr;
		}

		std::vector<CronWheel> wheels;
		for (int i = 0; i < CronExpression::DT_MAX; i++) {
			const auto& expression = v[i];
			CronExpression::DATA_TYPE data_type = CronExpression::DATA_TYPE(i);
			CronWheel wheel;
			if (!CronExpression::GetValues(expression, data_type, wheel.values)) {
				assert(false);
				return nullptr;
			}

			wheels.emplace_back(wheel);
		}

		auto p = std::make_shared<CronTimer>(*this, wheels, func, count);
		insert(p);
		return p;
	}

	// 新增一个延时执行的定时器
	std::shared_ptr<BaseTimer> AddTimer(int seconds, const FUNC_CALLBACK& func, int count = 1) {
		assert(seconds > 0);
		seconds = (std::max)(seconds, 1);
		auto p = std::make_shared<LaterTimer>(*this, seconds, func, count);
		insert(p);
		return p;
	}

	size_t Update() {
		time_t time_now = time(nullptr);
		size_t count = 0;
		if (time_now == last_proc_)
			return 0;

		last_proc_ = time_now;
		while (!timers_.empty()) {
			auto& first = *timers_.begin();
			auto expire_time = first.first;
			auto& timer_list = first.second;
			if (expire_time > time_now) {
				break;
			}

			while (!timer_list.empty()) {
				auto p = *timer_list.begin();
				p->DoFunc();
				++count;
			}

			timers_.erase(timers_.begin());
		}

		return count;
	}

	void insert(const std::shared_ptr<BaseTimer>& p) {
		time_t t = p->GetCurTime();
		auto it = timers_.find(t);
		if (it == timers_.end()) {
			std::list<std::shared_ptr<BaseTimer>> l;
			timers_.insert(std::make_pair(t, l));
			it = timers_.find(t);
		}

		auto& l = it->second;
		p->SetIt(l.insert(l.end(), p));
	}

	bool remove(const std::shared_ptr<BaseTimer>& p) {
		time_t t = p->GetCurTime();
		auto it = timers_.find(t);
		if (it == timers_.end()) {
			return false;
		}

		auto& l = it->second;
		if (p->GetIt() == l.end()) {
			// 删除了多次？
			assert(false);
			return false;
		}

		l.erase(p->GetIt());
		p->SetIt(l.end());
		return true;
	}

private:
	void ThreadProc() {
		while (!thread_stop_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			update_func_();
		}
	}

private:
	std::map<time_t, std::list<std::shared_ptr<BaseTimer>>> timers_;
	time_t last_proc_ = 0;

	FUNC_CALLBACK update_func_;
	std::shared_ptr<std::thread> thread_;
	std::atomic_bool thread_stop_;
};

void BaseTimer::Cancel() {
	auto self = shared_from_this();
	owner_.remove(self);
}

void CronTimer::DoFunc() {
	func_();
	auto self = shared_from_this();
	owner_.remove(self);

	Next(CronExpression::DT_SECOND);
	if ((count_left_ <= TimerMgr::RUN_FOREVER || --count_left_ > 0) && !over_flowed_) {
		owner_.insert(self);
	}
}

void LaterTimer::DoFunc() {
	func_();
	auto self = shared_from_this();
	owner_.remove(self);

	if (count_left_ <= TimerMgr::RUN_FOREVER || --count_left_ > 0) {
		Next();
		owner_.insert(self);
	}
}

} // namespace cron_timer
