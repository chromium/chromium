// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_timer.h"

#include <utility>

#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"

RelaunchRecommendedTimer::RelaunchRecommendedTimer(
    base::Time upgrade_detected_time,
    base::RepeatingClosure callback)
    : upgrade_detected_time_(upgrade_detected_time),
      callback_(std::move(callback)) {
  ScheduleNextTitleRefresh();
}

RelaunchRecommendedTimer::~RelaunchRecommendedTimer() {}

std::u16string RelaunchRecommendedTimer::GetWindowTitle() const {
  const base::TimeDelta elapsed = base::Time::Now() - upgrade_detected_time_;
  return l10n_util::GetPluralStringFUTF16(IDS_RELAUNCH_RECOMMENDED_TITLE,
                                          elapsed.InDays());
}

void RelaunchRecommendedTimer::ScheduleNextTitleRefresh() {
  // Refresh at the next day boundary.
  const base::Time now = base::Time::Now();
  const base::TimeDelta elapsed = now - upgrade_detected_time_;
  const base::TimeDelta delta = base::Days(elapsed.InDays() + 1) - elapsed;

  refresh_timer_.Start(FROM_HERE, now + delta, this,
                       &RelaunchRecommendedTimer::OnTitleRefresh);
}

void RelaunchRecommendedTimer::OnTitleRefresh() {
  callback_.Run();
  ScheduleNextTitleRefresh();
}
