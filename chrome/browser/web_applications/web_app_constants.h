// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_

#include <stddef.h>

#include <iosfwd>
#include <vector>

#include "chrome/browser/web_applications/user_display_mode.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace web_app {

// Installations of Web Apps have different sources of management. Apps can be
// installed by different management systems - for example an app can be both
// installed by the user and by policy. Keeping track of the which installation
// managers have installed a web app allows for them to be installed by multiple
// at the same time, and uninstalls from one manager doesn't affect another -
// the app will stay installed as long as at least one management source has it
// installed.
//
// This enum is also used to rank installation sources, so the ordering matters.
// This enum should be zero based: values are used as index in a bitset.
// We don't use this enum values in prefs or metrics: enumerators can be
// reordered. This enum is not strongly typed enum class: it supports implicit
// conversion to int and <> comparison operators.
namespace WebAppManagement {
enum Type {
  kMinValue = 0,
  kSystem = kMinValue,
  kPolicy,
  kSubApp,
  kWebAppStore,
  // User-installed web apps are managed by the sync system.or
  // user-installed apps without overlaps this is the only source that will be
  // set.
  kSync,
  // This value is used by both the PreinstalledWebAppManager AND the
  // AndroidSmsAppSetupControllerImpl, which is a potential conflict in the
  // future.
  // TODO(dmurph): Add a new source here so that the
  // AndroidSmsAppSetupControllerImpl has it's own source, and migrate those
  // installations to have the new source.
  // https://crbug.com/1314055
  kDefault,
  kMaxValue = kDefault,
};
}  // namespace WebAppManagement

std::ostream& operator<<(std::ostream& os, WebAppManagement::Type type);

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
// TODO(dmurph): Remove this and merge it into WebAppManagement after it has a
// new source for the  AndroidSmsAppSetupControllerImpl.
// https://crbug.com/1314055
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
  // in ash::SystemWebAppManager::RefreshPolicyInstalledApps.
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

// Limit on the number of jump list entries per web app.
constexpr size_t kMaxApplicationDockMenuItems = 10;

using DisplayMode = blink::mojom::DisplayMode;

// When user_display_mode indicates a user preference for opening in
// a browser tab, we open in a browser tab. If the developer has specified
// the app should utilize more advanced display modes and/or fallback chain,
// attempt honor those preferences. Otherwise, we open in a standalone
// window (for app_display_mode 'standalone' or 'fullscreen'), or a minimal-ui
// window (for app_display_mode 'browser' or 'minimal-ui').
//
// |is_isolated| overrides browser display mode for isolated apps because they
// can't be open as a tab.
DisplayMode ResolveEffectiveDisplayMode(
    DisplayMode app_display_mode,
    const std::vector<DisplayMode>& app_display_mode_overrides,
    UserDisplayMode user_display_mode,
    bool is_isolated);

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

// Converts RunOnOsLoginMode from web_app::RunOnOsLoginMode to
// apps::RunOnOsLoginMode.
apps::RunOnOsLoginMode ConvertOsLoginMode(web_app::RunOnOsLoginMode login_mode);

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
