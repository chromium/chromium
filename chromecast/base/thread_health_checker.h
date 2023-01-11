// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_THREAD_HEALTH_CHECKER_H_
#define CHROMECAST_BASE_THREAD_HEALTH_CHECKER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {
class OneShotTimer;
class SequencedTaskRunner;
class TaskRunner;
}  // namespace base

namespace chromecast {
// A class used to periodically check the responsiveness of a thread.
//
// The class takes two task runners, a "patient", and a "doctor". The doctor
// task runner will post a "sentinel" task to the patient and verify that it
// gets run within a certain response time, determined by |timeout|.
// If the task is not run in time, then the patient fails the health checkup and
// |on_failure| is invoked.
//
// The thread health is checked periodically, with the length between one check
// and the next determined by |interval|, and the amount of time allowed for the
// sentinel task to complete determined by |timeout|.
class ThreadHealthChecker {
 public:
  ThreadHealthChecker(
      scoped_refptr<base::TaskRunner> patient_task_runner,
      scoped_refptr<base::SequencedTaskRunner> doctor_task_runner,
      base::TimeDelta interval,
      base::TimeDelta timeout,
      base::RepeatingClosure on_failure);

  ThreadHealthChecker(const ThreadHealthChecker&) = delete;
  ThreadHealthChecker& operator=(const ThreadHealthChecker&) = delete;

  ~ThreadHealthChecker();

 private:
  class Internal : public base::RefCountedThreadSafe<Internal> {
   public:
    Internal(scoped_refptr<base::TaskRunner> patient_task_runner,
             scoped_refptr<base::SequencedTaskRunner> doctor_task_runner,
             base::TimeDelta interval,
             base::TimeDelta timeout,
             base::RepeatingClosure on_failure);
    void StartHealthCheck();
    void StopHealthCheck();

   private:
    friend class base::RefCountedThreadSafe<Internal>;
    ~Internal();
    void ScheduleHealthCheck();
    void CheckThreadHealth();
    void ThreadOk();
    void ThreadTimeout();

    scoped_refptr<base::TaskRunner> patient_task_runner_;
    scoped_refptr<base::SequencedTaskRunner> doctor_task_runner_;
    base::TimeDelta interval_;
    base::TimeDelta timeout_;
    std::unique_ptr<base::OneShotTimer> ok_timer_;
    std::unique_ptr<base::OneShotTimer> failure_timer_;
    base::RepeatingClosure on_failure_;
    THREAD_CHECKER(thread_checker_);
  };

  scoped_refptr<base::SequencedTaskRunner> doctor_task_runner_;
  scoped_refptr<Internal> internal_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_THREAD_HEALTH_CHECKER_H_
