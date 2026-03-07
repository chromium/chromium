// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_migrate_to_app_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job.h"
#include "chrome/browser/web_applications/jobs/manifest_update_job_result.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job_result.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scheduler/install_migrate_to_app_result.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

InstallMigrateToAppCommand::InstallMigrateToAppCommand(
    const webapps::ManifestId& source_manifest_id,
    const webapps::ManifestId& target_manifest_id,
    const GURL& target_install_url,
    Profile* profile,
    InstallMigrateToAppResultCallback callback)
    : WebAppCommand<SharedWebContentsLock, InstallMigrateToAppResult>(
          "InstallMigrateToAppCommand",
          SharedWebContentsLockDescription(),
          base::BindOnce([](InstallMigrateToAppResult result) {
            base::UmaHistogramEnumeration("WebApp.InstallMigrateToApp.Result",
                                          result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/InstallMigrateToAppResult::kShutdown),
      source_manifest_id_(source_manifest_id),
      target_manifest_id_(target_manifest_id),
      target_install_url_(target_install_url),
      profile_(profile) {
  GetMutableDebugValue().Set("source_manifest_id", source_manifest_id_.spec());
  GetMutableDebugValue().Set("target_manifest_id", target_manifest_id_.spec());
  GetMutableDebugValue().Set("target_install_url", target_install_url_.spec());
}

InstallMigrateToAppCommand::~InstallMigrateToAppCommand() = default;

void InstallMigrateToAppCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  shared_web_contents_lock_ = std::move(lock);

  url_loader_ =
      shared_web_contents_lock_->web_contents_manager().CreateUrlLoader();
  url_loader_->LoadUrl(target_install_url_,
                       &shared_web_contents_lock_->shared_web_contents(),
                       webapps::WebAppUrlLoader::UrlComparison::kExact,
                       base::BindOnce(&InstallMigrateToAppCommand::OnUrlLoaded,
                                      weak_factory_.GetWeakPtr()));
}

void InstallMigrateToAppCommand::OnUrlLoaded(
    webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("url_load_result", base::ToString(result));
  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            InstallMigrateToAppResult::kUrlLoadFailed);
    return;
  }

  data_retriever_ =
      shared_web_contents_lock_->web_contents_manager().CreateDataRetriever();

  // This explicitly does NOT ask to download the primary icon, to prevent
  // network usage and because we check for the icon downloading later.
  // However, kValidManifestIgnoreDisplay does still check for the existence of
  // a primary icon url.
  // TODO(https://crbug.com/468037835): Make this criteria logic not need the
  // whole InstallableManager layer here if possible.
  webapps::InstallableParams params;
  params.check_eligibility = true;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &shared_web_contents_lock_->shared_web_contents(),
      base::BindOnce(&InstallMigrateToAppCommand::OnManifestFetched,
                     weak_factory_.GetWeakPtr()),
      params);
}

void InstallMigrateToAppCommand::OnManifestFetched(
    blink::mojom::ManifestPtr manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode installable_status) {
  GetMutableDebugValue().Set("installable_status",
                             base::ToString(installable_status));
  if (!manifest || !valid_manifest_for_web_app) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            InstallMigrateToAppResult::kInstallError);
    return;
  }

  // Verify manifest id matches expected target_manifest_id_.
  if (manifest->id != target_manifest_id_) {
    GetMutableDebugValue().Set("manifest_id_mismatch", true);
    GetMutableDebugValue().Set("actual_manifest_id", manifest->id.spec());
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            InstallMigrateToAppResult::kManifestIdMismatch);
    return;
  }

  // Verify migrate_from field is present and matches source_manifest_id_.
  bool migrate_from_match = false;
  for (const auto& migrate_from : manifest->migrate_from) {
    if (migrate_from->id == source_manifest_id_) {
      migrate_from_match = true;
      break;
    }
  }

  if (!migrate_from_match) {
    GetMutableDebugValue().Set("migrate_from_mismatch", true);
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            InstallMigrateToAppResult::kMigrateFromMismatch);
    return;
  }

  manifest_ = std::move(manifest);
  webapps::AppId target_app_id =
      GenerateAppIdFromManifestId(target_manifest_id_);
  shared_web_contents_with_app_lock_ =
      std::make_unique<SharedWebContentsWithAppLock>();

  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(shared_web_contents_lock_), *shared_web_contents_with_app_lock_,
      {target_app_id},
      base::BindOnce(&InstallMigrateToAppCommand::OnAppLockAcquired,
                     weak_factory_.GetWeakPtr()));
}

