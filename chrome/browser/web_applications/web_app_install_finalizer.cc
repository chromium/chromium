// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <map>
#include <optional>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/model/app_installed_by.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
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
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {
namespace {
}  // namespace

WebAppInstallFinalizer::FinalizeOptions::IwaOptions::IwaOptions(
    IsolatedWebAppStorageLocation location,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data)
    : location(std::move(location)),
      integrity_block_data(std::move(integrity_block_data)) {}

WebAppInstallFinalizer::FinalizeOptions::IwaOptions::~IwaOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::IwaOptions::IwaOptions(
    const IwaOptions&) = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    webapps::WebappInstallSource install_surface)
    : source(ConvertInstallSurfaceToWebAppSource(install_surface)),
      install_surface(install_surface) {}

WebAppInstallFinalizer::FinalizeOptions::~FinalizeOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    const FinalizeOptions&) = default;

bool& WebAppInstallFinalizer::
    DisableUserDisplayModeSyncMitigationsForTesting() {
  static bool disable = false;
  return disable;
}

WebAppInstallFinalizer::WebAppInstallFinalizer(Profile* profile)
    : profile_(profile) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebAppInstallInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40693380): Implement a before-start queue in
  // WebAppInstallManager and replace this runtime error in
  // WebAppInstallFinalizer with DCHECK(started_).
  if (!started_) {
    std::move(callback).Run(
        webapps::AppId(), webapps::InstallResultCode::kWebAppProviderNotReady);
    return;
  }

  if (options.install_state == proto::InstallState::SUGGESTED_FROM_MIGRATION &&
      web_app_info.migration_sources.empty()) {
    std::move(callback).Run(
        webapps::AppId(), webapps::InstallResultCode::kNoValidMigrationSource);
    return;
  }

  std::unique_ptr<FinalizeInstallJob> web_app_install_job =
      std::make_unique<FinalizeInstallJob>(*profile_, *provider_, clock_.get(),
                                           *this, std::move(web_app_info),
                                           options);
  FinalizeInstallJob* job_ptr = web_app_install_job.get();
  install_jobs_.insert(std::move(web_app_install_job));
  job_ptr->Start(base::BindOnce(&WebAppInstallFinalizer::OnInstallJobFinished,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::Unretained(job_ptr),
                                std::move(callback)));
}

void WebAppInstallFinalizer::OnInstallJobFinished(
    FinalizeInstallJob* job,
    InstallFinalizedCallback callback,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  install_jobs_.erase(job);
  std::move(callback).Run(app_id, code);
}

void WebAppInstallFinalizer::FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                                            InstallFinalizedCallback callback) {
  CHECK(started_);
  webapps::ManifestId manifest_id = web_app_info.manifest_id();
  const webapps::AppId app_id = GenerateAppIdFromManifestId(manifest_id);
  const WebApp* existing_web_app =
      provider_->registrar_unsafe().GetAppById(app_id);

  if (!existing_web_app ||
      existing_web_app->is_from_sync_and_pending_installation() ||
      app_id != existing_web_app->app_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), webapps::AppId(),
                                  webapps::InstallResultCode::kWebAppDisabled));
    return;
  }

  bool needs_scope_validation =
      !web_app_info.scope_extensions.empty() &&
      !web_app_info.validated_scope_extensions.has_value();
  bool needs_migration_validation = !web_app_info.migration_sources.empty();

  // Remove this shortcut after the ManifestUpdateCheckCommand is deleted:
  if (!needs_scope_validation && !needs_migration_validation) {
    OnOriginAssociationValidatedForUpdate(
        web_app_info.Clone(), std::move(callback), OriginAssociations());
    return;
  }
  OriginAssociations origin_associations;
  if (needs_scope_validation) {
    origin_associations.scope_extensions = web_app_info.scope_extensions;
  }
  if (needs_migration_validation) {
    origin_associations.migration_sources = web_app_info.migration_sources;
  }
  provider_->origin_association_manager().GetWebAppOriginAssociations(
      manifest_id, std::move(origin_associations),
      base::BindOnce(
          &WebAppInstallFinalizer::OnOriginAssociationValidatedForUpdate,
          weak_ptr_factory_.GetWeakPtr(), web_app_info.Clone(),
          std::move(callback)));
}

