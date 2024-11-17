// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_sync_bridge.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {
namespace {
// Returns the manifest id from the sync entity. Does not validate whether the
// manifest_id is valid.
base::expected<webapps::ManifestId, StorageKeyParseResult>
ParseManifestIdFromSyncEntity(const sync_pb::WebAppSpecifics& specifics) {
  // Validate the entity is not corrupt.
  if (!specifics.has_start_url()) {
    return base::unexpected(StorageKeyParseResult::kNoStartUrl);
  }

  const GURL start_url = GURL(specifics.start_url());
  if (!start_url.is_valid()) {
    return base::unexpected(StorageKeyParseResult::kInvalidStartUrl);
  }

  // Set the manifest id first, as ApplySyncDataToApp verifies that the
  // computed manifest ids match.
  webapps::ManifestId manifest_id;
  if (specifics.has_relative_manifest_id()) {
    manifest_id =
        GenerateManifestIdUnsafe(specifics.relative_manifest_id(), start_url);
  } else {
    manifest_id = GenerateManifestIdFromStartUrlOnly(start_url);
  }
  if (!manifest_id.is_valid()) {
    return base::unexpected(StorageKeyParseResult::kInvalidManifestId);
  }
  return base::ok(manifest_id);
}

base::expected<webapps::ManifestId, ManifestIdParseResult>
ValidateManifestIdFromParsableSyncEntity(
    const sync_pb::WebAppSpecifics& specifics,
    const WebApp* existing_web_app) {
  base::expected<webapps::ManifestId, StorageKeyParseResult> manifest_id =
      ParseManifestIdFromSyncEntity(specifics);
  // These are guaranteed to be true, as it is checked in IsEntityDataValid,
  // which prevents the entity from ever being given to our system.
  CHECK(manifest_id.has_value());
  CHECK(manifest_id->is_valid());
  GURL start_url = GURL(specifics.start_url());
  CHECK(start_url.is_valid());

  if (!url::IsSameOriginWith(start_url, manifest_id.value())) {
    return base::unexpected(
        ManifestIdParseResult::kManifestIdResolutionFailure);
  }

  if (existing_web_app && existing_web_app->manifest_id() != manifest_id) {
    return base::unexpected(
        ManifestIdParseResult::kManifestIdDoesNotMatchLocalData);
  }

  return base::ok(manifest_id.value());
}

}  // namespace

BASE_FEATURE(kDeleteBadWebAppSyncEntitites,
             "DeleteBadWebAppSyncEntitites",
             base::FEATURE_DISABLED_BY_DEFAULT);

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(const WebApp& app) {
  // The Sync System doesn't allow empty entity_data name.
  DCHECK(!app.untranslated_name().empty());

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = app.untranslated_name();
  // TODO(crbug.com/40139320): Remove this fallback later.
  if (entity_data->name.empty())
    entity_data->name = app.start_url().spec();

  *(entity_data->specifics.mutable_web_app()) = app.sync_proto();
  return entity_data;
}

void ApplySyncDataToApp(const sync_pb::WebAppSpecifics& sync_proto,
                        WebApp* app) {
  app->AddSource(WebAppManagement::kSync);

  sync_pb::WebAppSpecifics modified_sync_proto = sync_proto;

  std::string relative_manifest_id_path =
      RelativeManifestIdPath(app->manifest_id());
  if (modified_sync_proto.has_relative_manifest_id() &&
      modified_sync_proto.relative_manifest_id() != relative_manifest_id_path) {
    modified_sync_proto.set_relative_manifest_id(relative_manifest_id_path);
    // Record when this happens. When it is rare enough we could remove the
    // logic here and instead drop incoming sync data with fragment parts in the
    // manifest_id_path.
    base::UmaHistogramBoolean("WebApp.ApplySyncDataToApp.ManifestIdMatch",
                              false);
  } else {
    // Record success for comparison.
    base::UmaHistogramBoolean("WebApp.ApplySyncDataToApp.ManifestIdMatch",
                              true);
  }

  // Prevent incoming sync data from clearing recently-added fields in our local
  // copy. This ensures new sync fields are preserved despite old (pre-M125)
  // clients incorrectly clearing unknown fields. Any new fields added to the
  // sync proto should also be added here (if we don't want them to be cleared
  // by old clients) until this block can be removed. This can be removed when
  // there are few <M125 clients remaining.
  if (app->sync_proto().has_user_display_mode_cros() &&
      !modified_sync_proto.has_user_display_mode_cros()) {
    modified_sync_proto.set_user_display_mode_cros(
        app->sync_proto().user_display_mode_cros());
  }
  if (app->sync_proto().has_user_display_mode_default() &&
      !modified_sync_proto.has_user_display_mode_default()) {
    modified_sync_proto.set_user_display_mode_default(
        app->sync_proto().user_display_mode_default());
  }

  // Ensure the current platform's UserDisplayMode is set.
  // Conditional to avoid clobbering an unknown new UDM with a fallback one.
  if (!HasCurrentPlatformUserDisplayMode(modified_sync_proto)) {
    auto udm = ResolvePlatformSpecificUserDisplayMode(modified_sync_proto);
    SetPlatformSpecificUserDisplayMode(udm, &modified_sync_proto);
  }

  app->SetSyncProto(std::move(modified_sync_proto));
}

