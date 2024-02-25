// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FEATURE_USAGE_FEATURE_USAGE_METRICS_H_
#define CHROMEOS_ASH_COMPONENTS_FEATURE_USAGE_FEATURE_USAGE_METRICS_H_

#include <memory>
#include <optional>

#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace ash::feature_usage {

// Helper class to unify tracking features usage by users.
// It provides unified naming for the tracked events. Which reduces effort on
// the data analytics side to incorporate a new feature.
// This class also provides a way to report periodically if the device is
// eligible for the feature and whether user has it enabled.
class FeatureUsageMetrics final : public base::PowerSuspendObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Event {
    kEligible = 0,
    kEnabled = 1,
    kUsedWithSuccess = 2,
    kUsedWithFailure = 3,
    kAccessible = 4,
    kMaxValue = kAccessible,
  };

  // Consumers should implement this interface to report periodically if the
  // feature is eligible on the device and enabled.
  class Delegate {
   public:
    // Whether the device is capable of running the feature.
    virtual bool IsEligible() const = 0;

    // Whether the feature is accessible to users (e.g. allowed by admins).
    virtual std::optional<bool> IsAccessible() const;

    // Whether the user has enabled the feature for themselves. If `IsEnabled`
    // returns true `IsEligible` must return true too.
    virtual bool IsEnabled() const = 0;

    virtual ~Delegate() = default;
  };

  static const base::TimeDelta kInitialInterval;
  static const base::TimeDelta kRepeatedInterval;

  // `feature_name` must correspond to the entry of `FeaturesLoggingUsageEvents`
  // and should never change.
  FeatureUsageMetrics(const std::string& feature_name,
                      Delegate* delegate);
  // Custom `tick_clock` could be passed for testing purposes. Must not be
  // nullptr.
  FeatureUsageMetrics(const std::string& feature_name,
                      Delegate* delegate,
                      const base::Clock* clock,
                      const base::TickClock* tick_clock);
  FeatureUsageMetrics(const FeatureUsageMetrics&) = delete;
  FeatureUsageMetrics& operator=(const FeatureUsageMetrics&) = delete;
  ~FeatureUsageMetrics() final;

  // `RecordUsage` should be called on every usage of the feature. `success`
  // indicates whether the usage was successful. For example if user touches the
  // fingerprint sensor and the finger was not recognized `RecordUsage` should
  // be called with `false`.
  void RecordUsage(bool success);

  // Use `StartSuccessfulUsage` and `StopSuccessfulUsage` to record feature
  // usage time. See ./README.md#Recording-usage-time for more details.
  void StartSuccessfulUsage();
  void StopSuccessfulUsage();

 private:
  void OnSuspend() final;
  void OnResume() final;

  void SetupTimer(base::TimeDelta delta);
  void MaybeReportPeriodicMetrics();
  void ReportPeriodicMetrics();
  void MaybeReportUseTime();

  void RecordUsetime(base::TimeDelta usetime) const;

  base::Time Now() const;

  const std::string histogram_name_;
  const raw_ptr<const Delegate> delegate_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::Time last_time_enabled_reported_;

  const raw_ptr<const base::Clock> clock_;
  base::Time start_usage_;
  base::OneShotTimer timer_;

#if DCHECK_IS_ON()
  std::optional<bool> last_record_usage_outcome_;
#endif
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash::feature_usage

#endif  // CHROMEOS_ASH_COMPONENTS_FEATURE_USAGE_FEATURE_USAGE_METRICS_H_
