// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/user_classifier.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/time_serialization.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

// The discount rate for computing the discounted-average metrics. Must be
// strictly larger than 0 and strictly smaller than 1!
const double kDiscountRatePerDay = 0.25;
const char kDiscountRatePerDayParam[] = "user_classifier_discount_rate_per_day";

// Never consider any larger interval than this (so that extreme situations such
// as losing your phone or going for a long offline vacation do not skew the
// average too much).
// When everriding via variation parameters, it is better to use smaller values
// than |kMaxHours| as this it the maximum value reported in the histograms.
const double kMaxHours = 7 * 24;
const char kMaxHoursParam[] = "user_classifier_max_hours";

// Ignore events within |kMinHours| hours since the last event (|kMinHours| is
// the length of the browsing session where subsequent events of the same type
// do not count again).
const double kMinHours = 0.5;
const char kMinHoursParam[] = "user_classifier_min_hours";

// Classification constants.
const double kActiveConsumerClicksAtLeastOncePerHours = 96;
const char kActiveConsumerClicksAtLeastOncePerHoursParam[] =
    "user_classifier_active_consumer_clicks_at_least_once_per_hours";

// The previous value in production was 66, i.e. 2.75 days. The new value is a
// shift in the direction we want (having more active users).
const double kRareUserOpensNTPAtMostOncePerHours = 96;
const char kRareUserOpensNTPAtMostOncePerHoursParam[] =
    "user_classifier_rare_user_opens_ntp_at_most_once_per_hours";

// Histograms for logging the estimated average hours to next event.
const char kHistogramAverageHoursToOpenNTP[] =
    "NewTabPage.UserClassifier.AverageHoursToOpenNTP";
const char kHistogramAverageHoursToShowSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToShowSuggestions";
const char kHistogramAverageHoursToUseSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToUseSuggestions";

// The enum used for iteration.
const UserClassifier::Metric kMetrics[] = {
    UserClassifier::Metric::NTP_OPENED,
    UserClassifier::Metric::SUGGESTIONS_SHOWN,
    UserClassifier::Metric::SUGGESTIONS_USED};

// The summary of the prefs.
const char* kMetricKeys[] = {
    prefs::kUserClassifierAverageNTPOpenedPerHour,
    prefs::kUserClassifierAverageSuggestionsShownPerHour,
    prefs::kUserClassifierAverageSuggestionsUsedPerHour};
const char* kLastTimeKeys[] = {prefs::kUserClassifierLastTimeToOpenNTP,
                               prefs::kUserClassifierLastTimeToShowSuggestions,
                               prefs::kUserClassifierLastTimeToUseSuggestions};

// Default lengths of the intervals for new users for the metrics.
const double kInitialHoursBetweenEvents[] = {24, 48, 120};
const char* kInitialHoursBetweenEventsParams[] = {
    "user_classifier_default_interval_ntp_opened",
    "user_classifier_default_interval_suggestions_shown",
    "user_classifier_default_interval_suggestions_used"};

static_assert(base::size(kMetrics) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  base::size(kMetricKeys) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  base::size(kLastTimeKeys) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  base::size(kInitialHoursBetweenEvents) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  base::size(kInitialHoursBetweenEventsParams) ==
                      static_cast<int>(UserClassifier::Metric::COUNT),
              "Fill in info for all metrics.");

// Computes the discount rate.
double GetDiscountRatePerHour() {
  double discount_rate_per_day = variations::GetVariationParamByFeatureAsDouble(
      kArticleSuggestionsFeature, kDiscountRatePerDayParam,
      kDiscountRatePerDay);
  // Check for illegal values.
  if (discount_rate_per_day <= 0 || discount_rate_per_day >= 1) {
    DLOG(WARNING) << "Illegal value " << discount_rate_per_day
                  << " for the parameter " << kDiscountRatePerDayParam
                  << " (must be strictly between 0 and 1; the default "
                  << kDiscountRatePerDay << " is used, instead).";
    discount_rate_per_day = kDiscountRatePerDay;
  }
  // Compute discount_rate_per_hour such that
  //   discount_rate_per_day = 1 - e^{-discount_rate_per_hour * 24}.
  return std::log(1.0 / (1.0 - discount_rate_per_day)) / 24.0;
}

double GetInitialHoursBetweenEvents(UserClassifier::Metric metric) {
  return variations::GetVariationParamByFeatureAsDouble(
      kArticleSuggestionsFeature,
      kInitialHoursBetweenEventsParams[static_cast<int>(metric)],
      kInitialHoursBetweenEvents[static_cast<int>(metric)]);
}

double GetMinHours() {
  return variations::GetVariationParamByFeatureAsDouble(
      kArticleSuggestionsFeature, kMinHoursParam, kMinHours);
}