WebAppSyncBridge::WebAppSyncBridge(WebAppRegistrarMutable* registrar)
    : WebAppSyncBridge(
          registrar,
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::WEB_APPS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel()))) {}

WebAppSyncBridge::WebAppSyncBridge(
    WebAppRegistrarMutable* registrar,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      registrar_(registrar) {
  DCHECK(registrar_);
}

WebAppSyncBridge::~WebAppSyncBridge() = default;

void WebAppSyncBridge::SetSubsystems(
    AbstractWebAppDatabaseFactory* database_factory,
    WebAppCommandManager* command_manager,
    WebAppCommandScheduler* command_scheduler,
    WebAppInstallManager* install_manager) {
  DCHECK(database_factory);
  database_ = std::make_unique<WebAppDatabase>(
      database_factory,
      base::BindRepeating(&WebAppSyncBridge::ReportErrorToChangeProcessor,
                          base::Unretained(this)));
  command_manager_ = command_manager;
  command_scheduler_ = command_scheduler;
  install_manager_ = install_manager;
}

[[nodiscard]] ScopedRegistryUpdate WebAppSyncBridge::BeginUpdate(
    CommitCallback callback) {
  DCHECK(database_->is_opened());

  DCHECK(!is_in_update_);
  is_in_update_ = true;

  return ScopedRegistryUpdate(
      base::PassKey<WebAppSyncBridge>(),
      std::make_unique<WebAppRegistryUpdate>(registrar_,
                                             base::PassKey<WebAppSyncBridge>()),
      base::BindOnce(&WebAppSyncBridge::CommitUpdate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppSyncBridge::Init(base::OnceClosure initialized_callback) {
  database_->OpenDatabase(base::BindOnce(&WebAppSyncBridge::OnDatabaseOpened,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(initialized_callback)));
}

void WebAppSyncBridge::SetAppUserDisplayModeForTesting(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetUserDisplayMode(user_display_mode);
    }
  }

  registrar_->NotifyWebAppUserDisplayModeChanged(app_id, user_display_mode);
}

void WebAppSyncBridge::SetAppWindowControlsOverlayEnabled(
    const webapps::AppId& app_id,
    bool enabled) {
  ScopedRegistryUpdate update = BeginUpdate();
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app) {
    web_app->SetWindowControlsOverlayEnabled(enabled);
  }
}

void WebAppSyncBridge::SetAppIsDisabled(AppLock& lock,
                                        const webapps::AppId& app_id,
                                        bool is_disabled) {
  if (!IsChromeOsDataMandatory()) {
    return;
  }

  bool notify = false;
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (!web_app) {
      return;
    }

    std::optional<WebAppChromeOsData> cros_data = web_app->chromeos_data();
    DCHECK(cros_data.has_value());

    if (cros_data->is_disabled != is_disabled) {
      cros_data->is_disabled = is_disabled;
      web_app->SetWebAppChromeOsData(std::move(cros_data));
      notify = true;
    }
  }

  if (notify) {
    registrar_->NotifyWebAppDisabledStateChanged(app_id, is_disabled);
  }
}

void WebAppSyncBridge::UpdateAppsDisableMode() {
  if (!IsChromeOsDataMandatory()) {
    return;
  }

  registrar_->NotifyWebAppsDisabledModeChanged();
}

void WebAppSyncBridge::SetAppLastBadgingTime(const webapps::AppId& app_id,
                                             const base::Time& time) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetLastBadgingTime(time);
    }
  }
  registrar_->NotifyWebAppLastBadgingTimeChanged(app_id, time);
}

void WebAppSyncBridge::SetAppLastLaunchTime(const webapps::AppId& app_id,
                                            const base::Time& time) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetLastLaunchTime(time);
    }
  }
  registrar_->NotifyWebAppLastLaunchTimeChanged(app_id, time);
}

