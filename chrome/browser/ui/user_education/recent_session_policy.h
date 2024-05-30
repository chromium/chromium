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
// profile/installation. Default is to follow guidance from UX research, but the
// parameters are adjustable via trade study parameters.
class RecentSessionPolicyImpl : public RecentSessionPolicy {
 public:
  // Represents one element of a logging and/or low-usage-detection policy.
  class Constraint {
   public:
    virtual ~Constraint() = default;

    // Analyzes `recent_sessions` and returns a count.
    // If there is insufficient data to produce a count, returns std::nullopt.
    virtual std::optional<int> GetCount(
        const RecentSessionData& recent_sessions) const = 0;

    // Returns whether recording of metrics and sessions should be skipped
    // because e.g. the metric would have been recorded already during the
    // current calendar day. Default is false.
    virtual bool ShouldSkipRecording(
        const RecentSessionData& recent_sessions) const;
  };

  // Counts the number of sessions in the given number of `days`. Does not
  // care about calendar days; this is just 24-hour periods.
  class SessionCountConstraint : public Constraint {
   public:
    explicit SessionCountConstraint(int days) : days_(days) {}
    ~SessionCountConstraint() override = default;
    std::optional<int> GetCount(
        const RecentSessionData& recent_sessions) const override;

   private:
    const int days_;
  };

  // Represents a constraint that should only be recorded daily.
  class DailyConstraint : public Constraint {
   public:
    bool ShouldSkipRecording(
        const RecentSessionData& recent_sessions) const override;
  };

  // Counts the number of active weeks in the past number of `weeks`. Uses the
  // last seven calendar days (including today).
  class ActiveWeeksConstraint : public DailyConstraint {
   public:
    explicit ActiveWeeksConstraint(int weeks, int active_days)
        : weeks_(weeks), active_days_(active_days) {}
    ~ActiveWeeksConstraint() override = default;
    std::optional<int> GetCount(
        const RecentSessionData& recent_sessions) const override;

   private:
    const int weeks_;
    const int active_days_;
  };

  // Counts the number of active days in the past number of `days`. Uses
  // calendar days, including today.
  class ActiveDaysConstraint : public DailyConstraint {
   public:
    explicit ActiveDaysConstraint(int days) : days_(days) {}
    ~ActiveDaysConstraint() override = default;
    std::optional<int> GetCount(
        const RecentSessionData& recent_sessions) const override;

   private:
    const int days_;
  };

  // Contains data about the various constraints.
  struct ConstraintInfo {
    ConstraintInfo();
    ConstraintInfo(std::unique_ptr<Constraint> constraint,
                   std::string histogram_name,
                   std::optional<int> histogram_max,
                   std::optional<int> low_usage_max);
    ConstraintInfo(ConstraintInfo&&) noexcept;
    ConstraintInfo& operator=(ConstraintInfo&&) noexcept;
    ~ConstraintInfo();

    // The constraint itself.
    std::unique_ptr<Constraint> constraint;

    // The histogram to log, if any.
    std::string histogram_name;

    // The max of the histogram; if zero, a default value is used.
    std::optional<int> histogram_max;

    // The threshold above which the current user is not eligible for low usage
    // promo mode. If not specified, only histograms will be emitted.
    std::optional<int> low_usage_max;
  };
  using ConstraintInfos = std::vector<ConstraintInfo>;

  // Creates a policy with the given constraints.
  explicit RecentSessionPolicyImpl(
      ConstraintInfos constraints = GetDefaultConstraints());
  RecentSessionPolicyImpl(const RecentSessionPolicyImpl&) = delete;
  void operator=(const RecentSessionPolicyImpl&) = delete;
  ~RecentSessionPolicyImpl() override;

  // RecentSessionPolicy:
  void RecordRecentUsageMetrics(
      const RecentSessionData& recent_sessions) override;
  bool ShouldEnableLowUsagePromoMode(
      const RecentSessionData& recent_sessions) const override;

  void set_constraints_for_testing(ConstraintInfos constraints) {
    CHECK(!constraints.empty());
    constraints_ = std::move(constraints);
  }

 private:
  // Gets a set of constraints with parameters read from the feature flag, or
  // sensible defaults.
  static ConstraintInfos GetDefaultConstraints();

  ConstraintInfos constraints_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_RECENT_SESSION_POLICY_H_
