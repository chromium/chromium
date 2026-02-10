// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_UPDATE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_UPDATE_JOB_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

namespace web_app {

class WebAppProvider;

// An finalizer job for updates in the installation process.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers the app.
//
// This is a job based on web_app_install_finalizer. At the moment it still
// depends on WebAppInstallFinalizer. When refactoring, this job will stand on
// its own.
class FinalizeUpdateJob {
 public:
  FinalizeUpdateJob(WebAppProvider& provider,
                    WebAppInstallFinalizer& finalizer,
                    const WebAppInstallInfo& web_app_info);
  ~FinalizeUpdateJob();

  void Start(WebAppInstallFinalizer::InstallFinalizedCallback callback);

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

  const raw_ref<WebAppProvider> provider_;
  const raw_ref<WebAppInstallFinalizer> finalizer_;

  WebAppInstallInfo web_app_info_;
  const webapps::AppId app_id_;
  WebAppInstallFinalizer::InstallFinalizedCallback callback_;

  base::WeakPtrFactory<FinalizeUpdateJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_UPDATE_JOB_H_