void WebAppSyncBridge::SetAppFirstInstallTime(const webapps::AppId& app_id,
                                              const base::Time& time) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetFirstInstallTime(time);
    }
  }
  registrar_->NotifyWebAppFirstInstallTimeChanged(app_id, time);
}

void WebAppSyncBridge::SetAppManifestUpdateTime(const webapps::AppId& app_id,
                                                const base::Time& time) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetManifestUpdateTime(time);
    }
  }
}

void WebAppSyncBridge::SetUserPageOrdinal(const webapps::AppId& app_id,
                                          syncer::StringOrdinal page_ordinal) {
  CHECK(page_ordinal.IsValid(), base::NotFatalUntil::M126);
  ScopedRegistryUpdate update = BeginUpdate();
  WebApp* web_app = update->UpdateApp(app_id);
  // Due to the extensions sync system setting ordinals on sync, this can get
  // called before the app is installed in the web apps system. Until apps are
  // no longer double-installed on both systems, ignore this case.
  // https://crbug.com/1101781
  if (!registrar_->IsInstalled(app_id)) {
    return;
  }
  if (web_app) {
    sync_pb::WebAppSpecifics mutable_sync_proto = web_app->sync_proto();
    mutable_sync_proto.set_user_page_ordinal(page_ordinal.ToInternalValue());
    web_app->SetSyncProto(std::move(mutable_sync_proto));
  }
}

void WebAppSyncBridge::SetUserLaunchOrdinal(
    const webapps::AppId& app_id,
    syncer::StringOrdinal launch_ordinal) {
  CHECK(launch_ordinal.IsValid(), base::NotFatalUntil::M126);
  ScopedRegistryUpdate update = BeginUpdate();
  // Due to the extensions sync system setting ordinals on sync, this can get
  // called before the app is installed in the web apps system. Until apps are
  // no longer double-installed on both systems, ignore this case.
  // https://crbug.com/1101781
  if (!registrar_->IsInstalled(app_id)) {
    return;
  }
  WebApp* web_app = update->UpdateApp(app_id);
  if (web_app) {
    sync_pb::WebAppSpecifics mutable_sync_proto = web_app->sync_proto();
    mutable_sync_proto.set_user_launch_ordinal(
        launch_ordinal.ToInternalValue());
    web_app->SetSyncProto(std::move(mutable_sync_proto));
  }
}

#if BUILDFLAG(IS_MAC)
void WebAppSyncBridge::SetAlwaysShowToolbarInFullscreen(
    const webapps::AppId& app_id,
    bool show) {
  if (!registrar_->IsInstalled(app_id)) {
    return;
  }
  {
    ScopedRegistryUpdate update = BeginUpdate();
    update->UpdateApp(app_id)->SetAlwaysShowToolbarInFullscreen(show);
  }
  registrar_->NotifyAlwaysShowToolbarInFullscreenChanged(app_id, show);
}
#endif

void WebAppSyncBridge::SetAppFileHandlerApprovalState(
    const webapps::AppId& app_id,
    ApiApprovalState state) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    update->UpdateApp(app_id)->SetFileHandlerApprovalState(state);
  }
  registrar_->NotifyWebAppFileHandlerApprovalStateChanged(app_id);
}

void WebAppSyncBridge::CommitUpdate(
    CommitCallback callback,
    std::unique_ptr<WebAppRegistryUpdate> update) {
  DCHECK(is_in_update_);
  is_in_update_ = false;

  if (update == nullptr) {
    std::move(callback).Run(/*success*/ true);
    return;
  }

  std::unique_ptr<RegistryUpdateData> update_data =
      update->TakeUpdateData(base::PassKey<WebAppSyncBridge>());

  // Remove all unchanged apps.
  RegistryUpdateData::Apps changed_apps_to_update;
  for (std::unique_ptr<WebApp>& app_to_update : update_data->apps_to_update) {
    const webapps::AppId& app_id = app_to_update->app_id();
    if (*app_to_update != *registrar().GetAppById(app_id)) {
      changed_apps_to_update.push_back(std::move(app_to_update));
    }
  }
  update_data->apps_to_update = std::move(changed_apps_to_update);

  if (update_data->IsEmpty()) {
    std::move(callback).Run(/*success*/ true);
    return;
  }

  if (!disable_checks_for_testing_) {
    CheckRegistryUpdateData(*update_data);
  }

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  UpdateSync(*update_data, metadata_change_list.get());

  database_->Write(
      *update_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  UpdateRegistrar(std::move(update_data));
}

void WebAppSyncBridge::CheckRegistryUpdateData(
    const RegistryUpdateData& update_data) const {
#if DCHECK_IS_ON()
  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_create) {
    DCHECK(!registrar_->GetAppById(web_app->app_id()));
    DCHECK(!web_app->untranslated_name().empty());
    DCHECK(web_app->manifest_id().is_valid());
  }

  for (const std::unique_ptr<WebApp>& web_app : update_data.apps_to_update) {
    DCHECK(registrar_->GetAppById(web_app->app_id()));
    DCHECK(!web_app->untranslated_name().empty());
    DCHECK(web_app->manifest_id().is_valid());
  }

  for (const webapps::AppId& app_id : update_data.apps_to_delete) {
    DCHECK(registrar_->GetAppById(app_id));
  }
#endif
}

