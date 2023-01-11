// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/system_time_change_notifier.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace chromecast {

namespace {

// Limits for periodic system time monitoring.
const int kLimitForMonitorPer1Sec = 60;  // 1 minute
const int kLimitForMonitorPer10Sec = 600;  // 10 minutes

}  // namespace

SystemTimeChangeNotifier::SystemTimeChangeNotifier()
    : observer_list_(new base::ObserverListThreadSafe<Observer>()) {}

SystemTimeChangeNotifier::~SystemTimeChangeNotifier() = default;

void SystemTimeChangeNotifier::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void SystemTimeChangeNotifier::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void SystemTimeChangeNotifier::NotifySystemTimeChanged() {
  observer_list_->Notify(FROM_HERE, &Observer::OnSystemTimeChanged);
}

SystemTimeChangeNotifierPeriodicMonitor::
SystemTimeChangeNotifierPeriodicMonitor(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      weak_factory_(this) {
  DCHECK(task_runner_);
  base::Time now = Now();
  ResetTimeAndLimits(now);
  ScheduleNextMonitor(now);
}

SystemTimeChangeNotifierPeriodicMonitor::
    ~SystemTimeChangeNotifierPeriodicMonitor() = default;

void SystemTimeChangeNotifierPeriodicMonitor::ResetTimeAndLimits(
    base::Time now) {
  // ScheduleNextMonitor() will adjust actual expected_system_time.
  expected_system_time_ = now;
  monitoring_limit_time_1sec_ = now + base::Seconds(kLimitForMonitorPer1Sec);
  monitoring_limit_time_10sec_ =
      monitoring_limit_time_1sec_ + base::Seconds(kLimitForMonitorPer10Sec);
}

void SystemTimeChangeNotifierPeriodicMonitor::ScheduleNextMonitor(
    base::Time now) {
  base::TimeDelta next_checking_interval =
      now <= monitoring_limit_time_1sec_
          ? base::Seconds(1)
          : now <= monitoring_limit_time_10sec_ ? base::Seconds(10)
                                                : base::Minutes(10);
  // Adjusting expected_system_time based on now cannot detect continuous system
  // time drift (false negative), but tolerates task delay (false positive).
  // Task delay is expected more than system time drift.
  expected_system_time_ = now + next_checking_interval;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SystemTimeChangeNotifierPeriodicMonitor::CheckSystemTime,
                     weak_factory_.GetWeakPtr()),
      next_checking_interval);
}

void SystemTimeChangeNotifierPeriodicMonitor::CheckSystemTime() {
  base::Time now = Now();
  const base::TimeDelta kInterval10Seconds(base::Seconds(10));
  if (now < expected_system_time_ - kInterval10Seconds ||
      now > expected_system_time_ + kInterval10Seconds) {  // Time changed!
    ResetTimeAndLimits(now);
    NotifySystemTimeChanged();
  }
  ScheduleNextMonitor(now);
}

base::Time SystemTimeChangeNotifierPeriodicMonitor::Now() const {
  if (!fake_now_.is_null())
    return fake_now_;
  return base::Time::Now();
}

}  // namespace chromecast
