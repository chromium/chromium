// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash::feature_usage {

namespace {

constexpr char kFeatureUsageMetricPrefix[] = "ChromeOS.FeatureUsage.";
constexpr char kFeatureUsetimeMetricPostfix[] = ".Usetime";

std::string FeatureToHistogram(const std::string& feature_name) {
  return kFeatureUsageMetricPrefix + feature_name;
}

}  // namespace

std::optional<bool> FeatureUsageMetrics::Delegate::IsAccessible() const {
  return std::nullopt;
}

// First time periodic metrics are reported after 'kInitialInterval` time.
constexpr base::TimeDelta FeatureUsageMetrics::kInitialInterval =
    base::Minutes(1);

// Consecutive reports run every `kRepeatedInterval`
constexpr base::TimeDelta FeatureUsageMetrics::kRepeatedInterval =
    base::Minutes(30);

FeatureUsageMetrics::FeatureUsageMetrics(const std::string& feature_name,
                                         Delegate* const delegate)
    : FeatureUsageMetrics(feature_name,
                          delegate,
                          base::DefaultClock::GetInstance(),
                          base::DefaultTickClock::GetInstance()) {}

FeatureUsageMetrics::FeatureUsageMetrics(const std::string& feature_name,
                                         Delegate* const delegate,
                                         const base::Clock* clock,
                                         const base::TickClock* tick_clock)
    : histogram_name_(FeatureToHistogram(feature_name)),
      delegate_(delegate),
      clock_(clock),
      timer_(tick_clock) {
  DCHECK(delegate_);

  // Schedule the first run some time in the future to not overload startup
  // flow.
  SetupTimer(kInitialInterval);
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
}

void FeatureUsageMetrics::SetupTimer(base::TimeDelta delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(FROM_HERE, delta,
               base::BindOnce(&FeatureUsageMetrics::MaybeReportPeriodicMetrics,
                              base::Unretained(this)));
}

FeatureUsageMetrics::~FeatureUsageMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  MaybeReportUseTime();
}

void FeatureUsageMetrics::RecordUsage(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_->IsEligible());
  DCHECK(delegate_->IsEnabled());
  DCHECK(start_usage_.is_null());
#if DCHECK_IS_ON()
  last_record_usage_outcome_ = success;
#endif
  MaybeReportPeriodicMetrics();

  Event e = success ? Event::kUsedWithSuccess : Event::kUsedWithFailure;
  base::UmaHistogramEnumeration(histogram_name_, e);
}

void FeatureUsageMetrics::RecordUsetime(base::TimeDelta usetime) const {
  base::UmaHistogramLongTimes100(histogram_name_ + kFeatureUsetimeMetricPostfix,
                                 usetime);
}

void FeatureUsageMetrics::StartSuccessfulUsage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(start_usage_.is_null());
#if DCHECK_IS_ON()
  DCHECK(last_record_usage_outcome_.value_or(false))
      << "Start usage must be preceded by RecordUsage(true)";
  last_record_usage_outcome_.reset();
#endif
  start_usage_ = Now();
}

void FeatureUsageMetrics::StopSuccessfulUsage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!start_usage_.is_null());
#if DCHECK_IS_ON()
  DCHECK(!last_record_usage_outcome_.has_value())
      << "There must be no RecordUsage calls between Start and "
         "StopSuccessfulUsage";
#endif
  MaybeReportUseTime();
  start_usage_ = base::Time();
}

void FeatureUsageMetrics::OnSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.AbandonAndStop();
  ReportPeriodicMetrics();
}

void FeatureUsageMetrics::OnResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!start_usage_.is_null()) {
    // Do not report usage during suspension.
    start_usage_ = Now();
  }
  SetupTimer(kInitialInterval);
}

void FeatureUsageMetrics::MaybeReportPeriodicMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!last_time_enabled_reported_.is_null()) {
    const base::TimeDelta delta_since_enabled_reported =
        Now() - last_time_enabled_reported_;
    // Do not report periodic metrics more often than once per
    // `kRepeatedInterval`.
    if (kRepeatedInterval > delta_since_enabled_reported) {
      // This could only happen when `RecordUsage` is called. In that case
      // `IsEnabled` must be true. And because `last_time_enabled_reported_` is
      // not null - `IsEnabled` was already reported recently.
      SetupTimer(kRepeatedInterval - delta_since_enabled_reported);
      return;
    }
  }
  ReportPeriodicMetrics();
}

void FeatureUsageMetrics::ReportPeriodicMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool is_eligible = delegate_->IsEligible();
  bool is_enabled = delegate_->IsEnabled();
  DCHECK(!is_enabled || is_eligible);

  if (is_eligible)
    base::UmaHistogramEnumeration(histogram_name_, Event::kEligible);

  std::optional<bool> is_accessible = delegate_->IsAccessible();

  if (is_accessible.has_value()) {
    // If accessible must be eligible.
    DCHECK(!is_accessible.value() || is_eligible);

    // If enabled must be accessible
    DCHECK(!is_enabled || is_accessible.value());

    if (is_accessible.value())
      base::UmaHistogramEnumeration(histogram_name_, Event::kAccessible);
  }

  if (is_enabled) {
    last_time_enabled_reported_ = Now();
    base::UmaHistogramEnumeration(histogram_name_, Event::kEnabled);
  }

  // If the feature in the active use - it must be enabled.
  DCHECK(start_usage_.is_null() || is_enabled);
  MaybeReportUseTime();

  SetupTimer(kRepeatedInterval);
}

void FeatureUsageMetrics::MaybeReportUseTime() {
  if (start_usage_.is_null())
    return;
  base::TimeDelta use_time = Now() - start_usage_;
  RecordUsetime(use_time);
  start_usage_ = Now();
}

base::Time FeatureUsageMetrics::Now() const {
  return clock_->Now();
}

}  // namespace ash::feature_usage
