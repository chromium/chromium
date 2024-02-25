// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/fake_nearby_scheduler.h"

#include <utility>

namespace ash::nearby {

FakeNearbyScheduler::FakeNearbyScheduler(OnRequestCallback callback)
    : NearbyScheduler(std::move(callback)) {}

FakeNearbyScheduler::~FakeNearbyScheduler() = default;

void FakeNearbyScheduler::MakeImmediateRequest() {
  ++num_immediate_requests_;
}

void FakeNearbyScheduler::HandleResult(bool success) {
  handled_results_.push_back(success);

  if (handle_result_callback_) {
    std::move(handle_result_callback_).Run();
  }
}

void FakeNearbyScheduler::Reschedule() {
  ++num_reschedule_calls_;
}

std::optional<base::Time> FakeNearbyScheduler::GetLastSuccessTime() const {
  return last_success_time_;
}

std::optional<base::TimeDelta> FakeNearbyScheduler::GetTimeUntilNextRequest()
    const {
  return time_until_next_request_;
}

bool FakeNearbyScheduler::IsWaitingForResult() const {
  return is_waiting_for_result_;
}

size_t FakeNearbyScheduler::GetNumConsecutiveFailures() const {
  return num_consecutive_failures_;
}

void FakeNearbyScheduler::OnStart() {
  can_invoke_request_callback_ = true;
}

void FakeNearbyScheduler::OnStop() {
  can_invoke_request_callback_ = false;
}

void FakeNearbyScheduler::InvokeRequestCallback() {
  DCHECK(can_invoke_request_callback_);
  NotifyOfRequest();
}

void FakeNearbyScheduler::SetLastSuccessTime(std::optional<base::Time> time) {
  last_success_time_ = time;
}

void FakeNearbyScheduler::SetTimeUntilNextRequest(
    std::optional<base::TimeDelta> time_delta) {
  time_until_next_request_ = time_delta;
}

void FakeNearbyScheduler::SetIsWaitingForResult(bool is_waiting) {
  is_waiting_for_result_ = is_waiting;
}

void FakeNearbyScheduler::SetNumConsecutiveFailures(size_t num_failures) {
  num_consecutive_failures_ = num_failures;
}

void FakeNearbyScheduler::SetHandleResultCallback(base::OnceClosure callback) {
  handle_result_callback_ = std::move(callback);
}

}  // namespace ash::nearby