double GetMaxHours() {
  return variations::GetVariationParamByFeatureAsDouble(
      kArticleSuggestionsFeature, kMaxHoursParam, kMaxHours);
}

// Returns the new value of the metric using its |old_value|, assuming
// |hours_since_last_time| hours have passed since it was last discounted.
double DiscountMetric(double old_value,
                      double hours_since_last_time,
                      double discount_rate_per_hour) {
  // Compute the new discounted average according to the formula
  //   avg_events := e^{-discount_rate_per_hour * hours_since} * avg_events
  return std::exp(-discount_rate_per_hour * hours_since_last_time) * old_value;
}

// Compute the number of hours between two events for the given metric value
// assuming the events were equally distributed.
double GetEstimateHoursBetweenEvents(double metric_value,
                                     double discount_rate_per_hour,
                                     double min_hours,
                                     double max_hours) {
  // The computation below is well-defined only for |metric_value| > 1 (log of
  // negative value or division by zero). When |metric_value| -> 1, the estimate
  // below -> infinity, so max_hours is a natural result, here.
  if (metric_value <= 1) {
    return max_hours;
  }

  // This is the estimate with the assumption that last event happened right
  // now and the system is in the steady-state. Solve estimate_hours in the
  // steady-state equation:
  //   metric_value = 1 + e^{-discount_rate * estimate_hours} * metric_value,
  // i.e.
  //   -discount_rate * estimate_hours = log((metric_value - 1) / metric_value),
  //   discount_rate * estimate_hours = log(metric_value / (metric_value - 1)),
  //   estimate_hours = log(metric_value / (metric_value - 1)) / discount_rate.
  double estimate_hours =
      std::log(metric_value / (metric_value - 1)) / discount_rate_per_hour;
  return base::ClampToRange(estimate_hours, min_hours, max_hours);
}

// The inverse of GetEstimateHoursBetweenEvents().
double GetMetricValueForEstimateHoursBetweenEvents(
    double estimate_hours,
    double discount_rate_per_hour,
    double min_hours,
    double max_hours) {
  estimate_hours = base::ClampToRange(estimate_hours, min_hours, max_hours);
  // Return |metric_value| such that GetEstimateHoursBetweenEvents for
  // |metric_value| returns |estimate_hours|. Thus, solve |metric_value| in
  //   metric_value = 1 + e^{-discount_rate * estimate_hours} * metric_value,
  // i.e.
  //   metric_value * (1 - e^{-discount_rate * estimate_hours}) = 1,
  //   metric_value = 1 / (1 - e^{-discount_rate * estimate_hours}).
  return 1.0 / (1.0 - std::exp(-discount_rate_per_hour * estimate_hours));
}

}  // namespace

UserClassifier::UserClassifier(PrefService* pref_service, base::Clock* clock)
    : pref_service_(pref_service),
      clock_(clock),
      discount_rate_per_hour_(GetDiscountRatePerHour()),
      min_hours_(GetMinHours()),
      max_hours_(GetMaxHours()),
      active_consumer_clicks_at_least_once_per_hours_(
          variations::GetVariationParamByFeatureAsDouble(
              kArticleSuggestionsFeature,
              kActiveConsumerClicksAtLeastOncePerHoursParam,
              kActiveConsumerClicksAtLeastOncePerHours)),
      rare_user_opens_ntp_at_most_once_per_hours_(
          variations::GetVariationParamByFeatureAsDouble(
              kArticleSuggestionsFeature,
              kRareUserOpensNTPAtMostOncePerHoursParam,
              kRareUserOpensNTPAtMostOncePerHours)) {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return;
  }

  // TODO(jkrcal): Store the current discount rate per hour into prefs. If it
  // differs from the previous value, rescale the metric values so that the
  // expectation does not change abruptly!

  // Initialize the prefs storing the last time: the counter has just started!
  for (const Metric metric : kMetrics) {
    if (!HasLastTime(metric)) {
      SetLastTimeToNow(metric);
    }
  }
}

UserClassifier::~UserClassifier() = default;

// static
void UserClassifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  double discount_rate = GetDiscountRatePerHour();
  double min_hours = GetMinHours();
  double max_hours = GetMaxHours();

  for (Metric metric : kMetrics) {
    double default_metric_value = GetMetricValueForEstimateHoursBetweenEvents(
        GetInitialHoursBetweenEvents(metric), discount_rate, min_hours,
        max_hours);
    registry->RegisterDoublePref(kMetricKeys[static_cast<int>(metric)],
                                 default_metric_value);
    registry->RegisterInt64Pref(kLastTimeKeys[static_cast<int>(metric)], 0);
  }
}

