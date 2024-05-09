// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_ALARM_MANAGER_H_
#define CHROMECAST_BASE_ALARM_MANAGER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class Clock;
class SingleThreadTaskRunner;
}

namespace chromecast {

// Alarm handle for scoping the in-flight alarm.
class AlarmHandle final {
 public:
  AlarmHandle();
  ~AlarmHandle();

  base::WeakPtr<AlarmHandle> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<AlarmHandle> weak_ptr_factory_{this};
};

// Alarm manager allows setting a task for wall clock time rather than for an
// elapsed amount of time. This is different from using long PostDelayedTasks
// that are sensitive to time changes, clock drift, and other factors.
//
// Alarm manager polls the wall clock time every 5 seconds. If the clock is
// equal or past the requested time, the alarm will fire.
class AlarmManager {
 public:
  // Construct and start the alarm manager. The clock poller will run on the
  // caller's thread.
  AlarmManager();

  AlarmManager(const AlarmManager&) = delete;
  AlarmManager& operator=(const AlarmManager&) = delete;

  ~AlarmManager();

  // For testing only. Allows setting a fake clock and using a custom task
  // runner.
  AlarmManager(const base::Clock* clock,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Add an alarm.
  // |task| will be executed at around |time|.
  // Returns an AlarmHandle that must be kept alive. If the AlarmHandle is
  // destroyed, the alarm will not fire.
  //
  // Any thread can add alarms. The alarm will be fired on the original
  // thread used to set the alarm.
  //
  // When an alarm is added to the alarm manager, the task is guaranteed to not
  // run before the clock passes the requested time. The task may not run even
  // if it is past the requested time if the software is suspended. However,
  // once woken up, the event will fire within 5 seconds if the target time has
  // passed.
  [[nodiscard]] std::unique_ptr<AlarmHandle> PostAlarmTask(
      base::OnceClosure task,
      base::Time time);

 private:
  class AlarmInfo {
   public:
    AlarmInfo(base::OnceClosure task,
              base::Time time,
              scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    AlarmInfo(const AlarmInfo&) = delete;
    AlarmInfo& operator=(const AlarmInfo&) = delete;

    ~AlarmInfo();

    void PostTask();

    base::Time time() const { return time_; }

   private:
    base::OnceClosure task_;
    const base::Time time_;
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  };

  // Check if an alarm should fire.
  void CheckAlarm();
  // Add the alarm to the queue.
  void AddAlarm(base::OnceClosure task,
                base::Time time,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Ordering alarms by earliest time.
  struct alarm_compare {
    bool operator()(const std::unique_ptr<AlarmInfo>& lhs,
                    const std::unique_ptr<AlarmInfo>& rhs) const {
      return lhs->time() > rhs->time();
    }
  };

  // Store a list of the alarms to fire with the earliest getting priority.
  std::priority_queue<std::unique_ptr<AlarmInfo>,
                      std::vector<std::unique_ptr<AlarmInfo>>,
                      alarm_compare>
      next_alarm_;

  // Poller for wall clock time.
  const base::Clock* const clock_;
  base::RepeatingTimer clock_tick_timer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<AlarmManager> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_ALARM_MANAGER_H_
