// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/ctr_aggregator.h"

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"

namespace {

const double kSecondsPerWeek =
    base::Time::kMicrosecondsPerWeek / base::Time::kMicrosecondsPerSecond;
// Used for validation in debug build.  Week numbers are > 2300 as of year 2016.
// TODO(donnd): reenable this const.  https://crbug.com/1094008. See below.
// const int kReasonableMinWeek = 2000;

}  // namespace

namespace contextual_search {

CtrAggregator::CtrAggregator(WeeklyActivityStorage& storage)
    : storage_(storage) {
  base::Time now = base::Time::NowFromSystemTime();
  double now_in_seconds = now.ToDoubleT();
  week_number_ = now_in_seconds / kSecondsPerWeek;
  // TODO(donnd): reenable this DCHECK.  Some bots have bad clocks or time
  // settings, causing flaky test failures. https://crbug.com/1094008.
  // DCHECK(week_number_ >= kReasonableMinWeek);
  // NOTE: This initialization may callback into the storage implementation so
  // that needs to be fully initialized when constructing this aggregator.
  storage_.AdvanceToWeek(week_number_);
}

// Testing only
CtrAggregator::CtrAggregator(WeeklyActivityStorage& storage, int week_number)
    : storage_(storage), week_number_(week_number) {
  storage_.AdvanceToWeek(week_number_);
}

CtrAggregator::~CtrAggregator() {}

void CtrAggregator::RecordImpression(bool did_click) {
  storage_.WriteImpressions(week_number_,
                            1 + storage_.ReadImpressions(week_number_));
  if (did_click)
    storage_.WriteClicks(week_number_, 1 + storage_.ReadClicks(week_number_));
}

int CtrAggregator::GetCurrentWeekNumber() {
  return week_number_;
}

bool CtrAggregator::HasPreviousWeekData() {
  return storage_.HasData(week_number_ - 1);
}

int CtrAggregator::GetPreviousWeekImpressions() {
  return storage_.ReadImpressions(week_number_ - 1);
}

float CtrAggregator::GetPreviousWeekCtr() {
  if (!HasPreviousWeekData())
    return NAN;

  int clicks = GetPreviousWeekClicks();
  int impressions = GetPreviousWeekImpressions();
  if (impressions == 0)
    return 0.0;
  return base::saturated_cast<float>(clicks) / impressions;
}

bool CtrAggregator::HasPrevious28DayData() {
  for (int previous = 1; previous <= kNumWeeksNeededFor28DayData; previous++) {
    if (!storage_.HasData(week_number_ - previous))
      return false;
  }
  return true;
}

float CtrAggregator::GetPrevious28DayCtr() {
  if (!HasPrevious28DayData())
    return NAN;

  int clicks = GetPrevious28DayClicks();
  int impressions = GetPrevious28DayImpressions();
  if (impressions == 0)
    return 0.0;
  return base::saturated_cast<float>(clicks) / impressions;
}

int CtrAggregator::GetPrevious28DayImpressions() {
  int impressions = 0;
  for (int previous = 1; previous <= kNumWeeksNeededFor28DayData; previous++) {
    impressions += storage_.ReadImpressions(week_number_ - previous);
  }
  return impressions;
}

// private

int CtrAggregator::GetPreviousWeekClicks() {
  return storage_.ReadClicks(week_number_ - 1);
}

int CtrAggregator::GetPrevious28DayClicks() {
  int clicks = 0;
  for (int previous = 1; previous <= kNumWeeksNeededFor28DayData; previous++) {
    clicks += storage_.ReadClicks(week_number_ - previous);
  }
  return clicks;
}

// Testing only

void CtrAggregator::IncrementWeek(int weeks) {
  week_number_ += weeks;
  storage_.AdvanceToWeek(week_number_);
}

}  // namespace contextual_search
