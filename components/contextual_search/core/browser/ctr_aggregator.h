// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides aggregation of feature usage by tracking impressions and clicks
// over 1-week and 28-day intervals.  Impressions are views of some UX and
// clicks are any measured interaction with that UX, yielding CTR -- Click
// Through Rate.
// Used by Contextual Search to record impressions of the Bar and CTR of
// panel opens to use as signals for Tap triggering.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CTR_AGGREGATOR_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CTR_AGGREGATOR_H_

#include "base/gtest_prod_util.h"
#include "components/contextual_search/core/browser/weekly_activity_storage.h"

namespace contextual_search {

// Number of weeks of data needed for 28 days.
const int kNumWeeksNeededFor28DayData = 4;

// Usage: Create a CtrAggregator and start recording impressions or reading
// aggregated data.  Get data from the previous week or previous 4-week period
// that ended with the previous week.
// A new week starts at an arbitrary time based on seconds since the Epoch.
// The data from the previous week and previous 28-day period are guaranteed to
// be complete only if the HasPrevious method returns true.  If one of the data
// accessors is called when the data is not complete invalid data may be
// returned.
class CtrAggregator {
 public:
  // Constructs a CtrAggregator using the given |storage| mechanism.
  // Data is stored by |storage| typically on persistent device-local storage.
  // A callback through the storage interface may occur at construction time,
  // so the |storage| must be fully initialized when this constructor is
  // called.
  CtrAggregator(WeeklyActivityStorage& storage);

  CtrAggregator(const CtrAggregator&) = delete;
  CtrAggregator& operator=(const CtrAggregator&) = delete;

  ~CtrAggregator();

  // Records an impression.  Records a click if |did_click| is true.
  void RecordImpression(bool did_click);

  // Returns the number for the current week. Useful for checking when the
  // current week changes.
  int GetCurrentWeekNumber();

  // Returns whether we have the previous week's data for this user.
  bool HasPreviousWeekData();

  // Gets the number of impressions from the previous week.
  // Callers must check if there is previous week's data for this user, or
  // invalid data may be returned.
  int GetPreviousWeekImpressions();

  // Gets the CTR from the previous week.
  // Callers must check if there is previous week's data for this user, or
  // invalid data may be returned.
  float GetPreviousWeekCtr();

  // Returns whether we have data from a 28 day period ending in the previous
  // week.
  bool HasPrevious28DayData();

  // Gets the number of impressions from a 28 day period ending in the previous
  // week.
  // Callers must check if there is previous 28 day data for this user, or
  // invalid data may be returned.
  int GetPrevious28DayImpressions();

  // Gets the CTR from a 28 day period ending in the previous week.
  // Callers must check if there is previous 28 day data for this user, or
  // invalid data may be returned.
  float GetPrevious28DayCtr();

 private:
  // This implementation uses a fixed number of bins to store integer impression
  // and click data for the most recent N weeks, where N = 5 (in order to keep 4
  // complete weeks).  Another bin keeps track of the current week being
  // written.  Yet another bin records when data was first stored or accessed so
  // we can know when a time period has complete data.
  friend class CtrAggregatorTest;
  FRIEND_TEST_ALL_PREFIXES(CtrAggregatorTest, SimpleOperationTest);
  FRIEND_TEST_ALL_PREFIXES(CtrAggregatorTest, MultiWeekTest);
  FRIEND_TEST_ALL_PREFIXES(CtrAggregatorTest, SkipOneWeekTest);
  FRIEND_TEST_ALL_PREFIXES(CtrAggregatorTest, SkipThreeWeeksTest);
  FRIEND_TEST_ALL_PREFIXES(CtrAggregatorTest, SkipFourWeeksTest);

  // Constructs an instance for testing; sets the week.
  CtrAggregator(WeeklyActivityStorage& storage, int week_number);
  // For testing, increments the current week number by |weeks|.
  void IncrementWeek(int weeks);

  // Gets the number of clicks from the previous week.
  // Callers must check if there is previous week's data for this user, or
  // invalid data may be returned.
  int GetPreviousWeekClicks();
  // Gets the number of clicks from a 28 day period ending in the previous
  // week.
  // Callers must check if there is previous 28 day data for this user, or
  // invalid data may be returned.
  int GetPrevious28DayClicks();

  // Stores the weekly activity data.
  WeeklyActivityStorage& storage_;

  // The current week number, expressed as the number of weeks since Epoch.
  int week_number_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CTR_AGGREGATOR_H_
