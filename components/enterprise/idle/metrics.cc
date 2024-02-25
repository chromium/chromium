// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/idle/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace enterprise_idle {

namespace metrics {

void RecordIdleTimeoutActionTimeTaken(IdleTimeoutActionType type,
                                      base::TimeDelta time_duration) {
  base::UmaHistogramMediumTimes(
      base::StringPrintf(kUMAIdleTimeoutActionSuccessTime,
                         GetActionNameFromActionType(type).c_str()),
      time_duration);
}

void RecordIdleTimeoutCase(IdleTimeoutCase timeout_case) {
  base::UmaHistogramEnumeration(kUMAIdleTimeoutActionCase, timeout_case);
}

void RecordActionsSuccess(IdleTimeoutActionType type, bool success) {
  base::UmaHistogramBoolean(
      base::StringPrintf(kUMAIdleTimeoutActionSuccesStatus,
                         GetActionNameFromActionType(type).c_str()),
      success);
}

void RecordIdleTimeoutDialogEvent(IdleTimeoutDialogEvent event) {
  base::UmaHistogramEnumeration(kUMAIdleTimeoutDialogEvent, event);
}

void RecordIdleTimeoutLaunchScreenEvent(IdleTimeoutLaunchScreenEvent event) {
  base::UmaHistogramEnumeration(kUMAIdleTimeoutLaunchScreenEvent, event);
}

std::string GetActionNameFromActionType(IdleTimeoutActionType type) {
  switch (type) {
    case IdleTimeoutActionType::kClearBrowsingData:
      return kUMAClearBrowsingDataSuffix;
    case IdleTimeoutActionType::kSignOut:
      return kUMASignOutSuffix;
    case IdleTimeoutActionType::kCloseBrowsers:
      return kUMACloseBrowsersSuffix;
    case IdleTimeoutActionType::kCloseTabs:
      return kUMACloseTabsSuffix;
    case IdleTimeoutActionType::kReloadPages:
      return kUMAReloadPagesSuffix;
    case IdleTimeoutActionType::kShowProfilePicker:
      return kUMAShowProfilePickerSuffix;
    case IdleTimeoutActionType::kAllActions:
      return kUMAAllActionsSuffix;
    default:
      return "Unknown";
  }
}

}  // namespace metrics

}  // namespace enterprise_idle
