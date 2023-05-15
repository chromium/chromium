// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_BACKOFF_OPERATOR_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_BACKOFF_OPERATOR_H_

#include "base/timer/timer.h"

namespace safe_browsing {

// This class is responsible for tracking and updating exponential backoff mode.
// This should be used for consumers who want to be able to throttle requests
// sent in cases where they are consistently failing. This class maintains the
// status of the failures and decides when backoff mode should be enabled.
class BackoffOperator {
 public:
  // |num_failures_to_enforce_backoff| = The number of consecutive failures
  // that trigger backoff mode.
  // |min_backoff_reset_duration_in_seconds| = The initial and minimum backoff
  // mode duration.
  // |max_backoff_reset_duration_in_seconds| = The maximum duration that backoff
  // mode can last.
  BackoffOperator(size_t num_failures_to_enforce_backoff,
                  size_t min_backoff_reset_duration_in_seconds,
                  size_t max_backoff_reset_duration_in_seconds);
  BackoffOperator(const BackoffOperator&) = delete;
  BackoffOperator& operator=(const BackoffOperator&) = delete;
  ~BackoffOperator();

  // Returns true if the backoff mode is currently enabled due to too many prior
  // errors. If this happens, the consumer is responsible for deciding what to
  // do instead of the main request.
  bool IsInBackoffMode() const;

  // Should be called by the consumer when a request fails. May initiate or
  // extend backoff. Returns whether backoff mode becomes enabled as a result of
  // the call into this function.
  bool ReportError();

  // Should be called by the consumer when the request succeeds. Resets error
  // count and ends backoff.
  void ReportSuccess();

  // Gets the remaining duration in the backoff mode. Returns 0 if it is
  // currently not in backoff mode.
  base::TimeDelta GetBackoffRemainingDuration();

 private:
  // Returns the duration of the next backoff. Starts at
  // |min_backoff_reset_duration_in_seconds_| and increases exponentially until
  // it reaches |max_backoff_reset_duration_in_seconds_|.
  size_t GetBackoffDurationInSeconds() const;

  // Resets the error count and ends backoff mode.
  void ResetFailures();

  // Count of consecutive failures to complete requests. When it reaches
  // |max_backoff_reset_duration_in_seconds_|, we enter the backoff mode. It
  // gets reset when we complete a request successfully or when the backoff
  // reset timer fires.
  size_t consecutive_failures_ = 0;

  // If true, represents that one or more requests did complete successfully
  // since the last backoff, or backoff mode was never enabled. If false and
  // backoff mode is re-enabled, the backoff duration is increased exponentially
  // (capped at |max_backoff_reset_duration_in_seconds_|).
  bool did_successful_request_since_last_backoff_ = true;

  // The current duration of backoff. Increases exponentially until it reaches
  // |max_backoff_reset_duration_in_seconds_|.
  size_t next_backoff_duration_secs_ = 0;

  // If this timer is running, backoff is in effect.
  base::OneShotTimer backoff_timer_;

  // The last time when |backoff_timer_| starts to run.
  base::Time last_backoff_start_time_;

  // The number of consecutive failures that trigger backoff mode.
  size_t num_failures_to_enforce_backoff_;

  // The initial and minimum backoff mode duration.
  size_t min_backoff_reset_duration_in_seconds_;

  // The maximum duration that backoff mode can last.
  size_t max_backoff_reset_duration_in_seconds_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_UTILS_BACKOFF_OPERATOR_H_
