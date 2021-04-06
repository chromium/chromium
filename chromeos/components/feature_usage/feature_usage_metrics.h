// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FEATURE_USAGE_FEATURE_USAGE_METRICS_H_
#define CHROMEOS_COMPONENTS_FEATURE_USAGE_FEATURE_USAGE_METRICS_H_

#include <memory>

#include "base/observer_list.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {
class DailyEvent;
}

namespace feature_usage {

// Helper class to unify tracking features usage by users.
// It provides unified naming for the tracked events. Which reduces effort on
// the data analytics side to incorporate a new feature.
// This class also provides a way to report daily if the device is eligible for
// the feature and whether user has it enabled.
class FeatureUsageMetrics {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Event {
    kEligible = 0,
    kEnabled = 1,
    kUsedWithSuccess = 2,
    kUsedWithFailure = 3,
    kMaxValue = kUsedWithFailure,
  };

  // Consumers should implement this interface to report daily if the feature
  // is eligible on the device and enabled.
  class Delegate {
   public:
    // Whether the device is capable of running the feature.
    virtual bool IsEligible() const = 0;

    // Whether the user has enabled the feature for themselves. If `IsEnabled`
    // returns true `IsEligible` must return true too.
    virtual bool IsEnabled() const = 0;

    virtual ~Delegate() = default;
  };

  // `feature_name` and `pref_service` must correspond to the `RegisterPref`
  // call.
  FeatureUsageMetrics(const std::string& feature_name,
                      PrefService* pref_service,
                      Delegate* delegate);
  FeatureUsageMetrics(const std::string& feature_name,
                      PrefService* pref_service,
                      Delegate* delegate,
                      const base::TickClock* tick_clock);
  FeatureUsageMetrics(const FeatureUsageMetrics&) = delete;
  FeatureUsageMetrics& operator=(const FeatureUsageMetrics&) = delete;
  ~FeatureUsageMetrics();

  // `RecordUsage` should be called on every usage of the feature. `success`
  // indicates whether the usage was successful. For example if user touches the
  // fingerprint sensor and the finger was not recognized `RecordUsage` should
  // be called with `false`.
  void RecordUsage(bool success) const;

  static void RegisterPref(PrefRegistrySimple* registry,
                           const std::string& feature_name);

 private:
  void ReportDailyMetrics() const;

  const std::string histogram_name_;
  const std::string pref_name_;
  const Delegate* const delegate_;
  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  std::unique_ptr<base::RepeatingTimer> timer_;
};

}  // namespace feature_usage

#endif  // CHROMEOS_COMPONENTS_FEATURE_USAGE_FEATURE_USAGE_METRICS_H_
