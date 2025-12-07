// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/android_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace device_signals {

std::optional<int64_t> GetSecurityPatchLevelEpoch() {
  auto security_patch_date = base::android::android_info::security_patch();

  auto split_date = base::SplitString(
      security_patch_date, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split_date.size() != 3) {
    return std::nullopt;
  }

  int year, month, day_of_month;
  if (!base::StringToInt(split_date[0], &year) ||
      !base::StringToInt(split_date[1], &month) ||
      !base::StringToInt(split_date[2], &day_of_month)) {
    return std::nullopt;
  }

  const base::Time::Exploded exploded = {
      .year = year, .month = month, .day_of_month = day_of_month};
  base::Time time;
  if (!base::Time::FromUTCExploded(exploded, &time)) {
    return std::nullopt;
  }
  return time.InMillisecondsSinceUnixEpoch();
}

}  // namespace device_signals
