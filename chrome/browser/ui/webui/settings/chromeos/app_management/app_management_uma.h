// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_APP_MANAGEMENT_APP_MANAGEMENT_UMA_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_APP_MANAGEMENT_APP_MANAGEMENT_UMA_H_

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppManagementEntryPoint enum listing in
// tools/metrics/histograms/enums.xml.
enum class AppManagementEntryPoint {
  kAppListContextMenuAppInfoArc = 0,
  kAppListContextMenuAppInfoChromeApp = 1,
  kAppListContextMenuAppInfoWebApp = 2,
  kShelfContextMenuAppInfoArc = 3,
  kShelfContextMenuAppInfoChromeApp = 4,
  kShelfContextMenuAppInfoWebApp = 5,
  kAppManagementMainViewArc = 6,
  kAppManagementMainViewChromeApp = 7,
  kAppManagementMainViewWebApp = 8,
  kOsSettingsMainPage = 9,
  kAppManagementMainViewPluginVm = 10,
  kDBusServicePluginVm = 11,
  kNotificationPluginVm = 12,
  kMaxValue = kNotificationPluginVm,
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_APP_MANAGEMENT_APP_MANAGEMENT_UMA_H_
