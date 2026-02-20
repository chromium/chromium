// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_UTILS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_UTILS_H_

#include <string>

class Profile;

namespace ash {

// Shows the Graduation app.
void ShowGraduationApp(Profile* profile);

// Shows the Print Management app.
void ShowPrintManagementApp(Profile* profile);

// Shows the Connectivity Diagnostics app.
void ShowConnectivityDiagnosticsApp(Profile* profile);

// Shows the Scanning app.
void ShowScanningApp(Profile* profile);

// Shows the Diagnostics app.
void ShowDiagnosticsApp(Profile* profile);

// Shows the Firmware Updates app.
void ShowFirmwareUpdatesApp(Profile* profile);

// Shows the Shortcut Customization app.
void ShowShortcutCustomizationApp(Profile* profile);

// Shows the Shortcut Customization app with the given action and category.
// The `action` and `category` will be appended the app URL in the following
// format: url?action={action}&category={category}.
void ShowShortcutCustomizationApp(Profile* profile,
                                  const std::string& action,
                                  const std::string& category);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_UTILS_H_
