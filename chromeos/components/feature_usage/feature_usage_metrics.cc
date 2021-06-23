// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/feature_usage/feature_usage_metrics.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace feature_usage {

namespace {

constexpr char kFeatureUsageMetricPrefix[] = "ChromeOS.FeatureUsage.";
constexpr char kFeatureUsetimeMetricPostfix[] = ".Usetime";

std::string FeatureToHistogram(const std::string& feature_name) {
  return kFeatureUsageMetricPrefix + feature_name;
}

}  // namespace

// First time periodic metrics are reported after 'kInitialInterval` time.
constexpr base::TimeDelta FeatureUsageMetrics::kInitialInterval =
    base::TimeDelta::FromMinutes(1);

// Consecutive reports run every `kRepeatedInterval`
constexpr base::TimeDelta FeatureUsageMetrics::kRepeatedInterval =
    base::TimeDelta::FromMinutes(30);

FeatureUsageMetrics::FeatureUsageMetrics(const std::string& feature_name,
                                         Delegate* const delegate)
    : FeatureUsageMetrics(feature_name,
                          delegate,
                          base::DefaultTickClock::GetInstance()) {}

FeatureUsageMetrics::FeatureUsageMetrics(const std::string& feature_name,
                                         Delegate* const delegate,
                                         const base::TickClock* tick_clock)
    : histogram_name_(FeatureToHistogram(feature_name)),
      delegate_(delegate),
      tick_clock_(tick_clock),
      timer_(std::make_unique<base::OneShotTimer>(tick_clock)) {
  DCHECK(delegate_);

  // Schedule the first run some time in the future to not overload startup
  // flow.
  SetupTimer(kInitialInterval);
}

void FeatureUsageMetrics::SetupTimer(base::TimeDelta delta) {
  timer_->Start(FROM_HERE, delta,
                base::BindOnce(&FeatureUsageMetrics::MaybeReportPeriodicMetrics,
                               base::Unretained(this)));
}

FeatureUsageMetrics::~FeatureUsageMetrics() {
  if (!start_usage_.is_null())
    StopSuccessfulUsage();
}

void FeatureUsageMetrics::RecordUsage(bool success) {
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
  DCHECK(start_usage_.is_null());
#if DCHECK_IS_ON()
  DCHECK(last_record_usage_outcome_.value_or(false))
      << "Start usage must be preceded by RecordUsage(true)";
  last_record_usage_outcome_.reset();
#endif
  start_usage_ = tick_clock_->NowTicks();
}

void FeatureUsageMetrics::StopSuccessfulUsage() {
  DCHECK(!start_usage_.is_null());
#if DCHECK_IS_ON()
  DCHECK(!last_record_usage_outcome_.has_value())
      << "There must be no RecordUsage calls between Start and "
         "StopSuccessfulUsage";
#endif
  const base::TimeDelta use_time = tick_clock_->NowTicks() - start_usage_;
  RecordUsetime(use_time);
  start_usage_ = base::TimeTicks();
}

void FeatureUsageMetrics::MaybeReportPeriodicMetrics() {
  if (!last_time_enabled_reported_.is_null()) {
    const base::TimeDelta time_left =
        kRepeatedInterval -
        (tick_clock_->NowTicks() - last_time_enabled_reported_);
    // Do not report periodic metrics more often than once per
    // `kRepeatedInterval`.
    if (time_left > base::TimeDelta()) {
      // This could only happen when `RecordUsage` is called. In that case
      // `IsEnabled` must be true. And because `last_time_enabled_reported_` is
      // not null - `IsEnabled` was already reported recently.
      SetupTimer(time_left);
      return;
    }
  }

  bool is_eligible = delegate_->IsEligible();
  bool is_enabled = delegate_->IsEnabled();
  DCHECK(!is_enabled || is_eligible);

  if (is_eligible)
    base::UmaHistogramEnumeration(histogram_name_, Event::kEligible);

  if (is_enabled) {
    last_time_enabled_reported_ = tick_clock_->NowTicks();
    base::UmaHistogramEnumeration(histogram_name_, Event::kEnabled);
  }

  if (!start_usage_.is_null()) {
    DCHECK(is_eligible);
    DCHECK(is_enabled);
    base::TimeDelta use_time = tick_clock_->NowTicks() - start_usage_;
    RecordUsetime(use_time);
    start_usage_ = tick_clock_->NowTicks();
  }

  SetupTimer(kRepeatedInterval);
}

}  // namespace feature_usage
