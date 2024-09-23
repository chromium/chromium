// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_

#include <iosfwd>

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
enum class InstallTrigger {
  AMBIENT_BADGE,
  API,
  AUTOMATIC_PROMPT,
  MENU,
  CREATE_SHORTCUT,
};

// Sources for triggering webapp installation. Each install source must map to
// one web_app::Source::Type that is calculated in the method
// `web_app::ConvertExternalInstallSourceToSource`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// NOTE: each enum entry which is reportable must be added to
// InstallableMetrics::IsReportableInstallSource(). This enum backs a UMA
// histogram and must be treated as append-only. A Java counterpart will be
// generated for this enum.
//
// This should be kept in sync with WebappInstallSource in
// tools/metrics/histograms/enums.xml.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.webapps
enum class WebappInstallSource {
  // Menu item in a browser tab.
  MENU_BROWSER_TAB = 0,

  // Menu item in an Android Custom Tab.
  MENU_CUSTOM_TAB = 1,

  // Automatic prompt in a browser tab.
  AUTOMATIC_PROMPT_BROWSER_TAB = 2,

  // Automatic prompt in an Android Custom Tab.
  AUTOMATIC_PROMPT_CUSTOM_TAB = 3,

  // Developer-initiated API in a browser tab.
  API_BROWSER_TAB = 4,

  // Developer-initiated API in an Android Custom Tab.
  API_CUSTOM_TAB = 5,

  // Installation from a debug flow (e.g. via devtools).
  DEVTOOLS = 6,

  // Extensions management API (not reported).
  MANAGEMENT_API = 7,

  // PWA ambient badge in Android browser Tab.
  AMBIENT_BADGE_BROWSER_TAB = 8,

  // PWA ambient badge in an Android Custom Tab.
  AMBIENT_BADGE_CUSTOM_TAB = 9,

  // Installation via ARC on Chrome OS.
  ARC = 10,

  // An internal default-installed app on Chrome OS (i.e. triggered from code).
  INTERNAL_DEFAULT = 11,

  // An external default-installed app on Chrome OS (i.e. triggered from an
  // external source file).
  EXTERNAL_DEFAULT = 12,

  // A policy-installed app on Chrome OS.
  // Note: IWAs use a separate `ISOLATED_WEB_APP_EXTERNAL_POLICY` source.
  EXTERNAL_POLICY = 13,

  // A system app installed on Chrome OS.
  SYSTEM_DEFAULT = 14,

  // Install icon in the Omnibox.
  OMNIBOX_INSTALL_ICON = 15,

  // Installed from sync (not reported by |TrackInstallEvent|).
  SYNC = 16,

  // Create shortcut item in menu
  MENU_CREATE_SHORTCUT = 17,

  // Installed via the SubApps API.
  SUB_APP = 18,

  // Chrome Android service for installing WebAPKs from another app.
  CHROME_SERVICE = 19,

  // PWA rich install bottom sheet in WebLayer.
  RICH_INSTALL_UI_WEBLAYER = 20,

  // Installed by Kiosk on Chrome OS.
  KIOSK = 21,

  // Isolated app installation for development via command line.
  IWA_DEV_COMMAND_LINE = 22,

  // Lock screen app infrastructure installing to the lock screen app profile.
  EXTERNAL_LOCK_SCREEN = 23,

  // OEM apps installed by the App Preload Service on ChromeOS.
  PRELOADED_OEM = 24,

  // Installed via the Microsoft 365 setup dialog.
  MICROSOFT_365_SETUP = 25,

  // Profile picking in ProfileMenuView (for installable WebUIs).
  PROFILE_MENU = 26,

  // Installation promotion was triggered via ML model.
  ML_PROMOTION = 27,

  // Default apps installed by the App Preload Service on ChromeOS.
  PRELOADED_DEFAULT = 28,

  // Apps installed in shimless RMA.
  IWA_SHIMLESS_RMA = 29,

  // A policy-installed Isolated Web App.
  // Note: PWAs use a separate `EXTERNAL_POLICY` source.
  IWA_EXTERNAL_POLICY = 30,

  IWA_GRAPHICAL_INSTALLER = 31,

