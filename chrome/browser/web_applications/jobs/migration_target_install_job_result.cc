// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/migration_target_install_job_result.h"

#include <ostream>
#include <string>

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         MigrationTargetInstallJobResult result) {
  switch (result) {
    case MigrationTargetInstallJobResult::kAlreadyInstalled:
      os << "kAlreadyInstalled";
      break;
    case MigrationTargetInstallJobResult::kSuccessInstalled:
      os << "kSuccessInstalled";
      break;
    case MigrationTargetInstallJobResult::kManifestToWebAppInstallInfoError:
      os << "kManifestToWebAppInstallInfoError";
      break;
    case MigrationTargetInstallJobResult::kInstallFromInfoFailed:
      os << "kInstallFromInfoFailed";
      break;
    case MigrationTargetInstallJobResult::kUpdateFailed:
      os << "kUpdateFailed";
      break;
    case MigrationTargetInstallJobResult::kWebContentsWasDestroyed:
      os << "kWebContentsWasDestroyed";
      break;
  }
  return os;
}

}  // namespace web_app
