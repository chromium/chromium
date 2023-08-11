// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/mac_notification_metrics.h"

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace mac_notifications {

ProcessType ProcessTypeFromAppBundle() {
  NSDictionary* infoDictionary = [base::apple::MainBundle() infoDictionary];
  NSString* appModeShortcut = infoDictionary[@"CrAppModeShortcutID"];
  if (appModeShortcut != nil) {
    return ProcessType::kAppShimProcess;
  }
  NSString* alertStyle = infoDictionary[@"NSUserNotificationAlertStyle"];
  return [alertStyle isEqualToString:@"alert"] ? ProcessType::kAlertProcess
                                               : ProcessType::kInProcess;
}

std::string MacNotificationStyleSuffix(ProcessType process_type) {
  switch (process_type) {
    case ProcessType::kInProcess:
      return "Banner";
    case ProcessType::kAlertProcess:
      return "Alert";
    case ProcessType::kAppShimProcess:
      return "AppShim";
  }
  NOTREACHED();
}

void LogMacNotificationActionReceived(ProcessType process_type, bool is_valid) {
  base::UmaHistogramBoolean(
      base::StrCat({"Notifications.macOS.ActionReceived.",
                    MacNotificationStyleSuffix(process_type)}),
      is_valid);
}

}  // namespace mac_notifications