  IWA_DEV_UI = 32,

  // Web apps installed via almanac://install-app navigation, ChromeOS only, see
  // [App Install
  // Service](../../chrome/browser/apps/app_service/app_install/README.md).
  ALMANAC_INSTALL_APP_URI = 33,

  // WebAPK Backup and restore.
  WEBAPK_RESTORE = 34,

  // Recommended apps screen in the ChromeOS Out Of Box Experience.
  OOBE_APP_RECOMMENDATIONS = 35,

  // Add any new values above this one.
  COUNT,
};

std::ostream& operator<<(std::ostream& os, WebappInstallSource source);

// Uninstall surface from which an uninstall was initiated. This value cannot be
// used to infer an install source. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused.
enum class WebappUninstallSource {
  // Unknown surface, potentially in ChromeOS.
  kUnknown = 0,

  // Menu item from the 3-dot menu of a WebApp window.
  kAppMenu = 1,

  // Context menu for a WebApp in chrome://apps.
  kAppsPage = 2,

  // Via OS Settings or Controls.
  kOsSettings = 3,

  // Uninstalled from Sync.
  kSync = 4,

  // App management surface, currently ChromeOS-only.
  kAppManagement = 5,

  // Migration.
  kMigration = 6,

  // App List (Launcher in ChromeOS).
  kAppList = 7,

  // Shelf (in ChromeOS).
  kShelf = 8,

  // Internally managed pre-installed app management.
  kInternalPreinstalled = 9,

  // Externally managed pre-installed app management.
  kExternalPreinstalled = 10,

  // Enterprise policy app management.
  // Note: IWAs use a separate `kIwaEnterprisePolicy` source.
  kExternalPolicy = 11,

  // System app management on ChromeOS.
  kSystemPreinstalled = 12,

  // Placeholder app management for preinstalled apps.
  kPlaceholderReplacement = 13,

  // Externally managed Arc apps.
  kArc = 14,

  // SubApp API.
  kSubApp = 15,

  // On system startup, any apps that are flagged as uninstalling but have not
  // yet been fully uninstalled are re-uninstalled.
  kStartupCleanup = 16,

  // Used to track uninstalls for web_apps which are installed as sub-apps and
  // are being removed because of the removal of the parent app.
  kParentUninstall = 17,

  // Lock screen app infrastructure uninstalling from the lock screen app
  // profile.
  kExternalLockScreen = 18,

  // Tests often need a way of fully installing apps to clean up OS integration.
  kTestCleanup = 19,

  // The DedupeInstallUrlsCommand.
  kInstallUrlDeduping = 20,

  // Healthcare app cleaning up all user installed apps in between shared
  // sessions.
  kHealthcareUserInstallCleanup = 21,

  // Isolated Web App Enterprise policy.
  kIwaEnterprisePolicy = 22,

  // Via devtools PWA.uninstall or similar commands.
  kDevtools = 23,

  // Add any new values above this one.
  kMaxValue = kDevtools,
};

std::ostream& operator<<(std::ostream& os, WebappUninstallSource source);

bool IsUserUninstall(WebappUninstallSource source);

class InstallableMetrics {
 public:
  InstallableMetrics() = delete;
  InstallableMetrics(const InstallableMetrics&) = delete;
  InstallableMetrics& operator=(const InstallableMetrics&) = delete;

  // Records |source| in the Webapp.Install.InstallEvent histogram.
  // IsReportableInstallSource(|source|) must be true.
  static void TrackInstallEvent(WebappInstallSource source);

  // Returns whether |source| is a value that may be passed to
  // TrackInstallEvent.
  static bool IsReportableInstallSource(WebappInstallSource source);

  // Returns the appropriate WebappInstallSource for |web_contents| when the
  // install originates from |trigger|.
  static WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      InstallTrigger trigger);

  // Records |source| in the Webapp.Install.UninstallEvent histogram.
  static void TrackUninstallEvent(WebappUninstallSource source);

  // Records the result for WebApp.Install.Result,
  // WebApp.Install.Source.Success and WebApp.Install.Source.Failure
  // histograms.
  static void TrackInstallResult(bool result, WebappInstallSource source);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_METRICS_H_