void WebAppSyncBridge::UpdateRegistrar(
    std::unique_ptr<RegistryUpdateData> update_data) {
  registrar_->CountMutation();

  for (std::unique_ptr<WebApp>& web_app : update_data->apps_to_create) {
    webapps::AppId app_id = web_app->app_id();
    DCHECK(!registrar_->GetAppById(app_id));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // We do not install non-system web apps in Ash when Lacros web apps are
    // enabled.
    DCHECK(web_app->IsSystemApp() || !IsWebAppsCrosapiEnabled());
#endif
    registrar_->registry().emplace(std::move(app_id), std::move(web_app));
  }

  for (std::unique_ptr<WebApp>& web_app : update_data->apps_to_update) {
    WebApp* original_web_app = registrar_->GetAppByIdMutable(web_app->app_id());
    DCHECK(original_web_app);
    DCHECK_EQ(web_app->IsSystemApp(), original_web_app->IsSystemApp());
    // Commit previously created copy into original. Preserve original web_app
    // object pointer value (the object's identity) to support stored pointers.
    *original_web_app = std::move(*web_app);
  }
  for (const webapps::AppId& app_id : update_data->apps_to_delete) {
    auto it = registrar_->registry().find(app_id);
    CHECK(it != registrar_->registry().end(), base::NotFatalUntil::M130);
    registrar_->registry().erase(it);
  }
}

void WebAppSyncBridge::UpdateSync(
    const RegistryUpdateData& update_data,
    syncer::MetadataChangeList* metadata_change_list) {
  // We don't block web app subsystems on WebAppSyncBridge::MergeFullSyncData:
  // we call WebAppProvider::OnRegistryControllerReady() right after
  // change_processor()->ModelReadyToSync. As a result, subsystems may produce
  // some local changes between OnRegistryControllerReady and MergeFullSyncData.
  // Return early in this case. The processor cannot do any useful metadata
  // tracking until MergeFullSyncData is called:
  if (!change_processor()->IsTrackingMetadata())
    return;

  for (const std::unique_ptr<WebApp>& new_app : update_data.apps_to_create) {
    if (new_app->IsSynced()) {
      CHECK(new_app->manifest_id().is_valid(), base::NotFatalUntil::M125);
      change_processor()->Put(new_app->app_id(), CreateSyncEntityData(*new_app),
                              metadata_change_list);
    }
  }

  for (const std::unique_ptr<WebApp>& new_state : update_data.apps_to_update) {
    const webapps::AppId& app_id = new_state->app_id();
    // Find the current state of the app to be overritten.
    const WebApp* current_state = registrar_->GetAppById(app_id);
    DCHECK(current_state);

    // Include the app in the sync "view" if IsSynced flag becomes true. Update
    // the app if IsSynced flag stays true. Exclude the app from the sync "view"
    // if IsSynced flag becomes false.
    if (new_state->IsSynced()) {
      CHECK(new_state->manifest_id().is_valid(), base::NotFatalUntil::M125);
      change_processor()->Put(app_id, CreateSyncEntityData(*new_state),
                              metadata_change_list);
    } else if (current_state->IsSynced()) {
      change_processor()->Delete(app_id, syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list);
    }
  }

  for (const webapps::AppId& app_id_to_delete : update_data.apps_to_delete) {
    const WebApp* current_state = registrar_->GetAppById(app_id_to_delete);
    DCHECK(current_state);
    // Exclude the app from the sync "view" if IsSynced flag was true.
    if (current_state->IsSynced())
      change_processor()->Delete(app_id_to_delete,
                                 syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list);
  }
}

