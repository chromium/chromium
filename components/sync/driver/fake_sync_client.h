// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_SYNC_CLIENT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace syncer {

class FakeSyncService;
class ModelTypeSyncBridge;

// Fake implementation of SyncClient interface for tests.
class FakeSyncClient : public SyncClient {
 public:
  FakeSyncClient();
  explicit FakeSyncClient(SyncApiComponentFactory* factory);
  ~FakeSyncClient() override;

  SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  ModelTypeStoreService* GetModelTypeStoreService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  bool HasPasswordStore() override;
  base::Closure GetPasswordStateChangedCallback() override;
  DataTypeController::TypeVector CreateDataTypeControllers(
      LocalDeviceInfoProvider* local_device_info_provider) override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  scoped_refptr<ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<SyncableService> GetSyncableServiceForType(
      ModelType type) override;
  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegateForModelType(
      ModelType type) override;
  scoped_refptr<ModelSafeWorker> CreateModelWorkerForGroup(
      ModelSafeGroup group) override;
  SyncApiComponentFactory* GetSyncApiComponentFactory() override;

  void SetModelTypeSyncBridge(ModelTypeSyncBridge* bridge);

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  ModelTypeSyncBridge* bridge_;
  SyncApiComponentFactory* factory_;
  std::unique_ptr<FakeSyncService> sync_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncClient);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_SYNC_CLIENT_H_
