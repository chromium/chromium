// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/install_migrate_to_app_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, InstallMigrateToAppResult result) {
  switch (result) {
    case InstallMigrateToAppResult::kSuccessNewInstall:
      return os << "SuccessNewInstall";
    case InstallMigrateToAppResult::kSuccessAlreadyInstalled:
      return os << "SuccessAlreadyInstalled";
    case InstallMigrateToAppResult::kInstallError:
      return os << "InstallError";
    case InstallMigrateToAppResult::kWebContentsDestroyed:
      return os << "WebContentsDestroyed";
    case InstallMigrateToAppResult::kShutdown:
      return os << "Shutdown";
    case InstallMigrateToAppResult::kIconDownloadFailed:
      return os << "IconDownloadFailed";
    case InstallMigrateToAppResult::kIconReadFromDiskFailed:
      return os << "IconReadFromDiskFailed";
    case InstallMigrateToAppResult::kIconWriteToDiskFailed:
      return os << "IconWriteToDiskFailed";
    case InstallMigrateToAppResult::kInstallFinalizeFailed:
      return os << "InstallFinalizeFailed";
    case InstallMigrateToAppResult::kManifestConversionFailed:
      return os << "ManifestConversionFailed";
    case InstallMigrateToAppResult::kAppNotAllowedToUpdate:
      return os << "AppNotAllowedToUpdate";
    case InstallMigrateToAppResult::kUserNavigated:
      return os << "UserNavigated";
    case InstallMigrateToAppResult::kManifestToWebAppInstallInfoError:
      return os << "ManifestToWebAppInstallInfoError";
    case InstallMigrateToAppResult::kInstallFromInfoFailed:
      return os << "InstallFromInfoFailed";
    case InstallMigrateToAppResult::kUpdateFailed:
      return os << "UpdateFailed";
    case InstallMigrateToAppResult::kUrlLoadFailed:
      return os << "UrlLoadFailed";
    case InstallMigrateToAppResult::kManifestIdMismatch:
      return os << "ManifestIdMismatch";
    case InstallMigrateToAppResult::kMigrateFromMismatch:
      return os << "MigrateFromMismatch";
  }
}

}  // namespace web_app
