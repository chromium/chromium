// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_web_apps/system_web_app_utils.h"

#include "ash/constants/webui_url_constants.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "url/gurl.h"

namespace ash {

namespace {

void ShowSystemAppInternal(Profile* profile, const SystemWebAppType type) {
  SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kUnknown;
  LaunchSystemWebAppAsync(profile, type, params);
}

void ShowSystemAppInternal(Profile* profile,
                           const SystemWebAppType type,
                           const SystemAppLaunchParams& params) {
  LaunchSystemWebAppAsync(profile, type, params);
}

}  // namespace

void ShowGraduationApp(Profile* profile) {
  SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kFromOtherApp;
  ShowSystemAppInternal(profile, SystemWebAppType::GRADUATION, params);
}

void ShowPrintManagementApp(Profile* profile) {
  ShowSystemAppInternal(profile, SystemWebAppType::PRINT_MANAGEMENT);
}

void ShowConnectivityDiagnosticsApp(Profile* profile) {
  ShowSystemAppInternal(profile, SystemWebAppType::CONNECTIVITY_DIAGNOSTICS);
}

void ShowScanningApp(Profile* profile) {
  ShowSystemAppInternal(profile, SystemWebAppType::SCANNING);
}

void ShowDiagnosticsApp(Profile* profile) {
  ShowSystemAppInternal(profile, SystemWebAppType::DIAGNOSTICS);
}

void ShowFirmwareUpdatesApp(Profile* profile) {
  ShowSystemAppInternal(profile, SystemWebAppType::FIRMWARE_UPDATE);
}

void ShowShortcutCustomizationApp(Profile* profile) {
  ShowSystemAppInternal(profile, SystemWebAppType::SHORTCUT_CUSTOMIZATION);
}

void ShowShortcutCustomizationApp(Profile* profile,
                                  const std::string& action,
                                  const std::string& category) {
  const std::string query_string =
      base::StrCat({"action=", action, "&category=", category});
  SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kUnknown;
  params.url = GURL(
      base::StrCat({kChromeUIShortcutCustomizationAppURL, "?", query_string}));
  ShowSystemAppInternal(profile, SystemWebAppType::SHORTCUT_CUSTOMIZATION,
                        params);
}

}  // namespace ash
