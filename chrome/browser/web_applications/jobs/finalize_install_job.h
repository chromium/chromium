// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_management_type.h"

class Profile;

namespace web_app {

class WebAppProvider;

// A finalizer job for the installation process, represents the last step.
// Takes WebAppInstallInfo as input, writes data to disk (e.g icons, shortcuts)
// and registers the app.
//
// This is a job based on web_app_install_finalizer. At the moment it still
// depends on WebAppInstallFinalizer. When refactoring, this job will stand on
// its own.
class FinalizeInstallJob {
 public:
  FinalizeInstallJob(Profile& profile,
                     WebAppProvider& provider,
                     base::Clock* clock,
                     WebAppInstallFinalizer& finalizer,
                     const WebAppInstallInfo& web_app_info,
                     const WebAppInstallFinalizer::FinalizeOptions& options);
  ~FinalizeInstallJob();

  void Start(WebAppInstallFinalizer::InstallFinalizedCallback callback);

  Profile* profile() { return &profile_.get(); }

  // Writes external config data to the web_app DB, mapped per source.
  void WriteExternalConfigMapInfo(
      WebApp& web_app,
      WebAppManagement::Type source,
      bool is_placeholder,
      GURL install_url,
      std::vector<std::string> additional_policy_ids);

  static void AdjustAppStateBeforeCommit(const WebApp* existing_app,
                                         WebApp& web_app,
                                         WebAppProvider& provider);

 private:
  friend class WebAppInstallFinalizer;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  void SetWebAppManifestFieldsAndWriteData(
      std::unique_ptr<WebApp> web_app,
      CommitCallback commit_callback,
      bool skip_icon_writes_on_download_failure);

  static void UpdateIsolationDataAndResetPendingUpdateInfo(
      WebApp* web_app,
      const IsolatedWebAppStorageLocation& location,
      const IwaVersion& version,
      const std::optional<GURL>& iwa_update_manifest_url,
      std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data);

  void CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                          CommitCallback commit_callback,
                          bool success);

  void WriteTranslations(
      const webapps::AppId& app_id,
      const base::flat_map<std::string, blink::Manifest::TranslationItem>&
          translations,
      CommitCallback commit_callback,
      bool success);

  void OnOriginAssociationValidated(
      OriginAssociations validated_origin_associations);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;
  raw_ptr<base::Clock> clock_;
  const raw_ref<WebAppInstallFinalizer> finalizer_;

  WebAppInstallInfo web_app_info_;
  WebAppInstallFinalizer::FinalizeOptions options_;
  WebAppInstallFinalizer::InstallFinalizedCallback callback_;

  base::WeakPtrFactory<FinalizeInstallJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_FINALIZE_INSTALL_JOB_H_
