// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_database.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/webapps/common/web_app_id.h"

namespace base {
class Time;
}

namespace syncer {
class DataTypeLocalChangeProcessor;
struct EntityData;
class MetadataBatch;
class MetadataChangeList;
class ModelError;
class StringOrdinal;
}  // namespace syncer

namespace sync_pb {
class WebAppSpecifics;
}  // namespace sync_pb

namespace webapps {
enum class UninstallResultCode;
enum class InstallResultCode;
}  // namespace webapps

namespace web_app {

class AppLock;
class ScopedRegistryUpdate;
class WebApp;
class WebAppProvider;
class WebAppRegistryUpdate;
enum class ApiApprovalState;
struct RegistryUpdateData;

// These errors cause the sync entity to no longer be parsable, and results in
// `IsEntityDataValid` returning false below.
//
// Used in metrics, do not re-number or remove entities.
enum class StorageKeyParseResult {
  // This is needed for normalization
  kSuccess = 0,
  kNoStartUrl = 1,
  kInvalidStartUrl = 2,
  kInvalidManifestId = 3,
  kMaxValue = kInvalidManifestId
};

// After parsing the storage key, other problems with parsing the manifest id
// can occur. In the future, these errors could result in deletion of sync
// and/or local data to clean things up.
//
// Used in metrics, do not re-number or remove entities.
enum class ManifestIdParseResult {
  // This is needed for normalization.
  kSuccess = 0,
  // The origin of the start_url and resolved manifest_id do not match.
  kManifestIdResolutionFailure = 1,
  // The manifest_id resolved from sync doesn't match the local app's
  // manifest_id.
  kManifestIdDoesNotMatchLocalData = 2,
  kMaxValue = kManifestIdDoesNotMatchLocalData
};

// A unified sync and storage controller.
//
// While WebAppRegistrar is a read-only model, WebAppSyncBridge is a
// controller for that model. WebAppSyncBridge is responsible for:
// - Registry initialization (reading model from a persistent storage like
// LevelDb or prefs).
// - Writing all the registry updates to a persistent store and sync.
//
// WebAppSyncBridge is the key class to support integration with Unified Sync
// and Storage (USS) system. The sync bridge exclusively owns
// DataTypeLocalChangeProcessor and WebAppDatabase (the storage).
class WebAppSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  // Disable the logic that resumes pending sync installs, and fixes cases where
  // os integration is missing but the app's install_state indicates OS
  // integration should be present. Only intended for use in tests that need to
  // check the app state before these operations are done.
  static base::AutoReset<bool>
  DisableResumeSyncInstallAndMissingOsIntegrationForTesting();

  explicit WebAppSyncBridge(WebAppRegistrarMutable* registrar);
  // Tests may inject mocks using this ctor.
  WebAppSyncBridge(
      WebAppRegistrarMutable* registrar,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  WebAppSyncBridge(const WebAppSyncBridge&) = delete;
  WebAppSyncBridge& operator=(const WebAppSyncBridge&) = delete;
  ~WebAppSyncBridge() override;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  using CommitCallback = base::OnceCallback<void(bool success)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const webapps::AppId& app_id,
                                   webapps::UninstallResultCode code)>;
  // This is the writable API for the registry. Any updates will be written to
  // LevelDb and sync service. There can be only 1 update at a time. The
  // returned update will be committed to the database automatically on
  // destruction.
  //
  // Writes to the RAM database are synchronous. It is normally not necessary to
  // wait for the disk write to complete, because:
  // - All reads and writes happen from the RAM database.
  // - The disk database is only read during startup before the system starts,
  //   and is only written to during the rest of the browser's lifetime.
  //
  // The only reason waiting may be necessary here is to handle the edge case
  // that a crash happens and the operation wants to ensure that the disk state
  // is updated before continuing. This may be necessary for, say, a two-phase
  // commit involving another browser system with its own storage.
  [[nodiscard]] ScopedRegistryUpdate BeginUpdate(
      CommitCallback callback = base::DoNothing());

  void Init(base::OnceClosure callback);

