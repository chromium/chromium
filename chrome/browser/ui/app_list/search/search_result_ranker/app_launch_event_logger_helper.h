// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_EVENT_LOGGER_HELPER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_EVENT_LOGGER_HELPER_H_

#include <array>

#include "base/time/time.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_helpers.h"

namespace app_list {

const char kArcScheme[] = "arc://";
const char kAppScheme[] = "app://play/";
const char kExtensionSchemeWithDelimiter[] = "chrome-extension://";

constexpr float kTotalHoursBucketSizeMultiplier = 1.25;

constexpr std::array<chromeos::power::ml::Bucket, 2> kClickBuckets = {
    {{20, 1}, {200, 10}}};
constexpr std::array<chromeos::power::ml::Bucket, 6>
    kTimeSinceLastClickBuckets = {{{60, 1},
                                   {600, 60},
                                   {1200, 300},
                                   {3600, 600},
                                   {18000, 1800},
                                   {86400, 3600}}};

// Returns the nearest bucket for |value|, where bucket sizes are determined
// exponentially, with each bucket size increasing by a factor of |base|.
// The return value is rounded to the nearest integer.
int ExponentialBucket(int value, float base);

int HourOfDay(base::Time time);
int DayOfWeek(base::Time time);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_EVENT_LOGGER_HELPER_H_
