// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MIGRATION_TARGET_INSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MIGRATION_TARGET_INSTALL_JOB_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job_result.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

class Profile;

namespace base {
class DictValue;
}

namespace content {
class WebContents;
}

namespace web_app {

struct WebAppInstallInfo;
class Lock;
class WithAppResources;
class ManifestToWebAppInstallInfoJob;
class InstallFromInfoJob;
class WebAppDataRetriever;

//  Given a manifest (and a web contents) and requires a lock for the (migration
//  target) app this manifest represents:
//
// * If the migration target is already installed:
//   * No-op (returns `kAlreadyInstalled`), as updates are handled by
//     `ManifestSilentUpdateCommand`.
// * Else:
//   * Run `ManifestToWebAppInstallInfoJob` to convert manifest to install info
//     and fetch icons
//   * Run `InstallFromInfoJob` with `SUGGESTED_FROM_MIGRATION` install state to
//     install the to-be-migrated-to app
//     * The install finalizer will kick off a
//       `ResolveWebAppPendingMigrationInfoCommand` as needed.
class MigrationTargetInstallJob {
 public:
  static std::unique_ptr<MigrationTargetInstallJob> CreateAndStart(
      blink::mojom::ManifestPtr manifest,
      base::WeakPtr<content::WebContents> web_contents,
      Profile* profile,
      WebAppDataRetriever* data_retriever,
      base::DictValue* debug_value,
      Lock* lock,
      WithAppResources* lock_resources,
      MigrationTargetInstallCallback callback);

  ~MigrationTargetInstallJob();

 private:
  MigrationTargetInstallJob(blink::mojom::ManifestPtr manifest,
                            base::WeakPtr<content::WebContents> web_contents,
                            Profile* profile,
                            WebAppDataRetriever* data_retriever,
                            base::DictValue* debug_value,
                            Lock* lock,
                            WithAppResources* lock_resources,
                            MigrationTargetInstallCallback callback);

  void Start();

  void OnManifestToWebAppInstallInfoJobFinished(
      std::unique_ptr<WebAppInstallInfo> install_info);
  void OnInstallFromInfoJobFinished(webapps::AppId app_id,
                                    webapps::InstallResultCode code);

  void Complete(MigrationTargetInstallJobResult result);

  blink::mojom::ManifestPtr manifest_;
  const base::WeakPtr<content::WebContents> web_contents_;
  const raw_ptr<Profile> profile_;
  const raw_ref<WebAppDataRetriever> data_retriever_;
  const raw_ref<base::DictValue> debug_value_;
  const raw_ref<Lock> lock_;
  const raw_ref<WithAppResources> lock_resources_;
  MigrationTargetInstallCallback callback_;

  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  std::unique_ptr<InstallFromInfoJob> install_from_info_job_;

  base::WeakPtrFactory<MigrationTargetInstallJob> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MIGRATION_TARGET_INSTALL_JOB_H_
