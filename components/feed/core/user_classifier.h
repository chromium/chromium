// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_USER_CLASSIFIER_H_
#define COMPONENTS_FEED_CORE_USER_CLASSIFIER_H_

#include <memory>
#include <string>

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace feed {

// Collects data about user usage patterns of content suggestions, computes
// long-term user rates locally using pref, and reports the metrics to UMA.
// Based on these long-term user rates, it classifies the user in a UserClass.
class UserClassifier {
 public:
  // Different groupings of usage. A user will belong to exactly one of these at
  // any given point in time. Can change at runtime.
  enum class UserClass {
    kRareSuggestionsViewer,      // Almost never opens surfaces that show
                                 // suggestions, like the NTP.
    kActiveSuggestionsViewer,    // Frequently shown suggestions, but does not
                                 // usually open them.
    kActiveSuggestionsConsumer,  // Frequently opens news articles.
  };

  // For estimating the average length of the intervals between two successive
  // events, we keep a simple frequency model, a single value that we call
  // "rate" below.
  // We track exponentially-discounted rate of the given event per hour where
  // the continuous utility function between two successive events (e.g. opening
  // a NTP) at times t1 < t2 is 1 / (t2-t1), i.e. intuitively the rate of this
  // event in this time interval.
  // See https://en.wikipedia.org/wiki/Exponential_discounting for more details.
  // We keep track of the following events.
  // NOTE: if you add any element, add it also in the static arrays in .cc and
  // create another histogram.
  enum class Event {
    kSuggestionsViewed = 0,  // When the user opens a surface that is showing
                             // suggestions, such as the NTP. This indicates
                             // potential use of content suggestions.
    kSuggestionsUsed = 1,    // When the user clicks on some suggestions or on
                             // the "More" button.
    kMaxValue = kSuggestionsUsed
  };

  // The provided |pref_service| may be nullptr in unit-tests.
  UserClassifier(PrefService* pref_service, base::Clock* clock);
  ~UserClassifier();

  // Registers profile prefs for all rates. Called from pref_names.cc.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Informs the UserClassifier about a new event for |event|. The
  // classification is based on these calls.
  void OnEvent(Event event);

  // Get the estimate average length of the interval between two successive
  // events of the given type.
  double GetEstimatedAvgTime(Event event) const;

  // Return the classification of the current user.
  UserClass GetUserClass() const;
  std::string GetUserClassDescriptionForDebugging() const;

  // Resets the classification (emulates a fresh upgrade / install).
  void ClearClassificationForDebugging();

 private:
  // The event has happened, recompute the rate accordingly. Then store and
  // return the new rate.
  double UpdateRateOnEvent(Event event);
  // No event has happened but we need to get up-to-date rate, recompute and
  // return the new rate. This function does not store the recomputed rate.
  double GetUpToDateRate(Event event) const;

  // Returns the number of hours since the last event of the same type. If there
  // is no last event of that type, assume it happened just now and return 0.
  double GetHoursSinceLastTime(Event event) const;
  bool HasLastTime(Event event) const;
  void SetLastTimeToNow(Event event);

  double GetRate(Event event) const;
  void SetRate(Event event, double rate);
  void ClearRate(Event event);

  PrefService* pref_service_;
  base::Clock* clock_;

  // Params of the rate.
  const double discount_rate_per_hour_;
  const double min_hours_;
  const double max_hours_;

  // Params of the classification.
  const double active_consumer_clicks_at_least_once_per_hours_;
  const double rare_viewer_opens_surface_at_most_once_per_hours_;

  DISALLOW_COPY_AND_ASSIGN(UserClassifier);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_USER_CLASSIFIER_H_
