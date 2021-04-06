// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/feature_usage/feature_usage_metrics.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/metrics/daily_event.h"

namespace feature_usage {

namespace {

// Interval for asking metrics::DailyEvent to check whether a day has passed.
constexpr base::TimeDelta kCheckDailyEventInternal =
    base::TimeDelta::FromMinutes(30);

constexpr char kDailySamplePrefPrefix[] = "feature_usage.daily_sample.";
constexpr char kFeatureUsageMetricPrefix[] = "ChromeOS.FeatureUsage.";

std::string FeatureToPref(const std::string& feature_name) {
  return kDailySamplePrefPrefix + feature_name;
}

std::string FeatureToHistogram(const std::string& feature_name) {
  return kFeatureUsageMetricPrefix + feature_name;
}

// `DailyEventObserver` implements `metrics::DailyEvent::Observer`. It runs
// `callback_` on the first usage and when the day has passed.
class DailyEventObserver : public metrics::DailyEvent::Observer {
 public:
  explicit DailyEventObserver(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
  }

  DailyEventObserver(const DailyEventObserver&) = delete;
  DailyEventObserver& operator=(const DailyEventObserver&) = delete;
  ~DailyEventObserver() override = default;

  // metrics::DailyEvent::Observer:
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) final {
    if (type == metrics::DailyEvent::IntervalType::DAY_ELAPSED ||
        type == metrics::DailyEvent::IntervalType::FIRST_RUN) {
      callback_.Run();
    }
  }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

FeatureUsageMetrics::FeatureUsageMetrics(const std::string& feature_name,
                                         PrefService* pref_service,
                                         Delegate* const delegate)
    : FeatureUsageMetrics(feature_name, pref_service, delegate, nullptr) {}

FeatureUsageMetrics::FeatureUsageMetrics(const std::string& feature_name,
                                         PrefService* pref_service,
                                         Delegate* const delegate,
                                         const base::TickClock* tick_clock)
    : histogram_name_(FeatureToHistogram(feature_name)),
      pref_name_(FeatureToPref(feature_name)),
      delegate_(delegate),
      timer_(tick_clock ? std::make_unique<base::RepeatingTimer>(tick_clock)
                        : std::make_unique<base::RepeatingTimer>()) {
  DCHECK(delegate_);

  daily_event_ = std::make_unique<metrics::DailyEvent>(
      pref_service, pref_name_.c_str(), /*histogram_name=*/std::string());

  auto observer = std::make_unique<DailyEventObserver>(base::BindRepeating(
      // base::Unretained is safe because `this` owns `daily_event_` which owns
      // the `observer`.
      &FeatureUsageMetrics::ReportDailyMetrics, base::Unretained(this)));

  daily_event_->AddObserver(std::move(observer));
  daily_event_->CheckInterval();
  timer_->Start(FROM_HERE, kCheckDailyEventInternal, daily_event_.get(),
                &metrics::DailyEvent::CheckInterval);
}

FeatureUsageMetrics::~FeatureUsageMetrics() = default;

// static
void FeatureUsageMetrics::RegisterPref(PrefRegistrySimple* registry,
                                       const std::string& feature_name) {
  metrics::DailyEvent::RegisterPref(registry, FeatureToPref(feature_name));
}

void FeatureUsageMetrics::RecordUsage(bool success) const {
  Event e = success ? Event::kUsedWithSuccess : Event::kUsedWithFailure;
  base::UmaHistogramEnumeration(histogram_name_, e);
}

void FeatureUsageMetrics::ReportDailyMetrics() const {
  if (delegate_->IsEligible())
    base::UmaHistogramEnumeration(histogram_name_, Event::kEligible);
  if (delegate_->IsEnabled()) {
    DCHECK(delegate_->IsEligible());
    base::UmaHistogramEnumeration(histogram_name_, Event::kEnabled);
  }
}

}  // namespace feature_usage
