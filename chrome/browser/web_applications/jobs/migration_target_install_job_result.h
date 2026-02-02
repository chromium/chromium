// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MIGRATION_TARGET_INSTALL_JOB_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MIGRATION_TARGET_INSTALL_JOB_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

enum class MigrationTargetInstallJobResult {
  // The target app was already installed.
  kAlreadyInstalled,
  // The target app was not installed, and it was successfully installed with
  // the SUGGESTED_FROM_MIGRATION state.
  kSuccessInstalled,
  // The manifest could not be converted to WebAppInstallInfo.
  kManifestToWebAppInstallInfoError,
  // The install from info job failed.
  kInstallFromInfoFailed,
  // The manifest update job failed.
  kUpdateFailed,
  // The web contents was destroyed before the job could complete.
  kWebContentsWasDestroyed,
};

std::ostream& operator<<(std::ostream& os,
                         MigrationTargetInstallJobResult result);

using MigrationTargetInstallCallback =
    base::OnceCallback<void(MigrationTargetInstallJobResult result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MIGRATION_TARGET_INSTALL_JOB_RESULT_H_
