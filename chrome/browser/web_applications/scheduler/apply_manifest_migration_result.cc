// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/apply_manifest_migration_result.h"

#include <ostream>

#include "chrome/browser/web_applications/commands/command_result.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         ApplyManifestMigrationResult result) {
  switch (result) {
    case ApplyManifestMigrationResult::kSystemShutdown:
      return os << "SystemShutdown";
    case ApplyManifestMigrationResult::kSourceAppInvalidForMigration:
      return os << "SourceAppNotInstalled";
    case ApplyManifestMigrationResult::kDestinationAppInvalid:
      return os << "DestinationAppInvalid";
    case ApplyManifestMigrationResult::kUnableToRemoveSourceApp:
      return os << "UnableToRemoveSourceApp";
    case ApplyManifestMigrationResult::kDestinationAppDoesNotLinkToSourceApp:
      return os << "DesinationAppDoesNotLinkToSourceApp";
    case ApplyManifestMigrationResult::kAppMigrationAppliedSuccessfully:
      return os << "AppMigrationAppliedSuccessfully";
    case ApplyManifestMigrationResult::kAppMigrationFailedDuringIconCopy:
      return os << "AppMigrationFailedDuringIconCopy";
    case ApplyManifestMigrationResult::
        kAppMigrationAppliedSuccessfullyLaunchFailed:
      return os << "AppMigrationAppliedSuccessfullyLaunchFailed";
  }
}

}  // namespace web_app