void WebAppSyncBridge::OnDatabaseOpened(
    base::OnceClosure initialized_callback,
    Registry registry,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK(database_->is_opened());

  // Provide sync metadata to the processor _before_ any local changes occur.
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  registrar_->InitRegistry(std::move(registry));

  // Do database migrations to ensure apps are valid before notifying anything
  // else that the sync bridge is ready.
  EnsureAppsHaveUserDisplayModeForCurrentPlatform();
  EnsurePartiallyInstalledAppsHaveCorrectStatus();

  std::move(initialized_callback).Run();

  // Already have data stored in web app system and shouldn't expect further
  // callbacks once `IsTrackingMetadata` is true.
  if (!on_sync_connected_.is_signaled() &&
      change_processor()->IsTrackingMetadata()) {
    on_sync_connected_.Signal();
  }

  MaybeUninstallAppsPendingUninstall();
  MaybeInstallAppsFromSyncAndPendingInstallation();
}

void WebAppSyncBridge::EnsureAppsHaveUserDisplayModeForCurrentPlatform() {
  web_app::ScopedRegistryUpdate update = BeginUpdate();
  for (const WebApp& app : registrar().GetAppsIncludingStubs()) {
    if (!HasCurrentPlatformUserDisplayMode(app.sync_proto())) {
      // On CrOS, populate the UDM-CrOS value by copying from the default value
      // (falling back to Standalone). On non-CrOS, populate the UDM-Default
      // value with Standalone.
      sync_pb::WebAppSpecifics_UserDisplayMode udm =
          ResolvePlatformSpecificUserDisplayMode(app.sync_proto());
      update->UpdateApp(app.app_id())
          ->SetUserDisplayMode(ToMojomUserDisplayMode(udm));
    }
  }
}

