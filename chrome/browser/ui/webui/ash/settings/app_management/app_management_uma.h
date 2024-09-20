// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_APP_MANAGEMENT_APP_MANAGEMENT_UMA_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_APP_MANAGEMENT_APP_MANAGEMENT_UMA_H_

namespace ash::settings {

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppManagementEntryPoint enum listing in
// tools/metrics/histograms/metadata/apps/enums.xml.
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
  kAppManagementMainViewBorealis = 13,
  kPageInfoView = 14,
  kPrivacyIndicatorsNotificationSettings = 15,
  kSubAppsInstallPrompt = 16,
  kSiteDataDialog = 17,
  kMaxValue = kSiteDataDialog,
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_APP_MANAGEMENT_APP_MANAGEMENT_UMA_H_
