// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/recent_session_policy.h"

#include <optional>

#include "base/dcheck_is_on.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/user_education_service.h"

namespace {

constexpr int kMaxRecords = RecentSessionTracker::kMaxRecentSessionRecords;

std::optional<int> GetActivePeriods(const RecentSessionData& recent_sessions,
                                    int num_periods,
                                    int period_length_in_days,
                                    int min_count) {
  const base::Time end =
      (recent_sessions.recent_session_start_times.front() + base::Days(1))
          .LocalMidnight();
  const base::Time start =
      end - base::Days(num_periods * period_length_in_days);
  if (recent_sessions.enabled_time > start) {
    return std::nullopt;
  }

  const base::TimeDelta period_length = base::Days(period_length_in_days);
  std::vector<int> active_periods(num_periods, 0);
  for (const auto& start_time : recent_sessions.recent_session_start_times) {
    if (start_time < start) {
      continue;
    }
    if (start_time >= end) {
      ++active_periods.back();
      continue;
    }
    const size_t index = (start_time - start) / period_length;
    ++active_periods[index];
  }
  return std::count_if(active_periods.begin(), active_periods.end(),
                       [min_count](int value) { return value >= min_count; });
}

std::optional<int> ValueOrNull(int value) {
  return value ? std::make_optional(value) : std::nullopt;
}

}  // namespace

std::optional<int> RecentSessionPolicyImpl::SessionCountConstraint::GetCount(
    const RecentSessionData& recent_sessions) const {
  const base::Time start =
      recent_sessions.recent_session_start_times.front() - base::Days(days_);
  if (recent_sessions.enabled_time > start) {
    return std::nullopt;
  }
  int count = 0;
  for (const auto& start_time : recent_sessions.recent_session_start_times) {
    if (start_time >= start) {
      ++count;
    }
  }
  return count;
}

std::optional<int> RecentSessionPolicyImpl::ActiveDaysConstraint::GetCount(
    const RecentSessionData& recent_sessions) const {
  return GetActivePeriods(recent_sessions, days_, 1, 1);
}

std::optional<int> RecentSessionPolicyImpl::ActiveWeeksConstraint::GetCount(
    const RecentSessionData& recent_sessions) const {
  return GetActivePeriods(recent_sessions, weeks_, 7, active_days_);
}

RecentSessionPolicyImpl::ConstraintInfo::ConstraintInfo() = default;
RecentSessionPolicyImpl::ConstraintInfo::ConstraintInfo(
    std::unique_ptr<Constraint> constraint_,
    std::string histogram_name_,
    std::optional<int> histogram_max_,
    std::optional<int> low_usage_max_)
    : constraint(std::move(constraint_)),
      histogram_name(std::move(histogram_name_)),
      histogram_max(histogram_max_),
      low_usage_max(low_usage_max_) {}
RecentSessionPolicyImpl::ConstraintInfo::ConstraintInfo(
    ConstraintInfo&&) noexcept = default;
RecentSessionPolicyImpl::ConstraintInfo&
RecentSessionPolicyImpl::ConstraintInfo::operator=(ConstraintInfo&&) noexcept =
    default;
RecentSessionPolicyImpl::ConstraintInfo::~ConstraintInfo() = default;

RecentSessionPolicyImpl::RecentSessionPolicyImpl(ConstraintInfos constraints)
    : constraints_(std::move(constraints)) {
  CHECK(!constraints_.empty());
  for (const auto& constraint : constraints_) {
    CHECK(constraint.constraint);
  }
}

RecentSessionPolicyImpl::~RecentSessionPolicyImpl() = default;

void RecentSessionPolicyImpl::RecordRecentUsageMetrics(
    const RecentSessionData& recent_sessions) {
  for (const auto& constraint : constraints_) {
    if (!constraint.histogram_name.empty()) {
      if (const auto result =
              constraint.constraint->GetCount(recent_sessions)) {
        base::UmaHistogramExactLinear(
            constraint.histogram_name.c_str(), *result,
            constraint.histogram_max.value_or(kMaxRecords));
      }
    }
  }
}

bool RecentSessionPolicyImpl::ShouldEnableLowUsagePromoMode(
    const RecentSessionData& recent_sessions) const {
  for (const auto& constraint : constraints_) {
    if (const auto limit = constraint.low_usage_max) {
      const auto result = constraint.constraint->GetCount(recent_sessions);
      if (!result || *result > *limit) {
        return false;
      }
    }
  }
  return true;
}

// static
RecentSessionPolicyImpl::ConstraintInfos
RecentSessionPolicyImpl::GetDefaultConstraints() {
  static constexpr int kShortTermDays = 7;
  static constexpr int kLongTermWeeks = 4;
  static constexpr int kLongTermDays = kLongTermWeeks * 7;
  const int max_active_weeks = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_active_weeks", 2);
  const int max_active_days = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_active_days", 3);
  const int super_active_days = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "super_active_days", 4);
  const int max_super_active_weeks = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_super_active_weeks", 0);
  const int max_weekly_sessions = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_weekly_sessions", 0);
  const int max_monthly_sessions = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_monthly_sessions", 0);
  ConstraintInfos result;
  result.emplace_back(std::make_unique<ActiveDaysConstraint>(kShortTermDays),
                      "UserEducation.Session.RecentActiveDays", kShortTermDays,
                      ValueOrNull(max_active_days));
  result.emplace_back(
      std::make_unique<ActiveWeeksConstraint>(kLongTermWeeks, 1),
      "UserEducation.Session.RecentActiveWeeks", kLongTermWeeks,
      ValueOrNull(max_active_weeks));
  result.emplace_back(std::make_unique<ActiveWeeksConstraint>(
                          kLongTermWeeks, super_active_days),
                      "UserEducation.Session.RecentSuperActiveWeeks",
                      kLongTermWeeks, ValueOrNull(max_super_active_weeks));
  result.emplace_back(std::make_unique<SessionCountConstraint>(kShortTermDays),
                      "UserEducation.Session.ShortTermCount",
                      kShortTermDays + 1, ValueOrNull(max_weekly_sessions));
  result.emplace_back(std::make_unique<SessionCountConstraint>(kLongTermDays),
                      "UserEducation.Session.LongTermCount", kMaxRecords,
                      ValueOrNull(max_monthly_sessions));
  return result;
}
