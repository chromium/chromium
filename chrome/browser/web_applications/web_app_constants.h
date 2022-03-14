// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_

#include <iosfwd>
#include <vector>

#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

// Install sources are listed in the order of priority (from top to bottom).
//
// This enum should be zero based: values are used as index in a bitset.
// We don't use this enum values in prefs or metrics: enumerators can be
// reordered. This enum is not strongly typed enum class: it supports implicit
// conversion to int and <> comparison operators.
namespace Source {
enum Type {
  kMinValue = 0,
  kSystem = kMinValue,
  kPolicy,
  kSubApp,
  kWebAppStore,
  // We sync only regular user-installed apps from the open web. For
  // user-installed apps without overlaps this is the only source that will be
  // set.
  kSync,
  kDefault,
  kMaxValue = kDefault,
};
}  // namespace Source

std::ostream& operator<<(std::ostream& os, Source::Type type);

// Type of OS hook.
//
// This enum should be zero based. It is not strongly typed enum class to
// support implicit conversion to int. Values are also used as index in
// OsHooksErrors and OsHooksOptions.
namespace OsHookType {
enum Type {
  kShortcuts = 0,
  kRunOnOsLogin,
  kShortcutsMenu,
  kUninstallationViaOsSettings,
  kFileHandlers,
  kProtocolHandlers,
  kUrlHandlers,
  kMaxValue = kUrlHandlers,
};
}  // namespace OsHookType

// ExternallyManagedAppManager: Where an app was installed from. This affects
// what flags will be used when installing the app.
//
// Internal means that the set of apps to install is defined statically, and
// can be determined solely by 'first party' data: the Chromium binary,
// stored user preferences (assumed to have been edited only by Chromiums
// past and present) and the like. External means that the set of apps to
// install is defined dynamically, depending on 'third party' data that can
// change from session to session even if those sessions are for the same
// user running the same binary on the same hardware.
//
// Third party data sources can include configuration files in well known
// directories on the file system, entries (or the lack of) in the Windows
// registry, or centrally configured sys-admin policy.
//
// The internal versus external distinction matters because, for external
// install sources, the code that installs apps based on those external data
// sources can also need to *un*install apps if those external data sources
// change, either by an explicit uninstall request or an implicit uninstall
// of a previously-listed no-longer-listed app.
//
// Without the distinction between e.g. kInternalDefault and kExternalXxx, the
// code that manages external-xxx apps might inadvertently uninstall internal
// apps that it otherwise doesn't recognize.
//
// In practice, every kExternalXxx enum definition should correspond to
// exactly one place in the code where
// ExternallyManagedAppManager::SynchronizeInstalledApps is called.
enum class ExternalInstallSource {
  // Do not remove or re-order the names, only append to the end. Their
  // integer values are persisted in the preferences.

  // Installed by default on the system from the C++ code. AndroidSms app is an
  // example.
  kInternalDefault = 0,

  // Installed by default on the system, such as "all such-and-such make and
  // model Chromebooks should have this app installed".
  // The corresponding ExternallyManagedAppManager::SynchronizeInstalledApps
  // call site is
  // in WebAppProvider::OnScanForExternalWebApps.
  kExternalDefault = 1,

  // Installed by sys-admin policy, such as "all example.com employees should
  // have this app installed".
  // The corresponding ExternallyManagedAppManager::SynchronizeInstalledApps
  // call site is
  // in WebAppPolicyManager::RefreshPolicyInstalledApps.
  kExternalPolicy = 2,

  // Installed as a Chrome component, such as a help app, or a settings app.
  // The corresponding ExternallyManagedAppManager::SynchronizeInstalledApps
  // call site is
  // in SystemWebAppManager::RefreshPolicyInstalledApps.
  kSystemInstalled = 3,

  // Installed from ARC.
  // There is no call to SynchronizeInstalledApps for this type, as these apps
  // are not installed via ExternallyManagedAppManager. This is used in
  // ExternallyInstalledWebAppPrefs to track navigation url to app_id entries.
  kArc = 4,
};

// Icon size in pixels.
// Small icons are used in confirmation dialogs and app windows.
constexpr int kWebAppIconSmall = 32;

using DisplayMode = blink::mojom::DisplayMode;

// When user_display_mode indicates a user preference for opening in
// a browser tab, we open in a browser tab. If the developer has specified
// the app should utilize more advanced display modes and/or fallback chain,
// attempt honor those preferences. Otherwise, we open in a standalone
// window (for app_display_mode 'standalone' or 'fullscreen'), or a minimal-ui
// window (for app_display_mode 'browser' or 'minimal-ui').
DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    DisplayMode user_display_mode);

