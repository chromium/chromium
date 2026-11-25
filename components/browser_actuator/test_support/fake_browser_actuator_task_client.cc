// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/test_support/fake_browser_actuator_task_client.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace browser_actuator {

namespace {

using CompletionStatus = BrowserActuatorTaskClient::CompletionStatus;

}  // namespace

FakeBrowserActuatorTaskClient::FakeBrowserActuatorTaskClient() = default;

FakeBrowserActuatorTaskClient::~FakeBrowserActuatorTaskClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeBrowserActuatorTaskClient::SetIdleTimeout(
    base::TimeDelta idle_timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kNotStarted);
  idle_timeout_ = idle_timeout;
}

void FakeBrowserActuatorTaskClient::StartTask(
    UseCase use_case,
    const google::protobuf::MessageLite& feature_payload,
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!start_task_called()) << "StartTask() is valid once.";
  CHECK(observer);

  observer_ = observer;
  use_case_ = use_case;
  feature_payload_type_url_ =
      base::StrCat({"type.googleapis.com/", feature_payload.GetTypeName()});
  serialized_feature_payload_ = feature_payload.SerializeAsString();

  if (state_ == State::kDetached) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FakeBrowserActuatorTaskClient::NotifyTaskCompleted,
            weak_factory_.GetWeakPtr(),
            Result{.status = CompletionStatus::kFailed,
                   .error_message = std::string(kShutdownErrorMessage)}));
    return;
  }

  state_ = State::kRunning;
  ResetIdleTimer();
}

void FakeBrowserActuatorTaskClient::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++cancel_call_count_;
  // As the interface requires, `Cancel()` is a no-op unless a task is running.
  if (state_ != State::kRunning) {
    return;
  }

  idle_timer_.Stop();
  state_ = State::kCancelling;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeBrowserActuatorTaskClient::NotifyTaskCompleted,
                     weak_factory_.GetWeakPtr(),
                     Result{.status = CompletionStatus::kCancelled}));
}

void FakeBrowserActuatorTaskClient::SimulateTaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kRunning) {
    return;
  }
  NotifyTaskStarted();
}

void FakeBrowserActuatorTaskClient::SimulateWorklogUpdate(
    std::string_view step_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kRunning) {
    return;
  }
  NotifyWorklogUpdate(step_description);
}

void FakeBrowserActuatorTaskClient::SimulateTaskCompleted(
    const Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kRunning) {
    return;
  }
  NotifyTaskCompleted(result);
}

void FakeBrowserActuatorTaskClient::SimulateShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A completed client has nothing left to detach.
  if (state_ == State::kCompleted) {
    return;
  }
  idle_timer_.Stop();
  observer_ = nullptr;
  state_ = State::kDetached;
  // Drops a pending cancellation or post-shutdown failure callback.
  weak_factory_.InvalidateWeakPtrs();
}

bool FakeBrowserActuatorTaskClient::start_task_called() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return use_case_.has_value();
}

std::optional<BrowserActuatorTaskClient::UseCase>
FakeBrowserActuatorTaskClient::use_case() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return use_case_;
}

const std::string& FakeBrowserActuatorTaskClient::feature_payload_type_url()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return feature_payload_type_url_;
}

const std::string& FakeBrowserActuatorTaskClient::serialized_feature_payload()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return serialized_feature_payload_;
}

int FakeBrowserActuatorTaskClient::cancel_call_count() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cancel_call_count_;
}

bool FakeBrowserActuatorTaskClient::is_completed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::kCompleted;
}

void FakeBrowserActuatorTaskClient::NotifyTaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Report `OnTaskStarted()` at most once.
  if (task_started_) {
    return;
  }
  task_started_ = true;
  ResetIdleTimer();
  observer_->OnTaskStarted();
}

void FakeBrowserActuatorTaskClient::NotifyWorklogUpdate(
    std::string_view step_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_started_) {
    return;
  }
  ResetIdleTimer();
  observer_->OnWorklogUpdate(step_description);
}

void FakeBrowserActuatorTaskClient::NotifyTaskCompleted(const Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kRunning || state_ == State::kCancelling ||
        state_ == State::kDetached);
  idle_timer_.Stop();
  state_ = State::kCompleted;
  // The observer can destroy `this`, so do not touch members after this call.
  std::exchange(observer_, nullptr)->OnTaskCompleted(result);
}

void FakeBrowserActuatorTaskClient::ResetIdleTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!idle_timeout_.is_positive()) {
    return;
  }
  // `base::Unretained` is safe: `idle_timer_` is a member, so its task cannot
  // run after `this` is destroyed.
  idle_timer_.Start(
      FROM_HERE, idle_timeout_,
      base::BindOnce(&FakeBrowserActuatorTaskClient::OnIdleTimeout,
                     base::Unretained(this)));
}

void FakeBrowserActuatorTaskClient::OnIdleTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kRunning) {
    return;
  }
  NotifyTaskCompleted({.status = CompletionStatus::kFailed,
                       .error_message = std::string(kIdleTimeoutErrorMessage)});
}

}  // namespace browser_actuator