void WebAppSyncBridge::EnsurePartiallyInstalledAppsHaveCorrectStatus() {
  web_app::ScopedRegistryUpdate update = BeginUpdate();
  for (const WebApp& app : registrar().GetApps()) {
    if (app.install_state() !=
        proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
      continue;
    }
    if (app.current_os_integration_states().has_shortcut()) {
      continue;
    }
    update->UpdateApp(app.app_id())
        ->SetInstallState(
            proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  }
}

void WebAppSyncBridge::OnDataWritten(CommitCallback callback, bool success) {
  if (!success)
    DLOG(ERROR) << "WebAppSyncBridge commit failed";

  base::UmaHistogramBoolean("WebApp.Database.WriteResult", success);
  std::move(callback).Run(success);
}

void WebAppSyncBridge::OnWebAppUninstallComplete(
    const webapps::AppId& app,
    webapps::UninstallResultCode code) {
  base::UmaHistogramBoolean("Webapp.SyncInitiatedUninstallResult",
                            UninstallSucceeded(code));
}

void WebAppSyncBridge::ReportErrorToChangeProcessor(
    const syncer::ModelError& error) {
  change_processor()->ReportError(error);
}

void WebAppSyncBridge::MergeLocalAppsToSync(
    const syncer::EntityChangeList& entity_data,
    syncer::MetadataChangeList* metadata_change_list) {
  auto sync_server_apps = base::MakeFlatSet<webapps::AppId>(
      entity_data, {}, &syncer::EntityChange::storage_key);

  for (const WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (!app.IsSynced())
      continue;

    bool exists_remotely = sync_server_apps.contains(app.app_id());
    if (!exists_remotely) {
      change_processor()->Put(app.app_id(), CreateSyncEntityData(app),
                              metadata_change_list);
    }
  }
}

ManifestIdParseResult WebAppSyncBridge::PrepareLocalUpdateFromSyncChange(
    const syncer::EntityChange& change,
    RegistryUpdateData* update_local_data,
    std::vector<webapps::AppId>& apps_display_mode_changed) {
  // app_id is storage key.
  const webapps::AppId& app_id = change.storage_key();

  const WebApp* existing_web_app = registrar_->GetAppById(app_id);

  // Handle deletion first.
  if (change.type() == syncer::EntityChange::ACTION_DELETE) {
    if (!existing_web_app) {
      DLOG(ERROR) << "ApplySyncDataChange error: no app to delete";
      return ManifestIdParseResult::kSuccess;
    }
    auto app_copy = std::make_unique<WebApp>(*existing_web_app);
    app_copy->RemoveSource(WebAppManagement::kSync);
    // Currently removing an app from sync will uninstall the app on all
    // profiles that are synced to it; we could consider not removing the
    // kUserInstalled source in this case.
    app_copy->RemoveSource(WebAppManagement::kUserInstalled);
    if (!app_copy->HasAnySources()) {
      // Uninstallation from the local database is a two-phase commit. Setting
      // this flag to true signals that uninstallation should occur, and then
      // when all asynchronous uninstallation tasks are complete then the entity
      // is deleted from the database.
      app_copy->SetIsUninstalling(true);
    }
    update_local_data->apps_to_update.push_back(std::move(app_copy));
    return ManifestIdParseResult::kSuccess;
  }

  // Handle EntityChange::ACTION_ADD and EntityChange::ACTION_UPDATE.
  CHECK(change.data().specifics.has_web_app());
  const sync_pb::WebAppSpecifics& specifics = change.data().specifics.web_app();

  base::expected<webapps::ManifestId, ManifestIdParseResult> manifest_id =
      ValidateManifestIdFromParsableSyncEntity(specifics, existing_web_app);

  if (!manifest_id.has_value()) {
    base::UmaHistogramEnumeration("WebApp.Sync.CorruptSyncEntity",
                                  manifest_id.error());
    return manifest_id.error();
  }
  base::UmaHistogramEnumeration("WebApp.Sync.CorruptSyncEntity",
                                ManifestIdParseResult::kSuccess);

  std::unique_ptr<WebApp> web_app;

  if (!existing_web_app) {
    // Any remote entities that don’t exist locally must be written to local
    // storage.
    web_app = std::make_unique<WebApp>(app_id);
    web_app->SetStartUrl(GURL(specifics.start_url()));
    web_app->SetManifestId(manifest_id.value());

    // Request a followup sync-initiated install for this stub app to fetch
    // full local data and all the icons.
    web_app->SetIsFromSyncAndPendingInstallation(true);
    // The sync system requires non-empty name, populate temp name from
    // the fallback sync data name.
    if (specifics.name().empty()) {
      web_app->SetName(change.data().name);
    } else {
      web_app->SetName(specifics.name());
    }
    // For a new app, automatically choose if we want to install it locally.
    web_app->SetInstallState(
        AreAppsLocallyInstalledBySync()
            ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
            : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
  } else {
    web_app = std::make_unique<WebApp>(*existing_web_app);
  }

  ApplySyncDataToApp(specifics, web_app.get());

  if (existing_web_app) {
    if (existing_web_app->user_display_mode() != web_app->user_display_mode()) {
      apps_display_mode_changed.push_back(app_id);
    }
    update_local_data->apps_to_update.push_back(std::move(web_app));
  } else {
    update_local_data->apps_to_create.push_back(std::move(web_app));
  }
  return ManifestIdParseResult::kSuccess;
}

void WebAppSyncBridge::ApplyIncrementalSyncChangesToRegistrar(
    std::unique_ptr<RegistryUpdateData> update_local_data,
    const std::vector<webapps::AppId>& apps_display_mode_changed) {
  if (update_local_data->IsEmpty())
    return;

  // Notify observers that web apps will be updated.
  // Prepare a short living read-only "view" to support const correctness:
  // observers must not modify the |new_apps_state|.
  if (!update_local_data->apps_to_update.empty()) {
    std::vector<const WebApp*> new_apps_state;
    new_apps_state.reserve(update_local_data->apps_to_update.size());
    for (const std::unique_ptr<WebApp>& new_web_app_state :
         update_local_data->apps_to_update) {
      new_apps_state.push_back(new_web_app_state.get());
    }
    registrar_->NotifyWebAppsWillBeUpdatedFromSync(new_apps_state);
  }

  std::vector<WebApp*> apps_to_install;
  for (const auto& web_app : update_local_data->apps_to_create)
    apps_to_install.push_back(web_app.get());

  UpdateRegistrar(std::move(update_local_data));

  for (const webapps::AppId& app_id : apps_display_mode_changed) {
    const WebApp* app = registrar_->GetAppById(app_id);
    registrar_->NotifyWebAppUserDisplayModeChanged(app_id,
                                                   app->user_display_mode());
  }

  std::vector<webapps::AppId> apps_to_delete;
  for (const WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (app.is_uninstalling())
      apps_to_delete.push_back(app.app_id());
  }

  // Initiate any uninstall actions to clean up os integration, disk data, etc.
  if (!apps_to_delete.empty()) {
    auto callback =
        base::BindRepeating(&WebAppSyncBridge::OnWebAppUninstallComplete,
                            weak_ptr_factory_.GetWeakPtr());
    if (uninstall_from_sync_before_registry_update_callback_for_testing_) {
      uninstall_from_sync_before_registry_update_callback_for_testing_.Run(
          apps_to_delete, callback);
    } else {
      for (const webapps::AppId& app_id : apps_to_delete) {
        command_scheduler_->RemoveAllManagementTypesAndUninstall(
            base::PassKey<WebAppSyncBridge>(), app_id,
            webapps::WebappUninstallSource::kSync,
            base::BindOnce(callback, app_id));
      }
    }
  }

  // Do a full follow up install for all remote entities that don’t exist
  // locally.
  if (!apps_to_install.empty()) {
    // TODO(dmurph): Just call the InstallFromSync command.
    // https://crbug.com/1328968
    InstallWebAppsAfterSync(std::move(apps_to_install), base::DoNothing());
  }
}

std::unique_ptr<syncer::MetadataChangeList>
WebAppSyncBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> WebAppSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  CHECK(change_processor()->IsTrackingMetadata());

  auto update_local_data = std::make_unique<RegistryUpdateData>();
  std::vector<webapps::AppId> apps_display_mode_changed;

  for (const auto& change : entity_data) {
    DCHECK_NE(change->type(), syncer::EntityChange::ACTION_DELETE);
    ManifestIdParseResult result = PrepareLocalUpdateFromSyncChange(
        *change, update_local_data.get(), apps_display_mode_changed);
    if (base::FeatureList::IsEnabled(kDeleteBadWebAppSyncEntitites) &&
        result != ManifestIdParseResult::kSuccess) {
      change_processor()->Delete(GetStorageKey(change->data()),
                                 syncer::DeletionOrigin::Unspecified(),
                                 metadata_change_list.get());
    }
  }

  MergeLocalAppsToSync(entity_data, metadata_change_list.get());

  database_->Write(
      *update_local_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistrar(std::move(update_local_data),
                                         apps_display_mode_changed);

  if (!on_sync_connected_.is_signaled()) {
    on_sync_connected_.Signal();
  }

  return std::nullopt;
}

std::optional<syncer::ModelError> WebAppSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // `change_processor()->IsTrackingMetadata()` may be false if the sync
  // metadata is invalid and ClearPersistedMetadataIfInvalid() is resetting it.

  auto update_local_data = std::make_unique<RegistryUpdateData>();
  std::vector<webapps::AppId> apps_display_mode_changed;

  for (const auto& change : entity_changes) {
    PrepareLocalUpdateFromSyncChange(*change, update_local_data.get(),
                                     apps_display_mode_changed);
  }

  database_->Write(
      *update_local_data, std::move(metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistrar(std::move(update_local_data),
                                         apps_display_mode_changed);

  if (!on_sync_connected_.is_signaled()) {
    on_sync_connected_.Signal();
  }

  return std::nullopt;
}

void WebAppSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  if (!base::FeatureList::IsEnabled(
          features::kWebAppDontAddExistingAppsToSync)) {
    syncer::DataTypeSyncBridge::ApplyDisableSyncChanges(
        std::move(delete_metadata_change_list));
    return;
  }

  auto update_local_data = std::make_unique<RegistryUpdateData>();

  for (const WebApp& web_app : registrar_->GetAppsIncludingStubs()) {
    if (web_app.GetSources().Has(WebAppManagement::kSync)) {
      auto app_copy = std::make_unique<WebApp>(web_app);
      app_copy->RemoveSource(WebAppManagement::kSync);
      if (!app_copy->HasAnySources()) {
        // Uninstallation from the local database is a two-phase commit. Setting
        // this flag to true signals that uninstallation should occur, and then
        // when all asynchronous uninstallation tasks are complete then the
        // entity is deleted from the database.
        app_copy->SetIsUninstalling(true);
      }
      update_local_data->apps_to_update.push_back(std::move(app_copy));
    }
  }

  database_->Write(
      *update_local_data, std::move(delete_metadata_change_list),
      base::BindOnce(&WebAppSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistrar(std::move(update_local_data),
                                         /*apps_display_mode_changed=*/{});
}

std::unique_ptr<syncer::DataBatch> WebAppSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const webapps::AppId& app_id : storage_keys) {
    const WebApp* app = registrar_->GetAppById(app_id);
    if (app && app->IsSynced())
      data_batch->Put(app->app_id(), CreateSyncEntityData(*app));
  }

  return data_batch;
}

std::unique_ptr<syncer::DataBatch> WebAppSyncBridge::GetAllDataForDebugging() {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (app.IsSynced())
      data_batch->Put(app.app_id(), CreateSyncEntityData(app));
  }

  return data_batch;
}

std::string WebAppSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  CHECK(entity_data.specifics.has_web_app(), base::NotFatalUntil::M125);
  base::expected<webapps::ManifestId, StorageKeyParseResult> manifest_id =
      ParseManifestIdFromSyncEntity(entity_data.specifics.web_app());
  // This is guaranteed to be true, as the contract for this function is that
  // IsEntityDataValid must be true.
  CHECK(manifest_id.has_value(), base::NotFatalUntil::M125);
  return GenerateAppIdFromManifestId(manifest_id.value());
}