apps::mojom::LaunchContainer ConvertDisplayModeToAppLaunchContainer(
    DisplayMode display_mode);

// The operation mode for Run on OS Login.
enum class RunOnOsLoginMode {
  kMinValue = 0,

  // kNotRun: The web app will not run during OS login.
  kNotRun = kMinValue,
  // kWindowed: The web app will run during OS login and will be launched as
  // normal window. This is also the default launch mode for web apps.
  kWindowed = 1,
  // kMinimized: The web app will run during OS login and will be launched as a
  // minimized window.
  kMinimized = 2,
  kMaxValue = kMinimized,
};

// Command line parameter representing RunOnOsLoginMode::kWindowed.
extern const char kRunOnOsLoginModeWindowed[];

enum class RunOnOsLoginPolicy {
  // kAllowed: User can configure an app to run on OS Login.
  kAllowed = 0,
  // kDisallow: Policy prevents users from configuring an app to run on OS
  // Login.
  kBlocked = 1,
  // kRunWindowed: Policy requires an app to to run on OS Login as a normal
  // window.
  kRunWindowed = 2,
};

std::string RunOnOsLoginModeToString(RunOnOsLoginMode mode);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Records result of user reaction to install in-product help promo.
enum class InstallIphResult {
  // Installed the web app after IPH was shown.
  kInstalled = 0,
  // Clicked the install icon but canceled install after IPH was shown.
  kCanceled = 1,
  // Ignored IPH, didn't click install.
  kIgnored = 2,
  kMaxValue = kIgnored,
};

// Number of times IPH can be ignored for this app before it's muted.
constexpr int kIphMuteAfterConsecutiveAppSpecificIgnores = 3;
// Number of times IPH can be ignored for any app before it's muted.
constexpr int kIphMuteAfterConsecutiveAppAgnosticIgnores = 4;
// Number of days to mute IPH after it's ignored for this app.
constexpr int kIphAppSpecificMuteTimeSpanDays = 90;
// Number of days to mute IPH after it's ignored for any app.
constexpr int kIphAppAgnosticMuteTimeSpanDays = 14;
// Default threshold for site engagement score if it's not set by field trial
// param.
constexpr int kIphFieldTrialParamDefaultSiteEngagementThreshold = 10;

// Expected file handler update actions to be taken by OsIntegrationManager
// during UpdateOsHooks.
enum class FileHandlerUpdateAction {
  // Perform update, removing and re-adding all file handlers.
  kUpdate = 0,
  // Remove all file handlers.
  kRemove = 1,
  // Do not perform update.
  kNoUpdate = 2,
};

// Reflects the user's decision to allow or disallow an API such as File
// Handling. APIs should generally start off as kRequiresPrompt.
enum class ApiApprovalState {
  kRequiresPrompt = 0,
  kAllowed = 1,
  kDisallowed = 2,
};

// State concerning whether a particular feature has been enabled at the OS
// level. For example, with File Handling, this indicates whether an app should
// be/has been registered with the OS to handle opening certain file types.
enum class OsIntegrationState {
  kEnabled = 0,
  kDisabled = 1,
};

using LaunchHandler = blink::Manifest::LaunchHandler;

// A result how `WebAppIconDownloader` processed the list of icon urls.
//
// Entries should not be renumbered and numeric values should never be reused.
// Update corresponding enums.xml entry when making changes here.
enum class IconsDownloadedResult {
  // All the requested icon urls have been processed and `icons_map` populated
  // for successful http responses. `icons_http_results` contains success and
  // failure codes. `icons_map` can be empty if every icon url failed,
  kCompleted,
  //
  // There was an error downloading the icons, `icons_map` is empty:
  //
  // Unexpected navigations or state changes on the `web_contents`.
  kPrimaryPageChanged,
  // At least one icon download failed and
  // `WebAppIconDownloader::FailAllIfAnyFail()` flag was specified.
  // `icons_http_results` contains the failed url and http status code.
  kAbortedDueToFailure,
  kMaxValue = kAbortedDueToFailure,
};

const char* IconsDownloadedResultToString(IconsDownloadedResult result);

// Generic result enumeration to be used for operations that can fail. If more
// information is needed in a return value, we can move to something similar to
// `base::FileErrorOr` in the future.
enum class Result {
  // No errors have occurred. This generally means the operation was either
  // completed successfully or possibly intentionally skipped.
  kOk,
  kError
};

using ResultCallback = base::OnceCallback<void(Result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_
