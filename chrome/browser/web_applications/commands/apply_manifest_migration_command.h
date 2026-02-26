// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_MANIFEST_MIGRATION_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_MANIFEST_MIGRATION_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/gather_migration_source_info_job.h"
#include "chrome/browser/web_applications/jobs/gather_migration_source_info_job_result.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/scheduler/apply_manifest_migration_result.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace apps {
enum class LaunchContainer;
}  // namespace apps

namespace web_app {

class RemoveInstallSourceJob;

struct SynchronizeOsOptions;

// This command executes the core logic for a manifest migration in the
// following order:
// 1. Finalize the installation of the new application (|destination_app_id|),
//    ensuring OS integration matches the old app.
// 2. Uninstall the old application (|source_app_id|).
// 3. Update sync data to link the new app to the old app.
// 4. Ensure UX consistency (name and icons) for forced migrations.
class ApplyManifestMigrationCommand
    : public WebAppCommand<AllAppsLock, ApplyManifestMigrationResult> {
 public:
  ApplyManifestMigrationCommand(
      const webapps::AppId& source_app_id,
      const webapps::AppId& destination_app_id,
      MigrationBehavior migration_behavior,
      Profile* profile,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ApplyManifestMigrationResultCallback callback);
  ~ApplyManifestMigrationCommand() override;

  // WebAppCommand overrides:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  // For forced migrations, start the OS integration process on successful
  // copying of icons on disk.
  void OnIconsCopied(bool success);
  void OnMigrationSourceInfoGathered(
      std::optional<GatherMigrationSourceInfoJobResult> migration_state);

  // Synchronizes OS integration for the destination app, and uninstalls the
  // source app consequently.
  void SynchronizeOsIntegration(SynchronizeOsOptions os_options);
  // Set up the destination app so that it appears to be "migrated" as seen from
  // the sync system, and uninstall the source app.
  void SetupDestinationAppUninstallSourceApp();
  void AppUninstalledCompleteMigration(webapps::UninstallResultCode code);

  void OnAppLaunched(base::WeakPtr<Browser> browser,
                     base::WeakPtr<content::WebContents> web_contents,
                     apps::LaunchContainer container,
                     base::Value debug_value);

  void CompleteCommandAndSelfDestruct(ApplyManifestMigrationResult result);

  const webapps::AppId source_app_id_;
  const webapps::AppId destination_app_id_;
  const MigrationBehavior migration_behavior_;
  const raw_ptr<Profile> profile_ = nullptr;
  // KeepAlive objects are needed to make sure that migration completes, even
  // when the browser windows have been closed.
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<AllAppsLock> all_apps_lock_;
  std::unique_ptr<RemoveInstallSourceJob> remove_source_app_job_;
  std::unique_ptr<GatherMigrationSourceInfoJob>
      gather_migration_source_info_job_;

  base::WeakPtrFactory<ApplyManifestMigrationCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_APPLY_MANIFEST_MIGRATION_COMMAND_H_
