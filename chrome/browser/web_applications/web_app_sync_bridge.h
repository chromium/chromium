// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_type_sync_bridge.h"

class Profile;

namespace syncer {
class MetadataBatch;
class MetadataChangeList;
class ModelError;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace sync_pb {
class WebAppSpecifics;
}  // namespace sync_pb

namespace web_app {

class AbstractWebAppDatabaseFactory;
class SyncInstallDelegate;
class WebAppDatabase;
class WebAppRegistryUpdate;
struct RegistryUpdateData;

// The sync bridge exclusively owns ModelTypeChangeProcessor and WebAppDatabase
// (the storage).
class WebAppSyncBridge : public AppRegistryController,
                         public syncer::ModelTypeSyncBridge {
 public:
  WebAppSyncBridge(Profile* profile,
                   AbstractWebAppDatabaseFactory* database_factory,
                   WebAppRegistrarMutable* registrar,
                   SyncInstallDelegate* install_delegate);
  // Tests may inject mocks using this ctor.
  WebAppSyncBridge(
      Profile* profile,
      AbstractWebAppDatabaseFactory* database_factory,
      WebAppRegistrarMutable* registrar,
      SyncInstallDelegate* install_delegate,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~WebAppSyncBridge() override;

  using CommitCallback = base::OnceCallback<void(bool success)>;
  // This is the writable API for the registry. Any updates will be written to
  // LevelDb and sync service. There can be only 1 update at a time.
  std::unique_ptr<WebAppRegistryUpdate> BeginUpdate();
  void CommitUpdate(std::unique_ptr<WebAppRegistryUpdate> update,
                    CommitCallback callback);

  // AppRegistryController:
  void Init(base::OnceClosure callback) override;
  void SetAppUserDisplayMode(const AppId& app_id,
                             DisplayMode user_display_mode) override;
  void SetAppIsLocallyInstalledForTesting(const AppId& app_id,
                                          bool is_locally_installed) override;
  WebAppSyncBridge* AsWebAppSyncBridge() override;

  // An access to read-only registry. Does an upcast to read-only type.
  const WebAppRegistrar& registrar() const { return *registrar_; }

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

 private:
  void CheckRegistryUpdateData(const RegistryUpdateData& update_data) const;

  // Update the in-memory model. Returns unregistered apps which may be
  // disposed.
  std::vector<std::unique_ptr<WebApp>> UpdateRegistrar(
      std::unique_ptr<RegistryUpdateData> update_data);

  // Update the remote sync server.
  void UpdateSync(const RegistryUpdateData& update_data,
                  syncer::MetadataChangeList* metadata_change_list);

  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnDataWritten(CommitCallback callback, bool success);

  void ReportErrorToChangeProcessor(const syncer::ModelError& error);

  // Any local entities that don’t exist remotely must be provided to sync.
  void MergeLocalAppsToSync(const syncer::EntityChangeList& entity_data,
                            syncer::MetadataChangeList* metadata_change_list);

  void ApplySyncDataChange(const syncer::EntityChange& change,
                           RegistryUpdateData* update_local_data);

  // Update registrar and Install/Uninstall missing/excessive local apps.
  void ApplySyncChangesToRegistrar(
      std::unique_ptr<RegistryUpdateData> update_local_data);

  void MaybeInstallAppsInSyncInstall();

  std::unique_ptr<WebAppDatabase> database_;
  WebAppRegistrarMutable* const registrar_;
  SyncInstallDelegate* const install_delegate_;

  bool is_in_update_ = false;

  base::WeakPtrFactory<WebAppSyncBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppSyncBridge);
};

bool AreAppsLocallyInstalledByDefault();

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(const WebApp& app);

void ApplySyncDataToApp(const sync_pb::WebAppSpecifics& sync_data, WebApp* app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_BRIDGE_H_
