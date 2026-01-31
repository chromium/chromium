// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/migration_target_install_job.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/jobs/install_from_info_job.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job_result.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

std::unique_ptr<MigrationTargetInstallJob>
MigrationTargetInstallJob::CreateAndStart(
    blink::mojom::ManifestPtr manifest,
    base::WeakPtr<content::WebContents> web_contents,
    Profile* profile,
    WebAppDataRetriever* data_retriever,
    base::Clock* clock,
    base::DictValue* debug_value,
    WithAppResources* lock,
    MigrationTargetInstallCallback callback) {
  CHECK(manifest);
  CHECK(web_contents);
  CHECK(profile);
  CHECK(debug_value);
  CHECK(lock);
  CHECK(data_retriever);

  auto job = base::WrapUnique(new MigrationTargetInstallJob(
      std::move(manifest), web_contents, profile, data_retriever, clock,
      debug_value, lock, std::move(callback)));
  job->Start();
  return job;
}

MigrationTargetInstallJob::~MigrationTargetInstallJob() = default;

MigrationTargetInstallJob::MigrationTargetInstallJob(
    blink::mojom::ManifestPtr manifest,
    base::WeakPtr<content::WebContents> web_contents,
    Profile* profile,
    WebAppDataRetriever* data_retriever,
    base::Clock* clock,
    base::DictValue* debug_value,
    WithAppResources* lock,
    MigrationTargetInstallCallback callback)
    : manifest_(std::move(manifest)),
      web_contents_(web_contents),
      profile_(profile),
      data_retriever_(*data_retriever),
      clock_(*clock),
      debug_value_(*debug_value),
      lock_(*lock),
      callback_(std::move(callback)) {}

void MigrationTargetInstallJob::Start() {
  bool installed_as_migration_suggestion =
      lock_->registrar().AppMatches(GenerateAppIdFromManifest(*manifest_),
                                    WebAppFilter::IsAppSuggestedForMigration());
  bool installed_as_normal = lock_->registrar().AppMatches(
      GenerateAppIdFromManifest(*manifest_), WebAppFilter::InstalledInChrome());

  // Only one of the above can be true, or both are false.
  CHECK(!(installed_as_migration_suggestion && installed_as_normal));

  if (installed_as_migration_suggestion || installed_as_normal) {
    ManifestUpdateJob::Options options;
    // If this app is only installed as a migration suggestion the entire app
    // can be updated as the app identity members (name and
    // icons) have not yet been presented to the user for the migration.
    options.force_silent_update_identity = installed_as_migration_suggestion;
    options.fail_if_any_icon_download_fails = true;
    options.skip_icon_download_if_no_manifest_change = true;
    manifest_update_job_ = ManifestUpdateJob::CreateAndStart(
        &lock_.get(), web_contents_.get(),
        debug_value_->EnsureDict("ManifestUpdateJob"), std::move(manifest_),
        &data_retriever_.get(), &clock_.get(),
        base::BindOnce(&MigrationTargetInstallJob::OnManifestUpdateJobFinished,
                       weak_factory_.GetWeakPtr()),
        options);
    return;
  }
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *manifest_, data_retriever_.get(),
          /*background_installation=*/true,
          webapps::WebappInstallSource::MIGRATION, web_contents_,
          [](IconUrlSizeSet&) {},
          *debug_value_->EnsureDict("ManifestToWebAppInstallInfoJob"),
          base::BindOnce(&MigrationTargetInstallJob::
                             OnManifestToWebAppInstallInfoJobFinished,
                         weak_factory_.GetWeakPtr()));
}

void MigrationTargetInstallJob::OnManifestToWebAppInstallInfoJobFinished(
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    Complete(
        MigrationTargetInstallJobResult::kManifestToWebAppInstallInfoError);
    return;
  }

  if (!web_contents_) {
    Complete(MigrationTargetInstallJobResult::kWebContentsWasDestroyed);
    return;
  }

  WebAppInstallParams install_params;
  install_params.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
  install_params.add_to_applications_menu = false;
  install_params.add_to_desktop = false;
  install_params.add_to_quick_launch_bar = false;

  install_from_info_job_ = std::make_unique<InstallFromInfoJob>(
      profile_.get(), *debug_value_->EnsureDict("InstallFromInfoJob"),
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::MIGRATION, install_params,
      base::BindOnce(&MigrationTargetInstallJob::OnInstallFromInfoJobFinished,
                     weak_factory_.GetWeakPtr()));
  install_from_info_job_->Start(&lock_.get());
}

void MigrationTargetInstallJob::OnInstallFromInfoJobFinished(
    webapps::AppId app_id,
    webapps::InstallResultCode code) {
  debug_value_->Set("install_from_info_result", base::ToString(code));
  if (webapps::IsSuccess(code)) {
    Complete(MigrationTargetInstallJobResult::kSuccessInstalled);
  } else {
    Complete(MigrationTargetInstallJobResult::kInstallFromInfoFailed);
  }
}

void MigrationTargetInstallJob::OnManifestUpdateJobFinished(
    ManifestUpdateJobResultWithTimestamp result) {
  debug_value_->Set("manifest_update_result", base::ToString(result.result()));
  switch (result.result()) {
    case ManifestUpdateJobResult::kNoUpdateNeeded:
    case ManifestUpdateJobResult::kSilentlyUpdated:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppOnlyHasSecurityUpdate:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges:
      Complete(MigrationTargetInstallJobResult::kSuccessUpdated);
      return;
    case ManifestUpdateJobResult::kAppNotAllowedToUpdate:
      // The job checks to see that the app is installed, so this should not be
      // possible.
      NOTREACHED();
    case ManifestUpdateJobResult::kIconDownloadFailed:
    case ManifestUpdateJobResult::kIconReadFromDiskFailed:
    case ManifestUpdateJobResult::kManifestConversionFailed:
      Complete(
          MigrationTargetInstallJobResult::kManifestToWebAppInstallInfoError);
      return;
    case ManifestUpdateJobResult::kUserNavigated:
    case ManifestUpdateJobResult::kWebContentsDestroyed:
      Complete(MigrationTargetInstallJobResult::kWebContentsWasDestroyed);
      return;
    case ManifestUpdateJobResult::kIconWriteToDiskFailed:
    case ManifestUpdateJobResult::kInstallFinalizeFailed:
      Complete(MigrationTargetInstallJobResult::kUpdateFailed);
      return;
  }
  NOTREACHED();
}

void MigrationTargetInstallJob::Complete(
    MigrationTargetInstallJobResult result) {
  debug_value_->Set("result", base::ToString(result));
  std::move(callback_).Run(result);
}

}  // namespace web_app
