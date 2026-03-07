// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/finalize_update_job.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

FinalizeUpdateJob::FinalizeUpdateJob(Lock* lock,
                                     WithAppResources* lock_with_app_resources,
                                     WebAppProvider& provider,
                                     const WebAppInstallInfo& web_app_info)
    : lock_(lock),
      lock_with_app_resources_(lock_with_app_resources),
      provider_(provider),
      web_app_info_(web_app_info.Clone()),
      app_id_(GenerateAppIdFromManifestId(web_app_info_.manifest_id())) {}

FinalizeUpdateJob::~FinalizeUpdateJob() = default;

void FinalizeUpdateJob::Start(InstallFinalizedCallback callback) {
  callback_ = std::move(callback);

  webapps::ManifestId manifest_id = web_app_info_.manifest_id();
  const WebApp* existing_web_app = registrar().GetAppById(app_id_);

  if (!existing_web_app ||
      existing_web_app->is_from_sync_and_pending_installation() ||
      app_id_ != existing_web_app->app_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FinalizeUpdateJob::RunCallbackAndResetLock,
                       weak_ptr_factory_.GetWeakPtr(), webapps::AppId(),
                       webapps::InstallResultCode::kWebAppDisabled));
    return;
  }

  bool needs_scope_validation =
      !web_app_info_.scope_extensions.empty() &&
      !web_app_info_.validated_scope_extensions.has_value();
  bool needs_migration_validation = !web_app_info_.migration_sources.empty();

  // Remove this shortcut after the ManifestUpdateCheckCommand is deleted:
  if (!needs_scope_validation && !needs_migration_validation) {
    OnOriginAssociationValidatedForUpdate(OriginAssociations());
    return;
  }
  OriginAssociations origin_associations;
  if (needs_scope_validation) {
    origin_associations.scope_extensions = web_app_info_.scope_extensions;
  }
  if (needs_migration_validation) {
    origin_associations.migration_sources = web_app_info_.migration_sources;
  }
  origin_association_manager().GetWebAppOriginAssociations(
      manifest_id, std::move(origin_associations),
      base::BindOnce(&FinalizeUpdateJob::OnOriginAssociationValidatedForUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FinalizeUpdateJob::OnOriginAssociationValidatedForUpdate(
    OriginAssociations validated_origin_associations) {
  const WebApp* existing_web_app = registrar().GetAppById(app_id_);

  if (!existing_web_app ||
      existing_web_app->is_from_sync_and_pending_installation() ||
      app_id_ != existing_web_app->app_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FinalizeUpdateJob::RunCallbackAndResetLock,
                       weak_ptr_factory_.GetWeakPtr(), webapps::AppId(),
                       webapps::InstallResultCode::kWebAppDisabled));
    return;
  }

  std::optional<WebAppScope> old_scope = existing_web_app->GetScope();

  CommitCallback commit_callback = base::BindOnce(
      &FinalizeUpdateJob::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), registrar().GetAppShortName(app_id_),
      GetFileHandlerUpdateAction(), std::move(old_scope));

  auto web_app = std::make_unique<WebApp>(*existing_web_app);
  if (web_app->isolation_data().has_value()) {
    const std::optional<IsolationData::PendingUpdateInfo>& pending_update_info =
        web_app->isolation_data()->pending_update_info();
    CHECK(pending_update_info.has_value())
        << "Isolated Web Apps can only be updated if "
           "`IsolationData::PendingUpdateInfo` is set.";
    CHECK_EQ(web_app_info_.isolated_web_app_version(),
             pending_update_info->version);
    UpdateIsolationDataAndResetPendingUpdateInfo(
        web_app.get(), pending_update_info->location,
        pending_update_info->version, web_app_info_.iwa_update_manifest_url,
        pending_update_info->integrity_block_data);
  }

  ScopeExtensions validated_scope_extensions =
      web_app_info_.validated_scope_extensions.value_or(
          validated_origin_associations.scope_extensions);
  for (auto& scope_extension : validated_scope_extensions) {
    // This is done to prune any queries or fragments from the scope URL which
    // may have been skipped by WebAppOriginAssociationManager validation.
    scope_extension = ScopeExtensionInfo::CreateForScope(
        scope_extension.scope, scope_extension.has_origin_wildcard);
  }
  web_app->SetValidatedScopeExtensions(validated_scope_extensions);
  web_app->SetValidatedMigrationSources(
      validated_origin_associations.migration_sources);

  // Prepare copy-on-write to update existing app.
  // This is not reached unless the data obtained from the manifest
  // update process is valid, so an invariant of the system is that
  // icons are valid here.
  SetWebAppManifestFieldsAndWriteData(
      std::move(web_app), std::move(commit_callback),
      /*skip_icon_writes_on_download_failure=*/false);
}

