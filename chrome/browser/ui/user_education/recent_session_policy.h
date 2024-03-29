// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_POLICY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_POLICY_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"

// Class that defines policy for handling recent sessions.
// Allows recording of metrics and possibly modifying promo behavior.
class RecentSessionPolicy {
 public:
  virtual ~RecentSessionPolicy() = default;

  // Records any metrics associated with tracking recent usage.
  virtual void RecordRecentUsageMetrics(
      const RecentSessionData& recent_sessions) = 0;

  // Determines whether low usage should be taken into account when choosing
  // whether to show promos.
  virtual bool ShouldEnableLowUsagePromoMode(
      const RecentSessionData& recent_sessions) const = 0;
};

// Uses a series of thresholds to determine if this is a high- or low-usage
// profile/installation.
class RecentSessionPolicyImpl : public RecentSessionPolicy {
 public:
  // Describes a limit on number of sessions in a particular period of time,
  // at or above which this is not a "low-usage" browser. The form is
  // "N usages in time delta T".
  struct UsageThreshold {
    int count = 0;
    base::TimeDelta period;
  };
  using UsageThresholds = std::vector<UsageThreshold>;

  // Creates a policy with the given thresholds.
  // The thresholds should be sorted in order of time delta, ascending; for
  // example, "2 in one week, 5 in one month".
  //
  // Due to limitations in the histogram system, only the first and last
  // thresholds will be logged.
  explicit RecentSessionPolicyImpl(
      const UsageThresholds& thresholds = GetDefaultThresholds());
  RecentSessionPolicyImpl(const RecentSessionPolicyImpl&) = delete;
  void operator=(const RecentSessionPolicyImpl&) = delete;
  ~RecentSessionPolicyImpl() override;

  // RecentSessionPolicy:
  void RecordRecentUsageMetrics(
      const RecentSessionData& recent_sessions) override;
  bool ShouldEnableLowUsagePromoMode(
      const RecentSessionData& recent_sessions) const override;

  UsageThresholds thresholds_for_testing() const { return thresholds_; }
  void set_thresholds_for_testing(UsageThresholds thresholds) {
    CHECK(!thresholds.empty());
    thresholds_ = thresholds;
  }

 private:
  static UsageThresholds GetDefaultThresholds();

  // Gets the counts of sessions under each threshold.
  //
  // If the feature has not been active long enough to satisfy the threshold,
  // instead stores `std::nullopt` for tha threshold.
  std::vector<std::optional<int>> GetThresholdCounts(
      const RecentSessionData& recent_sessions) const;

  UsageThresholds thresholds_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_POLICY_H_