  // Non testing code should use SetUserDisplayModeCommand instead.
  void SetAppUserDisplayModeForTesting(
      const webapps::AppId& app_id,
      mojom::UserDisplayMode user_display_mode);

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppIsDisabled(AppLock& lock,
                        const webapps::AppId& app_id,
                        bool is_disabled);

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void UpdateAppsDisableMode();

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppLastBadgingTime(const webapps::AppId& app_id,
                             const base::Time& time);

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppLastLaunchTime(const webapps::AppId& app_id,
                            const base::Time& time);

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppFirstInstallTime(const webapps::AppId& app_id,
                              const base::Time& time);

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppManifestUpdateTime(const webapps::AppId& app_id,
                                const base::Time& time);

  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppWindowControlsOverlayEnabled(const webapps::AppId& app_id,
                                          bool enabled);

  // These methods are used by extensions::AppSorting, which manages the sorting
  // of web apps on chrome://apps.
  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetUserPageOrdinal(const webapps::AppId& app_id,
                          syncer::StringOrdinal user_page_ordinal);
  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetUserLaunchOrdinal(const webapps::AppId& app_id,
                            syncer::StringOrdinal user_launch_ordinal);

  // Stores the user's preference for the app's use of the File Handling API.
  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAppFileHandlerApprovalState(const webapps::AppId& app_id,
                                      ApiApprovalState state);

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/41490924): Remove this and use a command instead.
  void SetAlwaysShowToolbarInFullscreen(const webapps::AppId& app_id,
                                        bool show);
#endif

  // An access to read-only registry. Does an upcast to read-only type.
  const WebAppRegistrar& registrar() const { return *registrar_; }

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // Signals that the sync system has received data from the server at some
  // point, potentially on a previous startup. Apps may still be installing or
  // uninstalling.
  const base::OneShotEvent& on_sync_connected() const {
    return on_sync_connected_;
  }

  // Used for testing only.
  void set_disable_checks_for_testing(bool disable_checks_for_testing) {
    disable_checks_for_testing_ = disable_checks_for_testing;
  }

  WebAppDatabase* GetDatabaseForTesting() const { return database_.get(); }

  // TODO(crbug.com/41490924): Remove this and make it so tests can
  // install via sync instead to reach this state.
  // Note: This doesn't synchronize the OS integration manager, so the os
  // integration state is not cleared.
  void SetAppNotLocallyInstalledForTesting(const webapps::AppId& app_id);

 private:
  void CommitUpdate(CommitCallback callback,
                    std::unique_ptr<WebAppRegistryUpdate> update);

  void CheckRegistryUpdateData(const RegistryUpdateData& update_data) const;

  // Update the in-memory model.
  void UpdateRegistrar(std::unique_ptr<RegistryUpdateData> update_data);

  // Update the remote sync server.
  void UpdateSync(const RegistryUpdateData& update_data,
                  syncer::MetadataChangeList* metadata_change_list);

  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void EnsureShortcutAppToDiyAppMigration();

  // Update apps that don't have a UserDisplayMode set for the current platform.
  void EnsureAppsHaveUserDisplayModeForCurrentPlatform();
  void EnsurePartiallyInstalledAppsHaveCorrectStatus();
  void OnDataWritten(CommitCallback callback, bool success);
  void OnWebAppUninstallComplete(const webapps::AppId& app,
                                 webapps::UninstallResultCode code);

  void ReportErrorToChangeProcessor(const syncer::ModelError& error);

  // Any local entities that don’t exist remotely must be provided to sync.
  void MergeLocalAppsToSync(const syncer::EntityChangeList& entity_data,
                            syncer::MetadataChangeList* metadata_change_list);

  // Returns if the data was parsed.
  ManifestIdParseResult PrepareLocalUpdateFromSyncChange(
      const syncer::EntityChange& change,
      RegistryUpdateData* update_local_data,
      std::vector<webapps::AppId>& apps_display_mode_changed);

  // Update registrar and Install/Uninstall missing/excessive local apps.
  void ApplyIncrementalSyncChangesToRegistrar(
      std::unique_ptr<RegistryUpdateData> update_local_data,
      const std::vector<webapps::AppId>& apps_display_mode_changed);

  void MaybeUninstallAppsPendingUninstall();
  void MaybeInstallAppsFromSyncAndPendingInstallOrSyncOsIntegration();

  std::unique_ptr<WebAppDatabase> database_;
  const raw_ptr<WebAppRegistrarMutable, DanglingUntriaged> registrar_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::OneShotEvent on_sync_connected_;

  bool is_in_update_ = false;
  bool disable_checks_for_testing_ = false;

  base::WeakPtrFactory<WebAppSyncBridge> weak_ptr_factory_{this};
};

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(const WebApp& app);

void ApplySyncDataToApp(const sync_pb::WebAppSpecifics& sync_data, WebApp* app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
