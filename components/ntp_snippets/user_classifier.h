// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_USER_CLASSIFIER_H_
#define COMPONENTS_NTP_SNIPPETS_USER_CLASSIFIER_H_

#include <memory>
#include <string>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace ntp_snippets {

// Collects data about user usage patterns of content suggestions, computes
// long-term user metrics locally using pref, and reports the metrics to UMA.
// Based on these long-term user metrics, it classifies the user in a UserClass.
class UserClassifier {
 public:
  // Enumeration listing user classes
  enum class UserClass {
    RARE_NTP_USER,
    ACTIVE_NTP_USER,
    ACTIVE_SUGGESTIONS_CONSUMER,
  };

  // For estimating the average length of the intervals between two successive
  // events, we keep a simple frequency model, a single value that we call
  // "metric" below.
  // We track exponentially-discounted rate of the given event per hour where
  // the continuous utility function between two successive events (e.g. opening
  // a NTP) at times t1 < t2 is 1 / (t2-t1), i.e. intuitively the rate of this
  // event in this time interval.
  // See https://en.wikipedia.org/wiki/Exponential_discounting for more details.
  // We keep track of the following events.
  // NOTE: if you add any element, add it also in the static arrays in .cc and
  // create another histogram.
  enum class Metric {
    NTP_OPENED,  // When the user opens a new NTP - this indicates potential
                 // use of content suggestions.
    // TODO(jkrcal): Remove the following metric as for condensed NTP / Chrome
    // Home, this coincides with NTP_OPENED.
    SUGGESTIONS_SHOWN,  // When the content suggestions are shown to the user -
                        // in the current implementation when the user scrolls
                        // below the fold.
    SUGGESTIONS_USED,   // When the user clicks on some suggestions or on some
                        // "More" button.
    COUNT               // Keep this as the last element.
  };

  // The provided |pref_service| may be nullptr in unit-tests.
  UserClassifier(PrefService* pref_service, base::Clock* clock);
  ~UserClassifier();

  // Registers profile prefs for all metrics. Called from browser_prefs.cc.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Informs the UserClassifier about a new event for |metric|. The
  // classification is based on these calls.
  void OnEvent(Metric metric);

  // Get the estimate average length of the interval between two successive
  // events of the given type.
  double GetEstimatedAvgTime(Metric metric) const;

  // Return the classification of the current user.
  UserClass GetUserClass() const;
  std::string GetUserClassDescriptionForDebugging() const;

  // Resets the classification (emulates a fresh upgrade / install).
  void ClearClassificationForDebugging();

 private:
  // The event has happened, recompute the metric accordingly. Then store and
  // return the new value.
  double UpdateMetricOnEvent(Metric metric);
  // No event has happened but we need to get up-to-date metric, recompute and
  // return the new value. This function does not store the recomputed metric.
  double GetUpToDateMetricValue(Metric metric) const;

  // Returns the number of hours since the last event of the same type.
  // If there is no last event of that type, assume it happened just now and
  // return 0.
  double GetHoursSinceLastTime(Metric metric) const;
  bool HasLastTime(Metric metric) const;
  void SetLastTimeToNow(Metric metric);

  double GetMetricValue(Metric metric) const;
  void SetMetricValue(Metric metric, double metric_value);
  void ClearMetricValue(Metric metric);

  PrefService* pref_service_;
  base::Clock* clock_;

  // Params of the metric.
  const double discount_rate_per_hour_;
  const double min_hours_;
  const double max_hours_;

  // Params of the classification.
  const double active_consumer_clicks_at_least_once_per_hours_;
  const double rare_user_opens_ntp_at_most_once_per_hours_;

  DISALLOW_COPY_AND_ASSIGN(UserClassifier);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_USER_CLASSIFIER_H_