void FinalizeUpdateJob::OnDatabaseCommitCompletedForUpdate(
    std::string old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    std::optional<WebAppScope> old_scope,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    RunCallbackAndResetLock(webapps::AppId(),
                            webapps::InstallResultCode::kWriteDataFailed);
    return;
  }

  const WebApp* web_app = registrar().GetAppById(app_id_);
  if (old_scope.has_value() && old_scope.value() != web_app->GetScope()) {
    registrar().NotifyWebAppEffectiveScopeChanged(app_id_);
  }

  // OS integration should always be enabled on ChromeOS for manifest updates.
  bool should_skip_os_integration_on_manifest_update = false;
#if !BUILDFLAG(IS_CHROMEOS)
  // If the app being updated was installed by default and not also manually
  // installed by the user or an enterprise policy, disable os integration.
  should_skip_os_integration_on_manifest_update =
      registrar().GetInstallState(app_id_) ==
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (should_skip_os_integration_on_manifest_update) {
    OnUpdateHooksFinished();
    return;
  }

  os_integration_manager().Synchronize(
      app_id_, base::BindOnce(&FinalizeUpdateJob::OnUpdateHooksFinished,
                              weak_ptr_factory_.GetWeakPtr()));
}

void FinalizeUpdateJob::OnUpdateHooksFinished() {
  install_manager().NotifyWebAppManifestUpdated(app_id_);
  RunCallbackAndResetLock(app_id_,
                          webapps::InstallResultCode::kSuccessAlreadyInstalled);
}

FileHandlerUpdateAction FinalizeUpdateJob::GetFileHandlerUpdateAction() {
  // TODO(crbug.com/411632946): Add test case: Update file handler in
  // manifest for an already installed app + override user choice by
  // adding the app to file handlers policy.
  if (registrar().GetAppFileHandlerUserApprovalState(app_id_) ==
      ApiApprovalState::kDisallowed) {
    return FileHandlerUpdateAction::kNoUpdate;
  }

  // TODO(crbug.com/40176713): Consider trying to re-use the comparison
  // results from the ManifestUpdateDataFetchCommand.
  const apps::FileHandlers* old_handlers =
      registrar().GetAppFileHandlers(app_id_);
  DCHECK(old_handlers);
  if (*old_handlers == web_app_info_.file_handlers) {
    return FileHandlerUpdateAction::kNoUpdate;
  }

  return FileHandlerUpdateAction::kUpdate;
}

void FinalizeUpdateJob::UpdateIsolationDataAndResetPendingUpdateInfo(
    WebApp* web_app,
    const IsolatedWebAppStorageLocation& location,
    const IwaVersion& version,
    const std::optional<GURL>& iwa_update_manifest_url,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data) {
  IsolationData::Builder builder(location, version);

  if (web_app->isolation_data()) {
    builder.PersistFieldsForUpdate(*web_app->isolation_data());
  }

  if (iwa_update_manifest_url) {
    builder.SetUpdateManifestUrl(*iwa_update_manifest_url);
  }

  if (integrity_block_data) {
    builder.SetIntegrityBlockData(std::move(*integrity_block_data));
  }

  web_app->SetIsolationData(std::move(builder).Build());
}

