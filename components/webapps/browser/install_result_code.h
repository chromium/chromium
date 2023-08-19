// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALL_RESULT_CODE_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALL_RESULT_CODE_H_

#include <iosfwd>

namespace webapps {

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
  // An inter-process request to blink renderer failed.
  kGetWebAppInstallInfoFailed = 3,
  // A user previously uninstalled the app, user doesn't want to see it again.
  kPreviouslyUninstalled = 4,
  // The blink renderer used to install the app was destroyed.
  kWebContentsDestroyed = 5,
  // I/O error: Disk output failed.
  kWriteDataFailed = 6,
  // A user rejected installation prompt.
  kUserInstallDeclined = 7,
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
  // The network request for the install URL failed.
  kInstallURLLoadFailed = 14,
  // The requested app_id check failed: actual resulting app_id doesn't match.
  kExpectedAppIdCheckFailed = 15,
  // The network request for the install URL timed out.
  kInstallURLLoadTimeOut = 16,
  // Placeholder uninstall fails (in ExternallyManagedAppManager).
  kFailedPlaceholderUninstall = 17,
  // Web App is not considered installable, i.e. missing manifest fields, no
  // service worker, etc.
  kNotInstallable = 18,
  // Apk Web App install fails.
  kApkWebAppInstallFailed = 20,
  // App managers are shutting down. For example, when user logs out immediately
  // after login.
  kCancelledOnWebAppProviderShuttingDown = 21,
  // The Web Apps system is not ready: registry is not yet opened or already
  // closed.
  kWebAppProviderNotReady = 22,

  // Success category for background installs:
  kSuccessOfflineOnlyInstall = 23,
  kSuccessOfflineFallbackInstall = 24,

  // Failure category:
  // The install task was destroyed, most likely due to WebAppInstallManager
  // shutdown.
  kInstallTaskDestroyed = 25,

  // Web App update due to manifest change failed.
  kUpdateTaskFailed = 26,

  // Web App was not present in the registrar after a successful database
  // commit.
  kAppNotInRegistrarAfterCommit = 27,

  // The installation stopped due to an uninstall from sync being scheduled.
  kHaltedBySyncUninstall = 28,

  // Invalid install URL for externally managed apps.
  kInstallURLInvalid = 29,

  // Downloading failed for all icons in an installation method which requires
  // non-generated icons.
  kIconDownloadingFailed = 30,

  kCancelledDueToMainFrameNavigation = 31,

  // No valid icons were provided in the manifest in an installation method
  // which requires non-generated icons.
  kNoValidIconsInManifest = 32,

  kMaxValue = kNoValidIconsInManifest,
};

// Checks if InstallResultCode is not a failure.
bool IsSuccess(InstallResultCode code);

// Checks if InstallResultCode indicates a new app was installed.
bool IsNewInstall(InstallResultCode code);

std::ostream& operator<<(std::ostream& os, InstallResultCode code);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALL_RESULT_CODE_H_
