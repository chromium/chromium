// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_UPDATE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_UPDATE_JOB_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

namespace web_app {

class IwaVersion;
class WebAppProvider;
class WebAppScope;
struct OriginAssociations;

// An finalizer job for updates in the installation process.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers the app.
//
// This is a job based on web_app_install_finalizer. It is currently only
// triggered by web_app_install_finalizer until refactoring is complete.
class FinalizeUpdateJob {
 public:
  FinalizeUpdateJob(WebAppProvider& provider,
                    const WebAppInstallInfo& web_app_info);
  ~FinalizeUpdateJob();

  void Start(InstallFinalizedCallback callback);

 private:
  friend class WebAppInstallFinalizer;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  void OnOriginAssociationValidatedForUpdate(
      OriginAssociations validated_origin_associations);

  void OnDatabaseCommitCompletedForUpdate(
      std::string old_name,
      FileHandlerUpdateAction file_handlers_need_os_update,
      std::optional<WebAppScope> old_scope,
      bool success);

  void OnUpdateHooksFinished();

  // Returns a value indicating whether the file handlers registered with the OS
  // should be updated. Used to avoid unnecessary updates. TODO(estade): why
  // does this optimization exist when other OS hooks don't have similar
  // optimizations?
  FileHandlerUpdateAction GetFileHandlerUpdateAction();

  void UpdateIsolationDataAndResetPendingUpdateInfo(
      WebApp* web_app,
      const IsolatedWebAppStorageLocation& location,
      const IwaVersion& version,
      const std::optional<GURL>& iwa_update_manifest_url,
      std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);

  void SetWebAppManifestFieldsAndWriteData(
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback,
      bool skip_icon_writes_on_download_failure);

  void WriteTranslations(
      const webapps::AppId& app_id,
      const base::flat_map<std::string, blink::Manifest::TranslationItem>&
          translations,
      CommitCallback commit_callback,
      bool success);

  void CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                          CommitCallback commit_callback,
                          bool success);

  const raw_ref<WebAppProvider> provider_;

  WebAppInstallInfo web_app_info_;
  const webapps::AppId app_id_;
  InstallFinalizedCallback callback_;

  base::WeakPtrFactory<FinalizeUpdateJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_UPDATE_JOB_H_
