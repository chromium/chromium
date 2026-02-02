// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/web_app_install_from_migrate_from_field_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         WebAppInstallFromMigrateFromFieldResult result) {
  switch (result) {
    case WebAppInstallFromMigrateFromFieldResult::kAlreadyInstalled:
      return os << "kAlreadyInstalled";
    case WebAppInstallFromMigrateFromFieldResult::kSuccessInstalled:
      return os << "kSuccessInstalled";
    case WebAppInstallFromMigrateFromFieldResult::kNoSourceAppInstalled:
      return os << "kNoSourceAppInstalled";
    case WebAppInstallFromMigrateFromFieldResult::
        kManifestToWebAppInstallInfoError:
      return os << "kManifestToWebAppInstallInfoError";
    case WebAppInstallFromMigrateFromFieldResult::kInstallFromInfoFailed:
      return os << "kInstallFromInfoFailed";
    case WebAppInstallFromMigrateFromFieldResult::kUpdateFailed:
      return os << "kUpdateFailed";
    case WebAppInstallFromMigrateFromFieldResult::kWebContentsWasDestroyed:
      return os << "kWebContentsWasDestroyed";
    case WebAppInstallFromMigrateFromFieldResult::kUserNavigated:
      return os << "kUserNavigated";
    case WebAppInstallFromMigrateFromFieldResult::kSystemShutdown:
      return os << "kSystemShutdown";
  }
}

}  // namespace web_app
