// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_MIGRATE_TO_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_MIGRATE_TO_APP_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/scheduler/install_migrate_to_app_result.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace base {
class DictValue;
}

namespace content {
class WebContents;
}

namespace web_app {

enum class MigrationTargetInstallJobResult;
enum class ManifestUpdateJobResult;
class ManifestUpdateJobResultWithTimestamp;
class MigrationTargetInstallJob;
class ManifestUpdateJob;
class WebAppDataRetriever;

// This command is triggered when a manifest containing a `migrate_to` field is
// encountered. The contents of the `migrate_to` field (i.e. manifest id and
// install url) are passed in to the command, along with the manifest id of the
// source app.
// This command starts out with just a shared web contents lock and will:
//
// * Navigate shared web contents to install url
// * Get first available manifest from install url
// * Verify manifest id and `migrate_from` field are present and match the
//   expected values.
// * Acquire a lock for the migration target app (i.e. the manifest id for the
//   manifest we parsed)
//   * If the migration target is already installed:
//     * Trigger ManifestUpdateJob to update the migration target app. If the
//       install state is `SUGGESTED_FROM_MIGRATION` this will update all
//       fields, otherwise this will only update the non-security sensitive
//       fields. Either way, the install finalizer will kick off a
//       `ResolveWebAppPendingMigrationInfoCommand` if any validated migration
//       sources are changed.
//   Else:
//     * Trigger a `MigrationTargetInstallJob` given the parsed manifest and
//       current web contents
class InstallMigrateToAppCommand
    : public WebAppCommand<SharedWebContentsLock, InstallMigrateToAppResult> {
 public:
  InstallMigrateToAppCommand(const webapps::ManifestId& source_manifest_id,
                             const webapps::ManifestId& target_manifest_id,
                             const GURL& target_install_url,
                             Profile* profile,
                             InstallMigrateToAppResultCallback callback);
  ~InstallMigrateToAppCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  void OnUrlLoaded(webapps::WebAppUrlLoaderResult result);
  void OnManifestFetched(blink::mojom::ManifestPtr manifest,
                         bool valid_manifest_for_web_app,
                         webapps::InstallableStatusCode installable_status);
  void OnAppLockAcquired();

  void OnInstallJobFinished(MigrationTargetInstallJobResult result);
  void OnUpdateJobFinished(ManifestUpdateJobResultWithTimestamp result);

  const webapps::ManifestId source_manifest_id_;
  const webapps::ManifestId target_manifest_id_;
  const GURL target_install_url_;

  blink::mojom::ManifestPtr manifest_;

  const raw_ptr<Profile> profile_;

  std::unique_ptr<SharedWebContentsLock> shared_web_contents_lock_;
  std::unique_ptr<SharedWebContentsWithAppLock>
      shared_web_contents_with_app_lock_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<MigrationTargetInstallJob> install_job_;
  std::unique_ptr<ManifestUpdateJob> update_job_;

  base::WeakPtrFactory<InstallMigrateToAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_MIGRATE_TO_APP_COMMAND_H_
