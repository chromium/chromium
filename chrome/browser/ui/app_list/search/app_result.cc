// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_result.h"

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/time/time.h"

namespace app_list {

AppResult::AppResult(Profile* profile,
                     const std::string& app_id,
                     AppListControllerDelegate* controller,
                     bool is_recommendation)
    : profile_(profile),
      app_id_(app_id),
      controller_(controller) {
  SetDisplayType(is_recommendation
                     ? ash::SearchResultDisplayType::kRecommendation
                     : ash::SearchResultDisplayType::kTile);
  SetResultType(ash::AppListSearchResultType::kInstalledApp);
}

AppResult::~AppResult() {
}

void AppResult::UpdateFromLastLaunchedOrInstalledTime(
    const base::Time& current_time,
    const base::Time& old_time) {
  // |current_time| can be before |old_time| in weird cases such as users
  // playing with their clocks. Handle this gracefully.
  if (current_time < old_time) {
    set_relevance(1.0);
    return;
  }

  base::TimeDelta delta = current_time - old_time;
  const int kSecondsInWeek = 60 * 60 * 24 * 7;

  // Set the relevance to a value between 0 and 1. This function decays as the
  // time delta increases and reaches a value of 0.5 at 1 week.
  set_relevance(1 / (1 + delta.InSecondsF() / kSecondsInWeek));
}

ash::SearchResultType AppResult::GetSearchResultType() const {
  return ash::SEARCH_RESULT_TYPE_BOUNDARY;
}

}  // namespace app_list
