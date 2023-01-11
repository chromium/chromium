// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_SYSTEM_TIME_CHANGE_NOTIFIER_H_
#define CHROMECAST_BASE_SYSTEM_TIME_CHANGE_NOTIFIER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromecast {

// Abstract class to notify system time changes.
class SystemTimeChangeNotifier {
 public:
  class Observer {
   public:
    // Called when current system time has changed.
    virtual void OnSystemTimeChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  SystemTimeChangeNotifier(const SystemTimeChangeNotifier&) = delete;
  SystemTimeChangeNotifier& operator=(const SystemTimeChangeNotifier&) = delete;

  virtual ~SystemTimeChangeNotifier();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  SystemTimeChangeNotifier();

  // Implementations are expected to call this to notify system time changes.
  void NotifySystemTimeChanged();

 private:
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;
};

// Default implementation of SystemTimeChangeNotifier for most platform.
// It monitors system time changes periodically and notifies if the current
// system time is different from the expected one by more than 10 seconds.
// After reboot or system time has changed, it checks for time changes every
// second for first 1 minute. Then, checks every 10 seconds for next 10 minutes.
// Then, checks every 10 minutes.
class SystemTimeChangeNotifierPeriodicMonitor
    : public SystemTimeChangeNotifier {
 public:
  explicit SystemTimeChangeNotifierPeriodicMonitor(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  SystemTimeChangeNotifierPeriodicMonitor(
      const SystemTimeChangeNotifierPeriodicMonitor&) = delete;
  SystemTimeChangeNotifierPeriodicMonitor& operator=(
      const SystemTimeChangeNotifierPeriodicMonitor&) = delete;

  ~SystemTimeChangeNotifierPeriodicMonitor() override;

  // For unittests.
  void set_fake_now_for_testing(base::Time now) { fake_now_ = now; }

 private:
  // Helper functions to monitor system time changes.
  void ResetTimeAndLimits(base::Time now);
  void ScheduleNextMonitor(base::Time now);
  void CheckSystemTime();

  // Returns base::Time::Now() unless fake_now is set.
  base::Time Now() const;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::Time expected_system_time_;
  base::Time monitoring_limit_time_1sec_;
  base::Time monitoring_limit_time_10sec_;

  base::Time fake_now_;

  base::WeakPtrFactory<SystemTimeChangeNotifierPeriodicMonitor> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_SYSTEM_TIME_CHANGE_NOTIFIER_H_
