// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/user_classifier.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace feed {

namespace {

// The discount rate for computing the discounted-average rates. Must be
// strictly larger than 0 and strictly smaller than 1!
const double kDiscountRatePerDay = 0.25;
const char kDiscountRatePerDayParam[] = "user_classifier_discount_rate_per_day";

// Never consider any larger interval than this (so that extreme situations such
// as losing your phone or going for a long offline vacation do not skew the
// average too much).
// When overriding via variation parameters, it is better to use smaller values
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
const double kRareUserViewsAtMostOncePerHours = 96;
const char kRareUserViewsAtMostOncePerHoursParam[] =
    "user_classifier_rare_user_views_at_most_once_per_hours";

// Histograms for logging the estimated average hours to next event. During
// launch these must match legacy histogram names.
const char kHistogramAverageHoursToOpenNTP[] =
    "NewTabPage.UserClassifier.AverageHoursToOpenNTP";
const char kHistogramAverageHoursToUseSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToUseSuggestions";

// List of all Events used for iteration.
const UserClassifier::Event kEvents[] = {
    UserClassifier::Event::kSuggestionsViewed,
    UserClassifier::Event::kSuggestionsUsed};

// Arrays of pref names, indexed by Event's int value.
const char* kRateKeys[] = {prefs::kUserClassifierAverageSuggestionsViwedPerHour,
                           prefs::kUserClassifierAverageSuggestionsUsedPerHour};
const char* kLastTimeKeys[] = {prefs::kUserClassifierLastTimeToViewSuggestions,
                               prefs::kUserClassifierLastTimeToUseSuggestions};

// Default lengths of the intervals for new users for the events.
const double kInitialHoursBetweenEvents[] = {24, 120};
const char* kInitialHoursBetweenEventsParams[] = {
    "user_classifier_default_interval_suggestions_viewed",
    "user_classifier_default_interval_suggestions_used"};

// This verifies that each of the arrays has exactly the same number of values
// as the number of enum values in UserClassifier::Event. These arrays are all
// indexed by the integer value of UserClassifier::Event values.
static_assert(base::size(kEvents) ==
                      static_cast<int>(UserClassifier::Event::kMaxValue) + 1 &&
                  base::size(kRateKeys) ==
                      static_cast<int>(UserClassifier::Event::kMaxValue) + 1 &&
                  base::size(kLastTimeKeys) ==
                      static_cast<int>(UserClassifier::Event::kMaxValue) + 1 &&
                  base::size(kInitialHoursBetweenEvents) ==
                      static_cast<int>(UserClassifier::Event::kMaxValue) + 1 &&
                  base::size(kInitialHoursBetweenEventsParams) ==
                      static_cast<int>(UserClassifier::Event::kMaxValue) + 1,
              "Fill in info for all event types.");

// Computes the discount rate.
double GetDiscountRatePerHour() {
  double discount_rate_per_day = variations::GetVariationParamByFeatureAsDouble(
      kInterestFeedContentSuggestions, kDiscountRatePerDayParam,
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

double GetInitialHoursBetweenEvents(UserClassifier::Event event) {
  return variations::GetVariationParamByFeatureAsDouble(
      kInterestFeedContentSuggestions,
      kInitialHoursBetweenEventsParams[static_cast<int>(event)],
      kInitialHoursBetweenEvents[static_cast<int>(event)]);
}

double GetMinHours() {
  return variations::GetVariationParamByFeatureAsDouble(
      kInterestFeedContentSuggestions, kMinHoursParam, kMinHours);
}

double GetMaxHours() {
  return variations::GetVariationParamByFeatureAsDouble(
      kInterestFeedContentSuggestions, kMaxHoursParam, kMaxHours);
}

// Returns the new value of the rate using its |old_value|, assuming
// |hours_since_last_time| hours have passed since it was last discounted.
double DiscountRate(double old_value,
                    double hours_since_last_time,
                    double discount_rate_per_hour) {
  // Compute the new discounted average according to the formula
  //   avg_events := e^{-discount_rate_per_hour * hours_since} * avg_events
  return std::exp(-discount_rate_per_hour * hours_since_last_time) * old_value;
}

// Compute the number of hours between two events for the given rate value
// assuming the events were equally distributed.
double GetEstimateHoursBetweenEvents(double rate,
                                     double discount_rate_per_hour,
                                     double min_hours,
                                     double max_hours) {
  // The computation below is well-defined only for |rate| > 1 (log of
  // negative value or division by zero). When |rate| -> 1, the estimate
  // below -> infinity, so max_hours is a natural result, here.
  if (rate <= 1) {
    return max_hours;
  }

  // This is the estimate with the assumption that last event happened right
  // now and the system is in the steady-state. Solve estimate_hours in the
  // steady-state equation:
  //   rate = 1 + e^{-discount_rate * estimate_hours} * rate,
  // i.e.
  //   -discount_rate * estimate_hours = log((rate - 1) / rate),
  //   discount_rate * estimate_hours = log(rate / (rate - 1)),
  //   estimate_hours = log(rate / (rate - 1)) / discount_rate.
  double estimate_hours = std::log(rate / (rate - 1)) / discount_rate_per_hour;
  return base::ClampToRange(estimate_hours, min_hours, max_hours);
}

// The inverse of GetEstimateHoursBetweenEvents().
double GetRateForEstimateHoursBetweenEvents(double estimate_hours,
                                            double discount_rate_per_hour,
                                            double min_hours,
                                            double max_hours) {
  // Keep the input value within [min_hours, max_hours].
  estimate_hours = base::ClampToRange(estimate_hours, min_hours, max_hours);
  // Return |rate| such that GetEstimateHoursBetweenEvents for |rate| returns
  // |estimate_hours|. Thus, solve |rate| in
  //   rate = 1 + e^{-discount_rate * estimate_hours} * rate,
  // i.e.
  //   rate * (1 - e^{-discount_rate * estimate_hours}) = 1,
  //   rate = 1 / (1 - e^{-discount_rate * estimate_hours}).
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
              kInterestFeedContentSuggestions,
              kActiveConsumerClicksAtLeastOncePerHoursParam,
              kActiveConsumerClicksAtLeastOncePerHours)),
      rare_viewer_opens_surface_at_most_once_per_hours_(
          variations::GetVariationParamByFeatureAsDouble(
              kInterestFeedContentSuggestions,
              kRareUserViewsAtMostOncePerHoursParam,
              kRareUserViewsAtMostOncePerHours)) {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return;
  }

  // TODO(jkrcal): Store the current discount rate per hour into prefs. If it
  // differs from the previous value, rescale the rate values so that the
  // expectation does not change abruptly!

  // Initialize the prefs storing the last time: the counter has just started!
  for (const Event event : kEvents) {
    if (!HasLastTime(event)) {
      SetLastTimeToNow(event);
    }
  }
}

UserClassifier::~UserClassifier() = default;

// static
void UserClassifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  double discount_rate = GetDiscountRatePerHour();
  double min_hours = GetMinHours();
  double max_hours = GetMaxHours();