std::string WebAppSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

bool WebAppSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  if (!entity_data.specifics.has_web_app()) {
    return false;
  }
  const sync_pb::WebAppSpecifics& specifics = entity_data.specifics.web_app();

  base::expected<webapps::ManifestId, StorageKeyParseResult> manifest_id =
      ParseManifestIdFromSyncEntity(specifics);
  if (manifest_id.has_value()) {
    base::UmaHistogramEnumeration("WebApp.Sync.InvalidEntity",
                                  StorageKeyParseResult::kSuccess);
    return true;
  }
  // Note: The GetClientTag function relies on this function to always return
  // `false` if the manifest id is not parsable, and otherwise will CHECK-fail.
  base::UmaHistogramEnumeration("WebApp.Sync.InvalidEntity",
                                manifest_id.error());
  DLOG(ERROR) << "Cannot parse sync entity: "
              << base::ToString(manifest_id.error());
  return false;
}

void WebAppSyncBridge::SetRetryIncompleteUninstallsCallbackForTesting(
    RetryIncompleteUninstallsCallback callback) {
  retry_incomplete_uninstalls_callback_for_testing_ = std::move(callback);
}

void WebAppSyncBridge::SetInstallWebAppsAfterSyncCallbackForTesting(
    InstallWebAppsAfterSyncCallback callback) {
  install_web_apps_after_sync_callback_for_testing_ = std::move(callback);
}

