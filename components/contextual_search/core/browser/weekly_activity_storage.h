// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_WEEKLY_ACTIVITY_STORAGE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_WEEKLY_ACTIVITY_STORAGE_H_

namespace contextual_search {

// An abstract class that stores weekly user interaction data in device-specific
// integer storage. Only a limited storage window is supported, set through the
// constructor. Allows callers to read and write user actions to persistent
// storage on the device by overriding the ReadStorage and WriteStorage calls.
// A user view of some UX is an "Impression", and user interaction is considered
// a "Click" even if the triggering gesture was something else.  Together they
// produce the Click-Through-Rate, or CTR.
class WeeklyActivityStorage {
 public:
  // Constructs an instance that will manage at least |weeks_needed| weeks of
  // data.
  WeeklyActivityStorage(int weeks_needed);

  WeeklyActivityStorage(const WeeklyActivityStorage&) = delete;
  WeeklyActivityStorage& operator=(const WeeklyActivityStorage&) = delete;

  virtual ~WeeklyActivityStorage();

  // Advances the accessible storage range to end at the given |week_number|.
  // Since only a limited number of storage weeks are supported, advancing to
  // a different week makes data from weeks than the range size inaccessible.
  // This must be called for each week before reading or writing any data
  // for that week.
  // HasData will return true for all the weeks that still have accessible data.
  void AdvanceToWeek(int week_number);

  // Returns the number of clicks for the given week.
  int ReadClicks(int week_number);
  // Writes |value| into the number of clicks for the given |week_number|.
  void WriteClicks(int week_number, int value);

  // Returns the number of impressions for the given week.
  int ReadImpressions(int week_number);
  // Writes |value| into the number of impressions for the given |week_number|.
  void WriteImpressions(int week_number, int value);

  // Returns whether the given |week_number| has data, based on whether
  // InitData has ever been called for that week.
  bool HasData(int week_number);

  // Reads and returns values from persistent storage.
  // If there is no stored value then 0 is returned.
  virtual int ReadClicksForWeekRemainder(int week_remainder) = 0;
  virtual int ReadImpressionsForWeekRemainder(int week_remainder) = 0;
  virtual int ReadOldestWeekWritten() = 0;
  virtual int ReadNewestWeekWritten() = 0;
  // Writes values to persistent storage.
  virtual void WriteClicksForWeekRemainder(int week_remainder, int value) = 0;
  virtual void WriteImpressionsForWeekRemainder(int week_remainder,
                                                int value) = 0;
  virtual void WriteOldestWeekWritten(int value) = 0;
  virtual void WriteNewestWeekWritten(int value) = 0;

 private:
  // Returns the key to bin information about the given week |which_week|.
  int GetWeekRemainder(int which_week);

  // Ensures that activity data is initialized for the given week |which_week|.
  void EnsureHasActivity(int which_week);

  // The number of weeks of data that this instance needs to support.
  int weeks_needed_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_WEEKLY_ACTIVITY_STORAGE_H_
