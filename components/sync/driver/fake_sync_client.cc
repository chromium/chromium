// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/fake_sync_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

FakeSyncClient::FakeSyncClient()
    : bridge_(nullptr),
      factory_(nullptr),
      sync_service_(std::make_unique<FakeSyncService>()) {
  // Register sync preferences and set them to "Sync everything" state.
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  SyncPrefs sync_prefs(GetPrefService());
  sync_prefs.SetFirstSetupComplete();
  sync_prefs.SetKeepEverythingSynced(true);
}

FakeSyncClient::FakeSyncClient(SyncApiComponentFactory* factory)
    : factory_(factory), sync_service_(std::make_unique<FakeSyncService>()) {
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
}

FakeSyncClient::~FakeSyncClient() {}

SyncService* FakeSyncClient::GetSyncService() {
  return sync_service_.get();
}

PrefService* FakeSyncClient::GetPrefService() {
  return &pref_service_;
}

base::FilePath FakeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

ModelTypeStoreService* FakeSyncClient::GetModelTypeStoreService() {
  return nullptr;
}

bookmarks::BookmarkModel* FakeSyncClient::GetBookmarkModel() {
  return nullptr;
}

favicon::FaviconService* FakeSyncClient::GetFaviconService() {
  return nullptr;
}

history::HistoryService* FakeSyncClient::GetHistoryService() {
  return nullptr;
}

sync_sessions::SessionSyncService* FakeSyncClient::GetSessionSyncService() {
  return nullptr;
}

bool FakeSyncClient::HasPasswordStore() {
  return false;
}

base::Closure FakeSyncClient::GetPasswordStateChangedCallback() {
  return base::DoNothing();
}

DataTypeController::TypeVector FakeSyncClient::CreateDataTypeControllers(
    LocalDeviceInfoProvider* local_device_info_provider) {
  DCHECK(factory_);
  return factory_->CreateCommonDataTypeControllers(
      /*disabled_types=*/ModelTypeSet(), local_device_info_provider);
}

autofill::PersonalDataManager* FakeSyncClient::GetPersonalDataManager() {
  return nullptr;
}

BookmarkUndoService* FakeSyncClient::GetBookmarkUndoServiceIfExists() {
  return nullptr;
}

invalidation::InvalidationService* FakeSyncClient::GetInvalidationService() {
  return nullptr;
}

scoped_refptr<ExtensionsActivity> FakeSyncClient::GetExtensionsActivity() {
  return scoped_refptr<ExtensionsActivity>();
}

base::WeakPtr<SyncableService> FakeSyncClient::GetSyncableServiceForType(
    ModelType type) {
  return base::WeakPtr<SyncableService>();
}

base::WeakPtr<ModelTypeControllerDelegate>
FakeSyncClient::GetControllerDelegateForModelType(ModelType type) {
  return bridge_->change_processor()->GetControllerDelegate();
}

scoped_refptr<ModelSafeWorker> FakeSyncClient::CreateModelWorkerForGroup(
    ModelSafeGroup group) {
  return scoped_refptr<ModelSafeWorker>();
}

SyncApiComponentFactory* FakeSyncClient::GetSyncApiComponentFactory() {
  return factory_;
}

void FakeSyncClient::SetModelTypeSyncBridge(ModelTypeSyncBridge* bridge) {
  bridge_ = bridge;
}

}  // namespace syncer