void WebAppSyncBridge::SetUninstallFromSyncCallbackForTesting(
    UninstallFromSyncCallback callback) {
  uninstall_from_sync_before_registry_update_callback_for_testing_ =
      std::move(callback);
}

void WebAppSyncBridge::SetAppNotLocallyInstalledForTesting(
    const webapps::AppId& app_id) {
  {
    ScopedRegistryUpdate update = BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    if (web_app) {
      web_app->SetInstallState(
          proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
    }
  }
}

void WebAppSyncBridge::MaybeUninstallAppsPendingUninstall() {
  std::vector<webapps::AppId> apps_uninstalling;

  for (WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (app.is_uninstalling())
      apps_uninstalling.push_back(app.app_id());
  }

  base::UmaHistogramCounts100("WebApp.Uninstall.NonSyncIncompleteCount",
                              apps_uninstalling.size());

  // Retrying incomplete uninstalls
  if (!apps_uninstalling.empty()) {
    if (retry_incomplete_uninstalls_callback_for_testing_) {
      retry_incomplete_uninstalls_callback_for_testing_.Run(apps_uninstalling);
      return;
    }
    auto callback =
        base::BindRepeating(&WebAppSyncBridge::OnWebAppUninstallComplete,
                            weak_ptr_factory_.GetWeakPtr());
    for (const auto& app_id : apps_uninstalling) {
      command_scheduler_->RemoveAllManagementTypesAndUninstall(
          base::PassKey<WebAppSyncBridge>(), app_id,
          webapps::WebappUninstallSource::kSync,
          base::BindOnce(callback, app_id));
    }
  }
}

void WebAppSyncBridge::MaybeInstallAppsFromSyncAndPendingInstallation() {
  std::vector<WebApp*> apps_in_sync_install;

  for (WebApp& app : registrar_->GetAppsIncludingStubs()) {
    if (app.is_from_sync_and_pending_installation())
      apps_in_sync_install.push_back(&app);
  }

  if (!apps_in_sync_install.empty()) {
    if (install_web_apps_after_sync_callback_for_testing_) {
      install_web_apps_after_sync_callback_for_testing_.Run(
          std::move(apps_in_sync_install), base::DoNothing());
      return;
    }
    InstallWebAppsAfterSync(std::move(apps_in_sync_install), base::DoNothing());
  }
}

void WebAppSyncBridge::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps,
    RepeatingInstallCallback callback) {
  if (install_web_apps_after_sync_callback_for_testing_) {
    install_web_apps_after_sync_callback_for_testing_.Run(std::move(web_apps),
                                                          callback);
    return;
  }
  for (WebApp* web_app : web_apps) {
    command_scheduler_->InstallFromSync(*web_app, base::DoNothing());
  }
}

}  // namespace web_app
