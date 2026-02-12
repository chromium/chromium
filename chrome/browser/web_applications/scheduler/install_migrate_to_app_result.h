// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_INSTALL_MIGRATE_TO_APP_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_INSTALL_MIGRATE_TO_APP_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

// This enum is recorded by UMA, the numeric values must not change.
// LINT.IfChange(InstallMigrateToAppResult)
enum class InstallMigrateToAppResult {
  kSuccessNewInstall = 0,
  kSuccessAlreadyInstalled = 1,
  kInstallError = 2,
  kWebContentsDestroyed = 3,
  kShutdown = 4,
  kIconDownloadFailed = 5,
  kIconReadFromDiskFailed = 6,
  kIconWriteToDiskFailed = 7,
  kInstallFinalizeFailed = 8,
  kManifestConversionFailed = 9,
  kAppNotAllowedToUpdate = 10,
  kUserNavigated = 11,
  kManifestToWebAppInstallInfoError = 12,
  kInstallFromInfoFailed = 13,
  kUpdateFailed = 14,
  kUrlLoadFailed = 15,
  kManifestIdMismatch = 16,
  kMigrateFromMismatch = 17,
  kMaxValue = kMigrateFromMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppInstallMigrateToAppResult)

std::ostream& operator<<(std::ostream& os, InstallMigrateToAppResult result);

using InstallMigrateToAppResultCallback =
    base::OnceCallback<void(InstallMigrateToAppResult result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_INSTALL_MIGRATE_TO_APP_RESULT_H_