void UserClassifier::OnEvent(Metric metric) {
  DCHECK_NE(metric, Metric::COUNT);
  double metric_value = UpdateMetricOnEvent(metric);

  double avg = GetEstimateHoursBetweenEvents(
      metric_value, discount_rate_per_hour_, min_hours_, max_hours_);
  // We use kMaxHours as the max value below as the maximum value for the
  // histograms must be constant.
  switch (metric) {
    case Metric::NTP_OPENED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToOpenNTP, avg, 1,
                                  kMaxHours, 50);
      break;
    case Metric::SUGGESTIONS_SHOWN:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToShowSuggestions, avg,
                                  1, kMaxHours, 50);
      break;
    case Metric::SUGGESTIONS_USED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToUseSuggestions, avg,
                                  1, kMaxHours, 50);
      break;
    case Metric::COUNT:
      NOTREACHED();
      break;
  }
}

double UserClassifier::GetEstimatedAvgTime(Metric metric) const {
  DCHECK_NE(metric, Metric::COUNT);
  double metric_value = GetUpToDateMetricValue(metric);
  return GetEstimateHoursBetweenEvents(metric_value, discount_rate_per_hour_,
                                       min_hours_, max_hours_);
}

UserClassifier::UserClass UserClassifier::GetUserClass() const {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return UserClass::ACTIVE_NTP_USER;
  }

  if (GetEstimatedAvgTime(Metric::NTP_OPENED) >=
      rare_user_opens_ntp_at_most_once_per_hours_) {
    return UserClass::RARE_NTP_USER;
  }

  if (GetEstimatedAvgTime(Metric::SUGGESTIONS_USED) <=
      active_consumer_clicks_at_least_once_per_hours_) {
    return UserClass::ACTIVE_SUGGESTIONS_CONSUMER;
  }

  return UserClass::ACTIVE_NTP_USER;
}

std::string UserClassifier::GetUserClassDescriptionForDebugging() const {
  switch (GetUserClass()) {
    case UserClass::RARE_NTP_USER:
      return "Rare user of the NTP";
    case UserClass::ACTIVE_NTP_USER:
      return "Active user of the NTP";
    case UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return "Active consumer of NTP suggestions";
  }
  NOTREACHED();
  return std::string();
}

void UserClassifier::ClearClassificationForDebugging() {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return;
  }

  for (const Metric& metric : kMetrics) {
    ClearMetricValue(metric);
    SetLastTimeToNow(metric);
  }
}

double UserClassifier::UpdateMetricOnEvent(Metric metric) {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return 0;
  }

  double hours_since_last_time =
      std::min(max_hours_, GetHoursSinceLastTime(metric));
  // Ignore events within the same "browsing session".
  if (hours_since_last_time < min_hours_) {
    return GetUpToDateMetricValue(metric);
  }

  SetLastTimeToNow(metric);

  double metric_value = GetMetricValue(metric);
  // Add 1 to the discounted metric as the event has happened right now.
  double new_metric_value =
      1 + DiscountMetric(metric_value, hours_since_last_time,
                         discount_rate_per_hour_);
  SetMetricValue(metric, new_metric_value);
  return new_metric_value;
}

double UserClassifier::GetUpToDateMetricValue(Metric metric) const {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return 0;
  }

  double hours_since_last_time =
      std::min(max_hours_, GetHoursSinceLastTime(metric));

  double metric_value = GetMetricValue(metric);
  return DiscountMetric(metric_value, hours_since_last_time,
                        discount_rate_per_hour_);
}

double UserClassifier::GetHoursSinceLastTime(Metric metric) const {
  if (!HasLastTime(metric)) {
    return 0;
  }

  base::TimeDelta since_last_time =
      clock_->Now() - DeserializeTime(pref_service_->GetInt64(
                          kLastTimeKeys[static_cast<int>(metric)]));
  return since_last_time.InSecondsF() / 3600;
}

bool UserClassifier::HasLastTime(Metric metric) const {
  return pref_service_->HasPrefPath(kLastTimeKeys[static_cast<int>(metric)]);
}

void UserClassifier::SetLastTimeToNow(Metric metric) {
  pref_service_->SetInt64(kLastTimeKeys[static_cast<int>(metric)],
                          SerializeTime(clock_->Now()));
}

double UserClassifier::GetMetricValue(Metric metric) const {
  return pref_service_->GetDouble(kMetricKeys[static_cast<int>(metric)]);
}

void UserClassifier::SetMetricValue(Metric metric, double metric_value) {
  pref_service_->SetDouble(kMetricKeys[static_cast<int>(metric)], metric_value);
}

void UserClassifier::ClearMetricValue(Metric metric) {
  pref_service_->ClearPref(kMetricKeys[static_cast<int>(metric)]);
}

}  // namespace ntp_snippets
