// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger_helper.h"

namespace app_list {

int ExponentialBucket(int value, float base) {
  if (base <= 0) {
    LOG(DFATAL) << "Base of exponential must be positive.";
    return 0;
  }
  if (value <= 0) {
    return 0;
  }
  return round(pow(base, round(log(value) / log(base))));
}

int HourOfDay(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.hour;
}

int DayOfWeek(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return exploded.day_of_week;
}

}  // namespace app_list