void FinalizeUpdateJob::SetWebAppManifestFieldsAndWriteData(
    std::unique_ptr<WebApp> web_app,
    CommitCallback commit_callback,
    bool skip_icon_writes_on_download_failure) {
  const auto& registrar = this->registrar();
  const WebApp* existing_app = registrar.GetAppById(app_id_);

  SetWebAppManifestFields(web_app_info_, *web_app,
                          skip_icon_writes_on_download_failure);
  FinalizeInstallJob::AdjustAppStateBeforeCommit(existing_app, *web_app,
                                                 *provider_);

  auto write_translations_callback = base::BindOnce(
      &FinalizeUpdateJob::WriteTranslations, weak_ptr_factory_.GetWeakPtr(),
      app_id_, web_app_info_.translations);
  auto commit_to_sync_bridge_callback =
      base::BindOnce(&FinalizeUpdateJob::CommitToSyncBridge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app));
  auto on_icon_write_complete_callback =
      base::BindOnce(std::move(write_translations_callback),
                     base::BindOnce(std::move(commit_to_sync_bridge_callback),
                                    std::move(commit_callback)));

  // Do not overwrite the icon data in the DB if icon downloading has failed. We
  // skip directly to writing translations and then writing the app via the
  // WebAppSyncBridge.
  if (skip_icon_writes_on_download_failure) {
    std::move(on_icon_write_complete_callback).Run(/*success=*/true);
  } else {
    IconBitmaps icon_bitmaps = web_app_info_.icon_bitmaps;
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps =
        web_app_info_.shortcuts_menu_icon_bitmaps;
    IconsMap other_icon_bitmaps = web_app_info_.other_icon_bitmaps;
    IconBitmaps trusted_icon_bitmaps = web_app_info_.trusted_icon_bitmaps;

    icon_manager().WriteData(
        app_id_, std::move(icon_bitmaps), std::move(trusted_icon_bitmaps),
        std::move(shortcuts_menu_icon_bitmaps), std::move(other_icon_bitmaps),
        std::move(on_icon_write_complete_callback));
  }
}

void FinalizeUpdateJob::WriteTranslations(
    const webapps::AppId& app_id,
    const base::flat_map<std::string, blink::Manifest::TranslationItem>&
        translations,
    CommitCallback commit_callback,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }
  translation_manager().WriteTranslations(app_id, translations,
                                          std::move(commit_callback));
}

void FinalizeUpdateJob::CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                                           CommitCallback commit_callback,
                                           bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  ScopedRegistryUpdate update =
      sync_bridge().BeginUpdate(std::move(commit_callback));

  WebApp* app_to_override = update->UpdateApp(app_id_);
  if (app_to_override) {
    *app_to_override = std::move(*web_app);
  } else {
    update->CreateApp(std::move(web_app));
  }
}

WebAppRegistrar& FinalizeUpdateJob::registrar() const {
  if (lock_with_app_resources_) {
    return lock_with_app_resources_->registrar();
  }
  return provider_->registrar_unsafe();
}

WebAppSyncBridge& FinalizeUpdateJob::sync_bridge() const {
  if (lock_with_app_resources_) {
    return lock_with_app_resources_->sync_bridge();
  }
  return provider_->sync_bridge_unsafe();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppInstallManager& FinalizeUpdateJob::install_manager() const {
  if (lock_with_app_resources_) {
    return lock_with_app_resources_->install_manager();
  }
  return provider_->install_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppIconManager& FinalizeUpdateJob::icon_manager() const {
  if (lock_with_app_resources_) {
    return lock_with_app_resources_->icon_manager();
  }
  return provider_->icon_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppTranslationManager& FinalizeUpdateJob::translation_manager() const {
  if (lock_with_app_resources_) {
    return lock_with_app_resources_->translation_manager();
  }
  return provider_->translation_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
OsIntegrationManager& FinalizeUpdateJob::os_integration_manager() const {
  if (lock_with_app_resources_) {
    return lock_with_app_resources_->os_integration_manager();
  }
  return provider_->os_integration_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppOriginAssociationManager& FinalizeUpdateJob::origin_association_manager()
    const {
  if (lock_) {
    return lock_->origin_association_manager();
  }
  return provider_->origin_association_manager();
}

void FinalizeUpdateJob::RunCallbackAndResetLock(
    webapps::AppId app_id,
    webapps::InstallResultCode code) {
  lock_with_app_resources_ = nullptr;
  std::move(callback_).Run(std::move(app_id), code);
}

}  // namespace web_app
