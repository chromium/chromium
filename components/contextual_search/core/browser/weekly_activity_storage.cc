// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/weekly_activity_storage.h"

#include <algorithm>  // std::min

#include "base/check.h"

namespace {

// Used for validation in debug build.  Week numbers are > 2300 as of year 2016.
// TODO(donnd): reenable this const.  https://crbug.com/1094008. See below.
// const int kReasonableMinWeek = 2000;

}  // namespace

namespace contextual_search {

WeeklyActivityStorage::WeeklyActivityStorage(int weeks_needed) {
  weeks_needed_ = weeks_needed;
}

WeeklyActivityStorage::~WeeklyActivityStorage() {}

int WeeklyActivityStorage::ReadClicks(int week_number) {
  return ReadClicksForWeekRemainder(GetWeekRemainder(week_number));
}

void WeeklyActivityStorage::WriteClicks(int week_number, int value) {
  WriteClicksForWeekRemainder(GetWeekRemainder(week_number), value);
}

int WeeklyActivityStorage::ReadImpressions(int week_number) {
  return ReadImpressionsForWeekRemainder(GetWeekRemainder(week_number));
}

void WeeklyActivityStorage::WriteImpressions(int week_number, int value) {
  WriteImpressionsForWeekRemainder(GetWeekRemainder(week_number), value);
}

bool WeeklyActivityStorage::HasData(int week_number) {
  return ReadOldestWeekWritten() <= week_number &&
         ReadNewestWeekWritten() >= week_number;
}

void WeeklyActivityStorage::AdvanceToWeek(int week_number) {
  EnsureHasActivity(week_number);
}

// private

// Round-robin implementation:
// GetWeekRemainder and EnsureHasActivity are implemented with a round-robin
// implementation that simply recycles usage of the last N weeks, where N is
// less than weeks_needed_.

int WeeklyActivityStorage::GetWeekRemainder(int which_week) {
  return which_week % (weeks_needed_ + 1);
}

void WeeklyActivityStorage::EnsureHasActivity(int which_week) {
  // TODO(donnd): reenable this DCHECK.  Some bots have bad clocks or time
  // settings, causing flaky test failures. https://crbug.com/1094008.
  // DCHECK(which_week > kReasonableMinWeek);

  // If still on the newest week we're done!
  int newest_week = ReadNewestWeekWritten();
  if (newest_week == which_week)
    return;

  // Update the newest and oldest week written.
  if (which_week > newest_week) {
    WriteNewestWeekWritten(which_week);
  }
  int oldest_week = ReadOldestWeekWritten();
  if (oldest_week == 0 || oldest_week > which_week)
    WriteOldestWeekWritten(which_week);

  // Any stale weeks to update?
  if (newest_week == 0)
    return;

  // Moved to some new week beyond the newest previously recorded.
  // Since we recycle storage we must clear the new week and all that we
  // may have skipped since our last access.
  int weeks_to_clear = std::min(which_week - newest_week, weeks_needed_);
  int week = which_week;
  while (weeks_to_clear > 0) {
    WriteImpressionsForWeekRemainder(GetWeekRemainder(week), 0);
    WriteClicksForWeekRemainder(GetWeekRemainder(week), 0);
    week--;
    weeks_to_clear--;
  }
}

}  // namespace contextual_search
