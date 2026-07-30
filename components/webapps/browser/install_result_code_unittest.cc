// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/install_result_code.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

TEST(InstallResultCodeTest, IsSuccess) {
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessNewInstall));
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessAlreadyInstalled));
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessOfflineOnlyInstall));
  EXPECT_TRUE(IsSuccess(InstallResultCode::kSuccessOfflineFallbackInstall));

  EXPECT_FALSE(IsSuccess(InstallResultCode::kGetWebAppInstallInfoFailed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kPreviouslyUninstalled));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kWebContentsDestroyed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kWriteDataFailed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kUserInstallDeclined));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kNotValidManifestForWebApp));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kIntentToPlayStore));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kWebAppDisabled));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInstallURLRedirected));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInstallURLLoadFailed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kExpectedAppIdCheckFailed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInstallURLLoadTimeOut));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kFailedPlaceholderUninstall));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kNotInstallable));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kApkWebAppInstallFailed));
  EXPECT_FALSE(
      IsSuccess(InstallResultCode::kCancelledOnWebAppProviderShuttingDown));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kWebAppProviderNotReady));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInstallTaskDestroyed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kUpdateTaskFailed));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kAppNotInRegistrarAfterCommit));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kHaltedBySyncUninstall));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInstallURLInvalid));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kIconDownloadingFailed));
  EXPECT_FALSE(
      IsSuccess(InstallResultCode::kCancelledDueToMainFrameNavigation));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kNoValidIconsInManifest));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kNoCustomManifestId));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kManifestIdMismatch));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kFallbackInstallUsingTrustedIcons));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kNoValidMigrationSource));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInvalidManifestId));
  EXPECT_FALSE(IsSuccess(InstallResultCode::kInstallAlreadyInProgress));
}

}  // namespace webapps
