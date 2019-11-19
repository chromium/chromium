// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CONSTANTS_H_

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
  kWebAppStore,
  // We sync only regular user-installed apps from the open web. For
  // user-installed apps without overlaps this is the only source that will be
  // set.
  kSync,
  kDefault,
  kMaxValue
};
}  // namespace Source

// The result of an attempted web app installation, uninstallation or update.
//
// This is an enum, instead of a struct with multiple fields (e.g. one field for
// success or failure, one field for whether action was taken), because we want
// to track metrics for the overall cross product of the these axes.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Update corresponding enums.xml entry
// when making changes here.
enum class InstallResultCode {
  // Success category:
  kSuccessNewInstall = 0,
  kSuccessAlreadyInstalled = 1,
  // Failure category:
  kFailedUnknownReason = 2,
  // An inter-process request to blink renderer failed.
  kGetWebApplicationInfoFailed = 3,
  // A user previously uninstalled the app, user doesn't want to see it again.
  kPreviouslyUninstalled = 4,
  // The blink renderer used to install the app was destroyed.
  kWebContentsDestroyed = 5,
  // I/O error: Disk output failed.
  kWriteDataFailed = 6,
  // A user rejected installation prompt.
  kUserInstallDeclined = 7,
  // A whole user profile was destroyed during installation.
  kProfileDestroyed = 8,
  // |require_manifest| was specified but the app had no valid manifest.
  kNotValidManifestForWebApp = 10,
  // We have terminated the installation pipeline and intented to the Play
  // Store, where the user still needs to accept the Play installation prompt to
  // install.
  kIntentToPlayStore = 11,
  // A web app has been disabled by device policy or by other reasons.
  kWebAppDisabled = 12,
  // The network request for the install URL was redirected.
  kInstallURLRedirected = 13,
  // The network request for the install URL failed or timed out.
  kInstallURLLoadFailed = 14,
  // The requested app_id check failed: actual resulting app_id doesn't match.
  kExpectedAppIdCheckFailed = 15,
  kMaxValue = kExpectedAppIdCheckFailed
};

// Checks if InstallResultCode is not a failure.
bool IsSuccess(InstallResultCode code);

// PendingAppManager: Where an app was installed from. This affects what flags
// will be used when installing the app.
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
// PendingAppManager::SynchronizeInstalledApps is called.
enum class ExternalInstallSource {
  // Do not remove or re-order the names, only append to the end. Their
  // integer values are persisted in the preferences.

  // Installed by default on the system from the C++ code. AndroidSms app is an
  // example.
  kInternalDefault = 0,

  // Installed by default on the system, such as "all such-and-such make and
  // model Chromebooks should have this app installed".
  // The corresponding PendingAppManager::SynchronizeInstalledApps call site is
  // in WebAppProvider::OnScanForExternalWebApps.
  kExternalDefault = 1,

  // Installed by sys-admin policy, such as "all example.com employees should
  // have this app installed".
  // The corresponding PendingAppManager::SynchronizeInstalledApps call site is
  // in WebAppPolicyManager::RefreshPolicyInstalledApps.
  kExternalPolicy = 2,

  // Installed as a Chrome component, such as a help app, or a settings app.
  // The corresponding PendingAppManager::SynchronizeInstalledApps call site is
  // in SystemWebAppManager::RefreshPolicyInstalledApps.
  kSystemInstalled = 3,

  // Installed from ARC.
  // There is no call to SynchronizeInstalledApps for this type, as these apps
  // are not installed via PendingAppManager. This is used in
  // ExternallyInstalledWebAppPrefs to track navigation url to app_id entries.
  kArc = 4,
};

using DisplayMode = blink::mojom::DisplayMode;

// When user_display_mode indicates a user preference for opening in
// a browser tab, we open in a browser tab. Otherwise, we open in a standalone
// window (for app_display_mode 'standalone' or 'fullscreen'), or a minimal-ui
// window (for app_display_mode 'browser' or 'minimal-ui').
DisplayMode ResolveEffectiveDisplayMode(DisplayMode app_display_mode,
                                        DisplayMode user_display_mode);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CONSTANTS_H_
