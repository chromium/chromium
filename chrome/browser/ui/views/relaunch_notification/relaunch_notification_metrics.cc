// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_notification_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace {

constexpr char kShowResultHistogramPrefix[] = "RelaunchNotification.ShowResult";
constexpr char kRecommendedSuffix[] = ".Recommended";
constexpr char kRequiredSuffix[] = ".Required";

// The result of an attempt to show a relaunch notification dialog. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class ShowResult {
  kShown = 0,
  DEPRECATED_kUnknownNotShownReason = 1,
  DEPRECATED_kBackgroundModeNoWindows = 2,
  kCount
};

}  // namespace

namespace relaunch_notification {

void RecordRecommendedShowResult() {
  base::UmaHistogramEnumeration(
      std::string(kShowResultHistogramPrefix) + kRecommendedSuffix,
      ShowResult::kShown, ShowResult::kCount);
}

void RecordRequiredShowResult() {
  base::UmaHistogramEnumeration(
      std::string(kShowResultHistogramPrefix) + kRequiredSuffix,
      ShowResult::kShown, ShowResult::kCount);
}

}  // namespace relaunch_notification
