// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/utils/backoff_operator.h"

namespace safe_browsing {

BackoffOperator::BackoffOperator(size_t num_failures_to_enforce_backoff,
                                 size_t min_backoff_reset_duration_in_seconds,
                                 size_t max_backoff_reset_duration_in_seconds)
    : num_failures_to_enforce_backoff_(num_failures_to_enforce_backoff),
      min_backoff_reset_duration_in_seconds_(
          min_backoff_reset_duration_in_seconds),
      max_backoff_reset_duration_in_seconds_(
          max_backoff_reset_duration_in_seconds) {}

BackoffOperator::~BackoffOperator() = default;

size_t BackoffOperator::GetBackoffDurationInSeconds() const {
  return did_successful_request_since_last_backoff_
             ? min_backoff_reset_duration_in_seconds_
             : std::min(max_backoff_reset_duration_in_seconds_,
                        2 * next_backoff_duration_secs_);
}

bool BackoffOperator::ReportError() {
  consecutive_failures_++;

  // Any successful request clears both |consecutive_failures_| as well as
  // |did_successful_request_since_last_backoff_|.
  // On a failure, the following happens:
  // 1) if |consecutive_failures_| < |num_failures_to_enforce_backoff_|:
  //    Do nothing more.
  // 2) if already in the backoff mode:
  //    Do nothing more. This can happen if we had some outstanding requests in
  //    flight when we entered the backoff mode.
  // 3) if |did_successful_request_since_last_backoff_| is true:
  //    Enter backoff mode for |min_backoff_reset_duration_in_seconds_| seconds.
  // 4) if |did_successful_request_since_last_backoff_| is false:
  //    This indicates that we've had |num_failures_to_enforce_backoff_| since
  //    exiting the last backoff with no successful requests since, so do an
  //    exponential backoff.

  if (consecutive_failures_ < num_failures_to_enforce_backoff_)
    return false;

  if (IsInBackoffMode()) {
    return false;
  }

  // Enter backoff mode, calculate duration.
  next_backoff_duration_secs_ = GetBackoffDurationInSeconds();
  backoff_timer_.Start(FROM_HERE, base::Seconds(next_backoff_duration_secs_),
                       this, &BackoffOperator::ResetFailures);
  last_backoff_start_time_ = base::Time::Now();
  did_successful_request_since_last_backoff_ = false;
  return true;
}

void BackoffOperator::ReportSuccess() {
  ResetFailures();

  // |did_successful_request_since_last_backoff_| is set to true only when a
  // request completes successfully.
  did_successful_request_since_last_backoff_ = true;
}

bool BackoffOperator::IsInBackoffMode() const {
  return backoff_timer_.IsRunning();
}

base::TimeDelta BackoffOperator::GetBackoffRemainingDuration() {
  return IsInBackoffMode() ? base::Seconds(next_backoff_duration_secs_) -
                                 (base::Time::Now() - last_backoff_start_time_)
                           : base::Seconds(0);
}

void BackoffOperator::ResetFailures() {
  consecutive_failures_ = 0;
  backoff_timer_.Stop();
}

}  // namespace safe_browsing