  for (Event event : kEvents) {
    double default_rate = GetRateForEstimateHoursBetweenEvents(
        GetInitialHoursBetweenEvents(event), discount_rate, min_hours,
        max_hours);
    registry->RegisterDoublePref(kRateKeys[static_cast<int>(event)],
                                 default_rate);
    registry->RegisterTimePref(kLastTimeKeys[static_cast<int>(event)],
                               base::Time());
  }
}

void UserClassifier::OnEvent(Event event) {
  double metric_value = UpdateRateOnEvent(event);
  double avg = GetEstimateHoursBetweenEvents(
      metric_value, discount_rate_per_hour_, min_hours_, max_hours_);
  // We use kMaxHours as the max value below as the maximum value for the
  // histograms must be constant.
  switch (event) {
    case Event::kSuggestionsViewed:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToOpenNTP, avg, 1,
                                  kMaxHours, 50);
      break;
    case Event::kSuggestionsUsed:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToUseSuggestions, avg,
                                  1, kMaxHours, 50);
      break;
  }
}

double UserClassifier::GetEstimatedAvgTime(Event event) const {
  double rate = GetUpToDateRate(event);
  return GetEstimateHoursBetweenEvents(rate, discount_rate_per_hour_,
                                       min_hours_, max_hours_);
}

