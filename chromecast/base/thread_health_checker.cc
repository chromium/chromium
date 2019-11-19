// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/thread_health_checker.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chromecast/base/bind_to_task_runner.h"

namespace chromecast {

ThreadHealthChecker::Internal::Internal(
    scoped_refptr<base::TaskRunner> patient_task_runner,
    scoped_refptr<base::SequencedTaskRunner> doctor_task_runner,
    base::TimeDelta interval,
    base::TimeDelta timeout,
    base::RepeatingClosure on_failure)
    : patient_task_runner_(std::move(patient_task_runner)),
      doctor_task_runner_(std::move(doctor_task_runner)),
      interval_(interval),
      timeout_(timeout),
      on_failure_(std::move(on_failure)) {
  DCHECK(patient_task_runner_);
  DCHECK(doctor_task_runner_);
}

ThreadHealthChecker::Internal::~Internal() {}

void ThreadHealthChecker::Internal::StartHealthCheck() {
  DCHECK(doctor_task_runner_->RunsTasksInCurrentSequence());
  DETACH_FROM_THREAD(thread_checker_);
  ok_timer_ = std::make_unique<base::OneShotTimer>();
  failure_timer_ = std::make_unique<base::OneShotTimer>();
  ScheduleHealthCheck();
}

void ThreadHealthChecker::Internal::StopHealthCheck() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ok_timer_);
  DCHECK(failure_timer_);
  ok_timer_->Stop();
  failure_timer_->Stop();
}

void ThreadHealthChecker::Internal::ScheduleHealthCheck() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ok_timer_->Start(FROM_HERE, interval_, this,
                   &ThreadHealthChecker::Internal::CheckThreadHealth);
}

void ThreadHealthChecker::Internal::CheckThreadHealth() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!failure_timer_->IsRunning());
  patient_task_runner_->PostTask(
      FROM_HERE, BindToCurrentThread(base::BindOnce(
                     &ThreadHealthChecker::Internal::ThreadOk, this)));
  failure_timer_->Start(FROM_HERE, timeout_, this,
                        &ThreadHealthChecker::Internal::ThreadTimeout);
}

void ThreadHealthChecker::Internal::ThreadOk() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  failure_timer_->Stop();
  ScheduleHealthCheck();
}

void ThreadHealthChecker::Internal::ThreadTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  on_failure_.Run();
  ScheduleHealthCheck();
}

// The public ThreadHealthChecker owns a ref-counted reference to an Internal
// object which does the heavy lifting.
ThreadHealthChecker::ThreadHealthChecker(
    scoped_refptr<base::TaskRunner> patient_task_runner,
    scoped_refptr<base::SequencedTaskRunner> doctor_task_runner,
    base::TimeDelta interval,
    base::TimeDelta timeout,
    base::RepeatingClosure on_failure)
    : doctor_task_runner_(doctor_task_runner),
      internal_(base::MakeRefCounted<ThreadHealthChecker::Internal>(
          patient_task_runner,
          doctor_task_runner,
          interval,
          timeout,
          std::move(on_failure))) {
  doctor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ThreadHealthChecker::Internal::StartHealthCheck,
                     internal_));
}

// When the public ThreadHealthChecker is destroyed, the reference to the
// Internal representation is freed, but if there are any pending tasks on the
// doctor thread that partially own Internal, they will be run asynchronously
// before the Internal object is destroyed and the health check stops.
ThreadHealthChecker::~ThreadHealthChecker() {
  doctor_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ThreadHealthChecker::Internal::StopHealthCheck,
                                internal_));
}

}  // namespace chromecast
