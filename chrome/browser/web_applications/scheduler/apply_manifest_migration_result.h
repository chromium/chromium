// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_APPLY_MANIFEST_MIGRATION_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_APPLY_MANIFEST_MIGRATION_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/commands/command_result.h"

namespace web_app {

// This enum is recorded by UMA, the numeric values must not change.
// LINT.IfChange(ApplyManifestMigrationResult)
enum class ApplyManifestMigrationResult {
  kSystemShutdown = 0,
  kSourceAppInvalidForMigration = 1,
  kDestinationAppInvalid = 2,
  kUnableToRemoveSourceApp = 3,
  kDestinationAppDoesNotLinkToSourceApp = 4,
  kAppMigrationAppliedSuccessfully = 5,
  kAppMigrationFailedDuringIconCopy = 6,
  kAppMigrationAppliedSuccessfullyLaunchFailed = 7,
  kMaxValue = kAppMigrationAppliedSuccessfullyLaunchFailed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppApplyManifestMigrationResult)

std::ostream& operator<<(std::ostream& os, ApplyManifestMigrationResult result);

using ApplyManifestMigrationResultCallback =
    base::OnceCallback<void(ApplyManifestMigrationResult migration_result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_APPLY_MANIFEST_MIGRATION_RESULT_H_
