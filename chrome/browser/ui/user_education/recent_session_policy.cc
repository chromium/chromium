// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_policy.h"

#include "base/dcheck_is_on.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/user_education/recent_session_tracker.h"
#include "chrome/browser/user_education/user_education_service.h"

RecentSessionPolicyImpl::RecentSessionPolicyImpl(
    const UsageThresholds& thresholds)
    : thresholds_(thresholds) {
  CHECK(!thresholds_.empty());
  if (DCHECK_IS_ON()) {
    for (size_t i = 1; i < thresholds_.size(); ++i) {
      CHECK_GT(thresholds_[i].count, thresholds_[i - 1].count)
          << "Threshold values are in the wrong order; values should go from "
             "smallest to largest.";
      CHECK_GT(thresholds_[i].period, thresholds_[i - 1].period)
          << "Threshold periods are in the wrong order; values should go from "
             "shortest to longest period.";
    }
  }
}

RecentSessionPolicyImpl::~RecentSessionPolicyImpl() = default;

void RecentSessionPolicyImpl::RecordRecentUsageMetrics(
    const RecentSessionData& recent_sessions) {
  const auto counts = GetThresholdCounts(recent_sessions);
  constexpr int kMaxValue = RecentSessionTracker::kMaxRecentSessionRecords;

  // Record counts in the smallest threshold.
  if (counts[0]) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("UserEducation.Session.ShortTermCount",
                                *counts[0], 1, kMaxValue + 1, kMaxValue);
  }

  // Record counts in the largest threshold.
  const size_t last_idx = counts.size() - 1;
  if (counts[last_idx]) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("UserEducation.Session.LongTermCount",
                                *counts[last_idx], 1, kMaxValue + 1, kMaxValue);
  }
}

bool RecentSessionPolicyImpl::ShouldEnableLowUsagePromoMode(
    const RecentSessionData& recent_sessions) const {
  const auto counts = GetThresholdCounts(recent_sessions);
  CHECK_EQ(counts.size(), thresholds_.size());
  for (size_t i = 0; i < counts.size(); ++i) {
    if (!counts[i] || *counts[i] >= thresholds_[i].count) {
      return false;
    }
  }
  return true;
}

std::vector<std::optional<int>> RecentSessionPolicyImpl::GetThresholdCounts(
    const RecentSessionData& recent_sessions) const {
  CHECK(recent_sessions.enabled_time);

  // Assume that the most recent session is approximately "now", since this
  // method should only be checked on a new session.
  const base::Time latest_session =
      recent_sessions.recent_session_start_times.front();

  // The total number of sessions within the current threshold.
  // Since threshold lengths and limits are monotonically increasing, a single
  // count may be kept.
  int count = 0;

  // Which threshold is currently being checked.
  auto threshold = thresholds_.begin();

  // Helper function to determine when to store a value for each threshold and
  // when to store "no value because the minimum time for this threshold has not
  // been met".
  const auto get_count_value = [valid_period = latest_session -
                                               *recent_sessions.enabled_time,
                                &count](const UsageThreshold& threshold) {
    return threshold.period <= valid_period ? std::make_optional(count)
                                            : std::nullopt;
  };

  // Iterate through the sessions from most recent to least recent, recording
  // counts when thresholds are crossed.
  std::vector<std::optional<int>> counts;
  for (const auto& start_time : recent_sessions.recent_session_start_times) {
    // If the current threshold duration is exceeded, store the count and
    // advance to the next threshold (at least until there are no more
    // thresholds).
    const auto age = latest_session - start_time;
    while (threshold != thresholds_.end() && age > threshold->period) {
      counts.emplace_back(get_count_value(*threshold));
      ++threshold;
    }
    if (threshold == thresholds_.end()) {
      break;
    }
    // The session fits within the current threshold so advance the count.
    ++count;
  }

  // If the sessions were exhausted without completing the thresholds, the count
  // continues to apply to the remaining thresholds, since they are guaranteed
  // to be of longer duration.
  for (; threshold != thresholds_.end(); ++threshold) {
    counts.emplace_back(get_count_value(*threshold));
  }
  return counts;
}

// static
RecentSessionPolicyImpl::UsageThresholds
RecentSessionPolicyImpl::GetDefaultThresholds() {
  const auto short_period = base::GetFieldTrialParamByFeatureAsTimeDelta(
      kAllowRecentSessionTracking, "short_period", base::Days(7));
  const auto short_period_count = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "short_period_count", 2);
  const auto long_period = base::GetFieldTrialParamByFeatureAsTimeDelta(
      kAllowRecentSessionTracking, "long_period", base::Days(21));
  const auto long_period_count = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "long_period_count", 5);
  return {UsageThreshold{short_period_count, short_period},
          UsageThreshold{long_period_count, long_period}};
}
