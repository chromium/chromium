// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <iosfwd>
#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-forward.h"

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

// Installations of Web Apps have different sources of management. Apps can be
// installed by different management systems - for example an app can be both
// installed by the user and by policy. Keeping track of which installation
// managers have installed a web app allows for them to be installed by multiple
// managers at the same time, and uninstalls from one manager doesn't affect
// another - the app will stay installed as long as at least one management
// source has it installed.
//
// This enum is also used to rank installation sources, so the ordering matters.
// This enum should be zero based: values are used as index in a bitset.
// We don't use this enum values in prefs or metrics: enumerators can be
// reordered. This enum is not a strongly typed enum class: it supports implicit
// conversion to int and <> comparison operators.
namespace WebAppManagement {
enum Type {
  kMinValue = 0,
  kSystem = kMinValue,
  kIwaShimlessRma,
  // Installed by Kiosk on Chrome OS.
  kKiosk,
  kPolicy,
  kIwaPolicy,
  // Installed by APS (App Preload Service) on ChromeOS as an OEM app.
  kOem,
  kSubApp,
  kWebAppStore,
  kOneDriveIntegration,
  // User-installed web apps are managed by the sync system.or
  // user-installed apps without overlaps this is the only source that will be
  // set.
  kSync,
  kUserInstalled,
  kIwaUserInstalled,
  // Installed by APS (App Preload Service) on ChromeOS as a default app. These
  // have the same UX as kDefault apps, but are are not managed by
  // PreinstalledWebAppManager.
  kApsDefault,
  // This value is used by both the PreinstalledWebAppManager AND the
  // AndroidSmsAppSetupControllerImpl, which is a potential conflict in the
  // future.
  // TODO(dmurph): Add a new source here so that the
  // AndroidSmsAppSetupControllerImpl has its own source, and migrate those
  // installations to have the new source.
  // https://crbug.com/1314055
  kDefault,
  kMaxValue = kDefault,
};

std::ostream& operator<<(std::ostream& os, WebAppManagement::Type type);

bool IsIwaType(WebAppManagement::Type type);

}  // namespace WebAppManagement

using WebAppManagementTypes = base::EnumSet<WebAppManagement::Type,
                                            WebAppManagement::kMinValue,
                                            WebAppManagement::kMaxValue>;

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

  // DEPRECATED: This was used by ApkWebAppInstaller to inject an entry into
  // the now removed ExternallyInstalledWebAppPrefs to track installation of
  // APK installed web apps however over time this enum value came to have no
  // effect. Instead the APK installation is tracked via
  // webapps::WebappUninstallSource::kArc and
  // web_app::WebAppManagement::kWebAppStore.
  // kArc = 4,

  // Installed by Kiosk. There is no call to SynchronizeInstalledApps for this
  // type because Kiosk apps are bound to their profiles. They will never be
  // uninstalled in Kiosk sessions.
  kKiosk = 5,

  // Installed into a special lock screen app profile when the user selects a
  // lock-screen-capable app to be used on the lock screen.
  // The corresponding ExternallyManagedAppManager::SynchronizeInstalledApps
  // call site is in ash::AppManagerImpl::AddAppToLockScreenProfile.
  kExternalLockScreen = 6,

  // Installed through the user-initiated Microsoft 365 setup dialog. There is
  // no call to SynchronizeInstalledApps for this type as these apps are
  // directly installed/uninstalled by the user, rather than being sync'd from
  // somewhere else.
  kInternalMicrosoft365Setup = 7,
};

// Icon size in pixels.
// Small icons are used in confirmation dialogs and app windows.
constexpr int kWebAppIconSmall = 32;

// Limit on the number of jump list entries per web app.
constexpr size_t kMaxApplicationDockMenuItems = 10;

using DisplayMode = blink::mojom::DisplayMode;

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

std::ostream& operator<<(std::ostream& os, RunOnOsLoginMode mode);

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

std::ostream& operator<<(std::ostream& os, ApiApprovalState state);

// TODO(b/274172447): Remove these and the manifest.h include after refactoring
// away blink::Manifest and moving the inner classes to regular classes
using LaunchHandler = blink::Manifest::LaunchHandler;
using TabStrip = blink::Manifest::TabStrip;

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

// Generic result enumeration to be used for operations that can fail. If more
// information is needed in a return value, we can move to something similar to
// `base::FileErrorOr` in the future.
enum class Result {
  // No errors have occurred. This generally means the operation was either
  // completed successfully or possibly intentionally skipped.
  kOk,
  kError
};

#if BUILDFLAG(IS_CHROMEOS)
// Represents the exit states of the PWABubbleView. To be used for CrOS events
// logging.
//
// Do not re-use values.
enum class WebAppInstallStatus : int64_t {
  kCancelled = 0,
  kAccepted = 1,
};
#endif

using ResultCallback = base::OnceCallback<void(Result)>;

// Management types that can be uninstalled by the user.
// Note: These work directly with the `webapps::IsUserUninstall` function - any
// source that returns true there can uninstall these types but not others, and
// will CHECK-fail in RemoveWebAppJob otherwise.
// All WebAppManagement::Types must be listed in either this constant or
// kNotUserUninstallableSources (located in the cc file).
constexpr WebAppManagementTypes kUserUninstallableSources = {
    WebAppManagement::kDefault,
    WebAppManagement::kApsDefault,
    WebAppManagement::kSync,
    WebAppManagement::kUserInstalled,
    WebAppManagement::kWebAppStore,
    WebAppManagement::kSubApp,
    WebAppManagement::kOem,
    WebAppManagement::kOneDriveIntegration,
    WebAppManagement::kIwaUserInstalled,
};

// Management types that resulted from a user web app install.
constexpr WebAppManagementTypes kUserDrivenInstallSources = {
    WebAppManagement::kSync,
    WebAppManagement::kUserInstalled,
    WebAppManagement::kWebAppStore,
    WebAppManagement::kOneDriveIntegration,
    WebAppManagement::kIwaUserInstalled,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CONSTANTS_H_