void WebAppInstallFinalizer::SetProvider(base::PassKey<WebAppProvider>,
                                         WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppInstallFinalizer::Start() {
  DCHECK(!started_);
  started_ = true;
}

void WebAppInstallFinalizer::Shutdown() {
  started_ = false;
  // TODO(crbug.com/40810770): Turn WebAppInstallFinalizer into a command so it
  // can properly call callbacks on shutdown instead of dropping them on
  // shutdown.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WebAppInstallFinalizer::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void WebAppInstallFinalizer::OnOriginAssociationValidatedForUpdate(
    WebAppInstallInfo web_app_info,
    InstallFinalizedCallback callback,
    OriginAssociations validated_origin_associations) {
  webapps::AppId app_id = GenerateAppIdFromManifestId(
      web_app_info.manifest_id(), web_app_info.parent_app_manifest_id);

  const WebApp* existing_web_app =
      provider_->registrar_unsafe().GetAppById(app_id);

  if (!existing_web_app ||
      existing_web_app->is_from_sync_and_pending_installation() ||
      app_id != existing_web_app->app_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), webapps::AppId(),
                                  webapps::InstallResultCode::kWebAppDisabled));
    return;
  }

  std::optional<WebAppScope> old_scope = existing_web_app->GetScope();

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id,
      provider_->registrar_unsafe().GetAppShortName(app_id),
      GetFileHandlerUpdateAction(app_id, web_app_info), web_app_info.Clone(),
      std::move(old_scope));

  auto web_app = std::make_unique<WebApp>(*existing_web_app);
  if (web_app->isolation_data().has_value()) {
    const std::optional<IsolationData::PendingUpdateInfo>& pending_update_info =
        web_app->isolation_data()->pending_update_info();
    CHECK(pending_update_info.has_value())
        << "Isolated Web Apps can only be updated if "
           "`IsolationData::PendingUpdateInfo` is set.";
    CHECK_EQ(web_app_info.isolated_web_app_version(),
             pending_update_info->version);
    FinalizeInstallJob::UpdateIsolationDataAndResetPendingUpdateInfo(
        web_app.get(), pending_update_info->location,
        pending_update_info->version, web_app_info.iwa_update_manifest_url,
        pending_update_info->integrity_block_data);
  }

  ScopeExtensions validated_scope_extensions =
      web_app_info.validated_scope_extensions.value_or(
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
      web_app_info, std::move(web_app), std::move(commit_callback),
      /*skip_icon_writes_on_download_failure=*/false);
}

void WebAppInstallFinalizer::SetWebAppManifestFieldsAndWriteData(
    const WebAppInstallInfo& web_app_info,
    std::unique_ptr<WebApp> web_app,
    CommitCallback commit_callback,
    bool skip_icon_writes_on_download_failure) {
  const auto& registrar = provider_->registrar_unsafe();
  const WebApp* existing_app = registrar.GetAppById(web_app->app_id());

  SetWebAppManifestFields(web_app_info, *web_app,
                          skip_icon_writes_on_download_failure);
  FinalizeInstallJob::AdjustAppStateBeforeCommit(existing_app, *web_app,
                                                 *provider_);

  webapps::AppId app_id = web_app->app_id();
  auto write_translations_callback = base::BindOnce(
      &WebAppInstallFinalizer::WriteTranslations,
      weak_ptr_factory_.GetWeakPtr(), app_id, web_app_info.translations);
  auto commit_to_sync_bridge_callback =
      base::BindOnce(&WebAppInstallFinalizer::CommitToSyncBridge,
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
    IconBitmaps icon_bitmaps = web_app_info.icon_bitmaps;
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps =
        web_app_info.shortcuts_menu_icon_bitmaps;
    IconsMap other_icon_bitmaps = web_app_info.other_icon_bitmaps;
    IconBitmaps trusted_icon_bitmaps = web_app_info.trusted_icon_bitmaps;

    provider_->icon_manager().WriteData(
        app_id, std::move(icon_bitmaps), std::move(trusted_icon_bitmaps),
        std::move(shortcuts_menu_icon_bitmaps), std::move(other_icon_bitmaps),
        std::move(on_icon_write_complete_callback));
  }
}

void WebAppInstallFinalizer::WriteTranslations(
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
  provider_->translation_manager().WriteTranslations(
      app_id, translations, std::move(commit_callback));
}

