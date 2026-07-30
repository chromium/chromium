// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/install_result_code.h"

#include <ostream>

namespace webapps {

bool IsSuccess(InstallResultCode code) {
  switch (code) {
    case InstallResultCode::kSuccessNewInstall:
    case InstallResultCode::kSuccessAlreadyInstalled:
    case InstallResultCode::kSuccessOfflineOnlyInstall:
    case InstallResultCode::kSuccessOfflineFallbackInstall:
      return true;
    case InstallResultCode::kGetWebAppInstallInfoFailed:
    case InstallResultCode::kPreviouslyUninstalled:
    case InstallResultCode::kWebContentsDestroyed:
    case InstallResultCode::kWriteDataFailed:
    case InstallResultCode::kUserInstallDeclined:
    case InstallResultCode::kNotValidManifestForWebApp:
    case InstallResultCode::kIntentToPlayStore:
    case InstallResultCode::kWebAppDisabled:
    case InstallResultCode::kInstallURLRedirected:
    case InstallResultCode::kInstallURLLoadFailed:
    case InstallResultCode::kExpectedAppIdCheckFailed:
    case InstallResultCode::kInstallURLLoadTimeOut:
    case InstallResultCode::kFailedPlaceholderUninstall:
    case InstallResultCode::kNotInstallable:
    case InstallResultCode::kApkWebAppInstallFailed:
    case InstallResultCode::kCancelledOnWebAppProviderShuttingDown:
    case InstallResultCode::kWebAppProviderNotReady:
    case InstallResultCode::kInstallTaskDestroyed:
    case InstallResultCode::kUpdateTaskFailed:
    case InstallResultCode::kAppNotInRegistrarAfterCommit:
    case InstallResultCode::kHaltedBySyncUninstall:
    case InstallResultCode::kInstallURLInvalid:
    case InstallResultCode::kIconDownloadingFailed:
    case InstallResultCode::kCancelledDueToMainFrameNavigation:
    case InstallResultCode::kNoValidIconsInManifest:
    case InstallResultCode::kNoCustomManifestId:
    case InstallResultCode::kManifestIdMismatch:
    case InstallResultCode::kFallbackInstallUsingTrustedIcons:
    case InstallResultCode::kNoValidMigrationSource:
    case InstallResultCode::kInvalidManifestId:
    case InstallResultCode::kInstallAlreadyInProgress:
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
    case InstallResultCode::kNoCustomManifestId:
      return os << "kNoCustomManifestId";
    case InstallResultCode::kManifestIdMismatch:
      return os << "kManifestIdMismatch";
    case InstallResultCode::kFallbackInstallUsingTrustedIcons:
      return os << "kFallbackInstallUsingTrustedIcons";
    case InstallResultCode::kNoValidMigrationSource:
      return os << "kNoValidMigrationSource";
    case InstallResultCode::kInvalidManifestId:
      return os << "kInvalidManifestId";
    case InstallResultCode::kInstallAlreadyInProgress:
      return os << "kInstallAlreadyInProgress";
  }
}

}  // namespace webapps
