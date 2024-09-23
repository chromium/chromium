// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/install_result_code.h"

#include <ostream>

namespace webapps {

bool IsSuccess(InstallResultCode code) {
  // TODO(crbug.com/40821686): enumerate all the constants instead of the
  // default clause to prevent accidentally implicitly returning false on any
  // newly added value.
  switch (code) {
    case InstallResultCode::kSuccessNewInstall:
    case InstallResultCode::kSuccessAlreadyInstalled:
    case InstallResultCode::kSuccessOfflineOnlyInstall:
    case InstallResultCode::kSuccessOfflineFallbackInstall:
      return true;
    default:
      return false;
  }
}

bool IsNewInstall(InstallResultCode code) {
  return IsSuccess(code) && code != InstallResultCode::kSuccessAlreadyInstalled;
}

std::ostream& operator<<(std::ostream& os, InstallResultCode code) {
  switch (code) {
    case InstallResultCode::kSuccessNewInstall:
      return os << "kSuccessNewInstall";
    case InstallResultCode::kSuccessAlreadyInstalled:
      return os << "kSuccessAlreadyInstalled";
    case InstallResultCode::kGetWebAppInstallInfoFailed:
      return os << "kGetWebAppInstallInfoFailed";
    case InstallResultCode::kPreviouslyUninstalled:
      return os << "kPreviouslyUninstalled";
    case InstallResultCode::kWebContentsDestroyed:
      return os << "kWebContentsDestroyed";
    case InstallResultCode::kInstallTaskDestroyed:
      return os << "kInstallTaskDestroyed";
    case InstallResultCode::kWriteDataFailed:
      return os << "kWriteDataFailed";
    case InstallResultCode::kUserInstallDeclined:
      return os << "kUserInstallDeclined";
    case InstallResultCode::kNotValidManifestForWebApp:
      return os << "kNotValidManifestForWebApp";
    case InstallResultCode::kIntentToPlayStore:
      return os << "kIntentToPlayStore";
    case InstallResultCode::kWebAppDisabled:
      return os << "kWebAppDisabled";
    case InstallResultCode::kInstallURLRedirected:
      return os << "kInstallURLRedirected";
    case InstallResultCode::kInstallURLLoadFailed:
      return os << "kInstallURLLoadFailed";
    case InstallResultCode::kExpectedAppIdCheckFailed:
      return os << "kExpectedAppIdCheckFailed";
    case InstallResultCode::kInstallURLLoadTimeOut:
      return os << "kInstallURLLoadTimeOut";
    case InstallResultCode::kFailedPlaceholderUninstall:
      return os << "kFailedPlaceholderUninstall";
    case InstallResultCode::kNotInstallable:
      return os << "kNotInstallable";
    case InstallResultCode::kApkWebAppInstallFailed:
      return os << "kApkWebAppInstallFailed";
    case InstallResultCode::kCancelledOnWebAppProviderShuttingDown:
      return os << "kCancelledOnWebAppProviderShuttingDown";
    case InstallResultCode::kWebAppProviderNotReady:
      return os << "kWebAppProviderNotReady";
    case InstallResultCode::kSuccessOfflineOnlyInstall:
      return os << "kSuccessOfflineOnlyInstall";
    case InstallResultCode::kSuccessOfflineFallbackInstall:
      return os << "kSuccessOfflineFallbackInstall";
    case InstallResultCode::kUpdateTaskFailed:
      return os << "kUpdateTaskFailed";
    case InstallResultCode::kAppNotInRegistrarAfterCommit:
      return os << "kAppNotInRegistrarAfterCommit";
    case InstallResultCode::kHaltedBySyncUninstall:
      return os << "kHaltedBySyncUninstall";
    case InstallResultCode::kInstallURLInvalid:
      return os << "kInstallURLInvalid";
    case InstallResultCode::kIconDownloadingFailed:
      return os << "kIconDownloadingFailed";
    case InstallResultCode::kCancelledDueToMainFrameNavigation:
      return os << "kCancelledDueToMainFrameNavigation";
    case InstallResultCode::kNoValidIconsInManifest:
      return os << "kNoValidIconsInManifest";
  }
}

}  // namespace webapps
