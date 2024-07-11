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

// Gets midnight at the start of the next local day.
// Note: inaccurate if the time is *exactly* midnight, but this will happen so
// rarely that it's not worth worrying about.
base::Time GetEndOfDay(base::Time time) {
  return (time + base::Days(1)).LocalMidnight();
}

// Counts the number of active days in `recent_sessions` going back `num_days`
// from `last_day`. Returns null if session data recording does not go back far
// enough to cover the whole span.
std::optional<int> CountActiveDays(const RecentSessionData& recent_sessions,
                                   base::Time last_day,
                                   int num_days) {
  const base::Time end = GetEndOfDay(last_day);
  const base::Time start = end - base::Days(num_days);

  if (recent_sessions.enabled_time > start) {
    return std::nullopt;
  }

  std::vector<bool> active_days(num_days, false);
  for (const auto& start_time : recent_sessions.recent_session_start_times) {
    if (start_time >= start && start_time < end) {
      const size_t index = (start_time - start) / base::Days(1);
      active_days[index] = true;
    }
  }
  return std::count(active_days.begin(), active_days.end(), true);
}

std::optional<int> ValueOrNull(int value) {
  return value ? std::make_optional(value) : std::nullopt;
}

}  // namespace

bool RecentSessionPolicyImpl::Constraint::ShouldSkipRecording(
    const RecentSessionData& recent_sessions) const {
  return false;
}

bool RecentSessionPolicyImpl::DailyConstraint::ShouldSkipRecording(
    const RecentSessionData& recent_sessions) const {
  // Do not record if there are at least two recent sessions and the most recent
  // session is on the same calendar day as the second-most-recent session; the
  // session would have already been recorded on this day.
  //
  // It is critical that calendar day is used rather than just a 24-hour period,
  // since if the test were simply less than 24 hours, there could be a sequence
  // of, say, 16-hour separations between sessions and only the first one would
  // be recorded, no matter how long the sequence lasted.
  return recent_sessions.recent_session_start_times.size() > 1U &&
         GetEndOfDay(recent_sessions.recent_session_start_times[0]) ==
             GetEndOfDay(recent_sessions.recent_session_start_times[1]);
}

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
  return CountActiveDays(recent_sessions,
                         recent_sessions.recent_session_start_times.front(),
                         days_);
}

std::optional<int> RecentSessionPolicyImpl::ActiveWeeksConstraint::GetCount(
    const RecentSessionData& recent_sessions) const {
  int count = 0;
  base::Time counting_back_from =
      recent_sessions.recent_session_start_times.front();
  for (int week = 0; week < weeks_; ++week) {
    const auto active_days =
        CountActiveDays(recent_sessions, counting_back_from, 7);
    if (!active_days) {
      return active_days;
    } else if (*active_days >= active_days_) {
      ++count;
    }
    counting_back_from -= base::Days(7);
  }
  return count;
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
    if (!constraint.histogram_name.empty() &&
        !constraint.constraint->ShouldSkipRecording(recent_sessions)) {
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
      kAllowRecentSessionTracking, "max_active_weeks", 0);
  const int max_active_days = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_active_days", 0);
  const int super_active_days = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "super_active_days", 4);
  const int max_monthly_active_days = base::GetFieldTrialParamByFeatureAsInt(
      kAllowRecentSessionTracking, "max_monthly_active_days", 2);
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
  result.emplace_back(std::make_unique<ActiveDaysConstraint>(kLongTermDays),
                      "UserEducation.Session.MonthlyActiveDays", kLongTermDays,
                      ValueOrNull(max_monthly_active_days));
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