void InstallMigrateToAppCommand::OnAppLockAcquired() {
  webapps::AppId target_app_id = GenerateAppIdFromManifestId(manifest_->id);
  std::optional<proto::InstallState> install_state =
      shared_web_contents_with_app_lock_->registrar().GetInstallState(
          target_app_id);
  GetMutableDebugValue().Set(
      "migrate_to_app_install_state",
      install_state.has_value() ? base::ToString(*install_state) : "nullopt");
  if (install_state.has_value()) {
    update_job_ = ManifestUpdateJob::CreateAndStart(
        *profile_, shared_web_contents_with_app_lock_.get(),
        shared_web_contents_with_app_lock_.get(),
        &shared_web_contents_with_app_lock_->shared_web_contents(),
        GetMutableDebugValue().EnsureDict("ManifestUpdateJob"),
        std::move(manifest_), data_retriever_.get(),
        &WebAppProvider::GetForWebApps(profile_)->clock(),
        base::BindOnce(&InstallMigrateToAppCommand::OnUpdateJobFinished,
                       weak_factory_.GetWeakPtr()),
        ManifestUpdateJob::Options());
  } else {
    install_job_ = MigrationTargetInstallJob::CreateAndStart(
        std::move(manifest_),
        shared_web_contents_with_app_lock_->shared_web_contents().GetWeakPtr(),
        profile_, data_retriever_.get(),
        GetMutableDebugValue().EnsureDict("MigrationTargetInstallJob"),
        shared_web_contents_with_app_lock_.get(),
        shared_web_contents_with_app_lock_.get(),
        base::BindOnce(&InstallMigrateToAppCommand::OnInstallJobFinished,
                       weak_factory_.GetWeakPtr()));
  }
}

void InstallMigrateToAppCommand::OnInstallJobFinished(
    MigrationTargetInstallJobResult result) {
  InstallMigrateToAppResult command_result;
  CommandResult cmd_res = CommandResult::kSuccess;
  switch (result) {
    case MigrationTargetInstallJobResult::kSuccessInstalled:
      command_result = InstallMigrateToAppResult::kSuccessNewInstall;
      break;
    case MigrationTargetInstallJobResult::kAlreadyInstalled:
      command_result = InstallMigrateToAppResult::kSuccessAlreadyInstalled;
      break;
    case MigrationTargetInstallJobResult::kManifestToWebAppInstallInfoError:
      command_result =
          InstallMigrateToAppResult::kManifestToWebAppInstallInfoError;
      cmd_res = CommandResult::kFailure;
      break;
    case MigrationTargetInstallJobResult::kInstallFromInfoFailed:
      command_result = InstallMigrateToAppResult::kInstallFromInfoFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case MigrationTargetInstallJobResult::kUpdateFailed:
      command_result = InstallMigrateToAppResult::kUpdateFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case MigrationTargetInstallJobResult::kWebContentsWasDestroyed:
      command_result =
          web_app::InstallMigrateToAppResult::kWebContentsDestroyed;
      cmd_res = CommandResult::kSuccess;
      break;
  }
  CompleteAndSelfDestruct(cmd_res, command_result);
}

void InstallMigrateToAppCommand::OnUpdateJobFinished(
    ManifestUpdateJobResultWithTimestamp result) {
  InstallMigrateToAppResult command_result;
  CommandResult cmd_res = CommandResult::kSuccess;
  switch (result.result()) {
    case ManifestUpdateJobResult::kNoUpdateNeeded:
    case ManifestUpdateJobResult::kSilentlyUpdated:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppOnlyHasSecurityUpdate:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle:
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges:
    case ManifestUpdateJobResult::kSilentlyUpdatedDueToSmallIconComparison:
      command_result = InstallMigrateToAppResult::kSuccessAlreadyInstalled;
      break;
    case ManifestUpdateJobResult::kIconDownloadFailed:
      command_result = InstallMigrateToAppResult::kIconDownloadFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kIconReadFromDiskFailed:
      command_result = InstallMigrateToAppResult::kIconReadFromDiskFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kIconWriteToDiskFailed:
      command_result = InstallMigrateToAppResult::kIconWriteToDiskFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kInstallFinalizeFailed:
      command_result = InstallMigrateToAppResult::kInstallFinalizeFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kManifestConversionFailed:
      command_result = InstallMigrateToAppResult::kManifestConversionFailed;
      cmd_res = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kAppNotAllowedToUpdate:
      command_result = InstallMigrateToAppResult::kAppNotAllowedToUpdate;
      cmd_res = CommandResult::kFailure;
      break;
    case ManifestUpdateJobResult::kWebContentsDestroyed:
      command_result = InstallMigrateToAppResult::kWebContentsDestroyed;
      break;
    case ManifestUpdateJobResult::kUserNavigated:
      command_result = InstallMigrateToAppResult::kUserNavigated;
      break;
  }
  CompleteAndSelfDestruct(cmd_res, command_result);
}

}  // namespace web_app