void WebAppInstallFinalizer::CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                                                CommitCallback commit_callback,
                                                bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  webapps::AppId app_id = web_app->app_id();

  ScopedRegistryUpdate update =
      provider_->sync_bridge_unsafe().BeginUpdate(std::move(commit_callback));

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override) {
    *app_to_override = std::move(*web_app);
  } else {
    update->CreateApp(std::move(web_app));
  }
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall(
    InstallFinalizedCallback callback,
    webapps::AppId app_id,
    FinalizeOptions finalize_options,
    std::optional<WebAppScope> old_scope,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(webapps::AppId(),
                            webapps::InstallResultCode::kWriteDataFailed);
    return;
  }

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  // TODO(dmurph): Verify this check is not needed and remove after
  // isolation work is done. https://crbug.com/1298130
  if (!web_app) {
    std::move(callback).Run(
        webapps::AppId(),
        webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
    return;
  }
  if (old_scope.has_value() && old_scope.value() != web_app->GetScope()) {
    provider_->registrar_unsafe().NotifyWebAppEffectiveScopeChanged(app_id);
  }

  provider_->install_manager().NotifyWebAppInstalled(app_id);

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = finalize_options.add_to_desktop;
  synchronize_options.add_to_quick_launch_bar =
      finalize_options.add_to_quick_launch_bar;

  switch (finalize_options.source) {
    case WebAppManagement::kSystem:
    case WebAppManagement::kPolicy:
    case WebAppManagement::kIwaPolicy:
    case WebAppManagement::kDefault:
    case WebAppManagement::kOem:
    case WebAppManagement::kApsDefault:
    case WebAppManagement::kIwaShimlessRma:
      synchronize_options.reason = SHORTCUT_CREATION_AUTOMATED;
      break;
    case WebAppManagement::kKiosk:
    case WebAppManagement::kSubApp:
    case WebAppManagement::kWebAppStore:
    case WebAppManagement::kOneDriveIntegration:
    case WebAppManagement::kSync:
    case WebAppManagement::kUserInstalled:
    case WebAppManagement::kIwaUserInstalled:
      synchronize_options.reason = SHORTCUT_CREATION_BY_USER;
      break;
  }

  provider_->os_integration_manager().Synchronize(
      app_id,
      base::BindOnce(&WebAppInstallFinalizer::OnInstallHooksFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_id),
      synchronize_options);
}

void WebAppInstallFinalizer::OnInstallHooksFinished(
    InstallFinalizedCallback callback,
    webapps::AppId app_id) {
  // Only notify that os hooks were added if the installation was a 'full'
  // installation.
  if (provider_->registrar_unsafe().GetInstallState(app_id) ==
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
    callback = std::move(callback).Then(base::BindOnce(
        &WebAppInstallFinalizer::NotifyWebAppInstalledWithOsHooks,
        weak_ptr_factory_.GetWeakPtr(), app_id));
  }
  std::move(callback).Run(app_id,
                          webapps::InstallResultCode::kSuccessNewInstall);
}

void WebAppInstallFinalizer::NotifyWebAppInstalledWithOsHooks(
    webapps::AppId app_id) {
  provider_->install_manager().NotifyWebAppInstalledWithOsHooks(app_id);
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate(
    InstallFinalizedCallback callback,
    webapps::AppId app_id,
    std::string old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebAppInstallInfo& web_app_info,
    std::optional<WebAppScope> old_scope,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(webapps::AppId(),
                            webapps::InstallResultCode::kWriteDataFailed);
    return;
  }

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (old_scope.has_value() && old_scope.value() != web_app->GetScope()) {
    provider_->registrar_unsafe().NotifyWebAppEffectiveScopeChanged(app_id);
  }

  // OS integration should always be enabled on ChromeOS for manifest updates.
  bool should_skip_os_integration_on_manifest_update = false;
#if !BUILDFLAG(IS_CHROMEOS)
  // If the app being updated was installed by default and not also manually
  // installed by the user or an enterprise policy, disable os integration.
  should_skip_os_integration_on_manifest_update =
      provider_->registrar_unsafe().GetInstallState(app_id) ==
      proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (should_skip_os_integration_on_manifest_update) {
    provider_->install_manager().NotifyWebAppManifestUpdated(app_id);
    std::move(callback).Run(
        app_id, webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  provider_->os_integration_manager().Synchronize(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnUpdateHooksFinished,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), app_id));
}

void WebAppInstallFinalizer::OnUpdateHooksFinished(
    InstallFinalizedCallback callback,
    webapps::AppId app_id) {
  provider_->install_manager().NotifyWebAppManifestUpdated(app_id);
  std::move(callback).Run(app_id,
                          webapps::InstallResultCode::kSuccessAlreadyInstalled);
}

FileHandlerUpdateAction WebAppInstallFinalizer::GetFileHandlerUpdateAction(
    const webapps::AppId& app_id,
    const WebAppInstallInfo& new_web_app_info) {
  // TODO(crbug.com/411632946): Add test case: Update file handler in
  // manifest for an already installed app + override user choice by
  // adding the app to file handlers policy.
  if (provider_->registrar_unsafe().GetAppFileHandlerUserApprovalState(
          app_id) == ApiApprovalState::kDisallowed) {
    return FileHandlerUpdateAction::kNoUpdate;
  }

  // TODO(crbug.com/40176713): Consider trying to re-use the comparison
  // results from the ManifestUpdateDataFetchCommand.
  const apps::FileHandlers* old_handlers =
      provider_->registrar_unsafe().GetAppFileHandlers(app_id);
  DCHECK(old_handlers);
  if (*old_handlers == new_web_app_info.file_handlers)
    return FileHandlerUpdateAction::kNoUpdate;

  return FileHandlerUpdateAction::kUpdate;
}

}  // namespace web_app
