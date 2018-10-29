// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CONSTANTS_H_

namespace web_app {

// How the app will be launched after installation.
enum class LaunchContainer {
  // When `kDefault` is used, the app will launch in a window if the site is
  // "installable" (also referred to as Progressive Web App) and in a tab if
  // the site is not "installable".
  kDefault,
  kTab,
  kWindow,
};

// The result of an attempted web app installation, uninstallation or update.
//
// This is an enum, instead of a struct with multiple fields (e.g. one field for
// success or failure, one field for whether action was taken), because we want
// to track metrics for the overall cross product of the these axes.
enum class InstallResultCode {
  kSuccess,
  kAlreadyInstalled,
  // Catch-all failure category. More-specific failure categories are below.
  kFailedUnknownReason,
  kGetWebApplicationInfoFailed,
  kPreviouslyUninstalled,
};

// Where an app was installed from. This affects what flags will be used when
// installing the app.
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
// Without the distinction between e.g. kInternal and kExternalXxx, the code
// that manages external-xxx apps might inadvertently uninstall internal apps
// that it otherwise doesn't recognize.
//
// In practice, every kExternalXxx enum definition should correspond to
// exactly one place in the code where SynchronizeInstalledApps is called.
enum class InstallSource {
  // Do not remove or re-order the names, only append to the end. Their
  // integer values are persisted in the preferences.

  kInternal = 0,
  // Installed by default on the system, such as "all such-and-such make and
  // model Chromebooks should have this app installed".
  //
  // The corresponding SynchronizeInstalledApps call site is in
  // WebAppProvider::OnScanForExternalWebApps.
  kExternalDefault = 1,
  // Installed by sys-admin policy, such as "all example.com employees should
  // have this app installed".
  //
  // The corresponding SynchronizeInstalledApps call site is in
  // WebAppPolicyManager::RefreshPolicyInstalledApps.
  kExternalPolicy = 2,
  // Installed as a Chrome component, such as a help app, or a settings app.
  //
  // The corresponding SynchronizeInstalledApps call site is in
  // SystemWebAppManager::RefreshPolicyInstalledApps.
  kSystemInstalled = 3,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_CONSTANTS_H_