UserClassifier::UserClass UserClassifier::GetUserClass() const {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return UserClass::kActiveSuggestionsViewer;
  }

  if (GetEstimatedAvgTime(Event::kSuggestionsViewed) >=
      rare_viewer_opens_surface_at_most_once_per_hours_) {
    return UserClass::kRareSuggestionsViewer;
  }

  if (GetEstimatedAvgTime(Event::kSuggestionsUsed) <=
      active_consumer_clicks_at_least_once_per_hours_) {
    return UserClass::kActiveSuggestionsConsumer;
  }

  return UserClass::kActiveSuggestionsViewer;
}

std::string UserClassifier::GetUserClassDescriptionForDebugging() const {
  switch (GetUserClass()) {
    case UserClass::kRareSuggestionsViewer:
      return "Rare viewer of Feed articles";
    case UserClass::kActiveSuggestionsViewer:
      return "Active viewer of Feed articles";
    case UserClass::kActiveSuggestionsConsumer:
      return "Active consumer of Feed articles";
  }
  NOTREACHED();
  return std::string();
}

void UserClassifier::ClearClassificationForDebugging() {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return;
  }

  for (const Event& event : kEvents) {
    ClearRate(event);
    SetLastTimeToNow(event);
  }
}

double UserClassifier::UpdateRateOnEvent(Event event) {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return 0;
  }

  double hours_since_last_time =
      std::min(max_hours_, GetHoursSinceLastTime(event));
  // Ignore events within the same "browsing session".
  if (hours_since_last_time < min_hours_) {
    return GetUpToDateRate(event);
  }

  SetLastTimeToNow(event);

  double rate = GetRate(event);
  // Add 1 to the discounted rate as the event has happened right now.
  double new_rate =
      1 + DiscountRate(rate, hours_since_last_time, discount_rate_per_hour_);
  SetRate(event, new_rate);
  return new_rate;
}

double UserClassifier::GetUpToDateRate(Event event) const {
  // The pref_service_ can be null in tests.
  if (!pref_service_) {
    return 0;
  }

  double hours_since_last_time =
      std::min(max_hours_, GetHoursSinceLastTime(event));

  double rate = GetRate(event);
  return DiscountRate(rate, hours_since_last_time, discount_rate_per_hour_);
}

double UserClassifier::GetHoursSinceLastTime(Event event) const {
  if (!HasLastTime(event)) {
    return 0;
  }

  base::TimeDelta since_last_time =
      clock_->Now() -
      pref_service_->GetTime(kLastTimeKeys[static_cast<int>(event)]);
  return since_last_time.InSecondsF() / 3600;
}

bool UserClassifier::HasLastTime(Event event) const {
  return pref_service_->HasPrefPath(kLastTimeKeys[static_cast<int>(event)]);
}

void UserClassifier::SetLastTimeToNow(Event event) {
  pref_service_->SetTime(kLastTimeKeys[static_cast<int>(event)], clock_->Now());
}

double UserClassifier::GetRate(Event event) const {
  return pref_service_->GetDouble(kRateKeys[static_cast<int>(event)]);
}

void UserClassifier::SetRate(Event event, double rate) {
  pref_service_->SetDouble(kRateKeys[static_cast<int>(event)], rate);
}

void UserClassifier::ClearRate(Event event) {
  pref_service_->ClearPref(kRateKeys[static_cast<int>(event)]);
}

}  // namespace feed
