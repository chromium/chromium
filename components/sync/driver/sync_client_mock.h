// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_MOCK_H_

#include "base/files/file_path.h"
#include "components/sync/driver/sync_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class SyncClientMock : public SyncClient {
 public:
  SyncClientMock();
  ~SyncClientMock() override;

  MOCK_METHOD0(GetSyncService, SyncService*());
  MOCK_METHOD0(GetPrefService, PrefService*());
  MOCK_METHOD0(GetLocalSyncBackendFolder, base::FilePath());
  MOCK_METHOD0(GetModelTypeStoreService, syncer::ModelTypeStoreService*());
  MOCK_METHOD0(GetBookmarkModel, bookmarks::BookmarkModel*());
  MOCK_METHOD0(GetFaviconService, favicon::FaviconService*());
  MOCK_METHOD0(GetHistoryService, history::HistoryService*());
  MOCK_METHOD0(HasPasswordStore, bool());
  MOCK_METHOD0(GetSessionSyncService, sync_sessions::SessionSyncService*());
  MOCK_METHOD1(CreateDataTypeControllers,
               DataTypeController::TypeVector(
                   LocalDeviceInfoProvider* local_device_info_provider));
  MOCK_METHOD0(GetPasswordStateChangedCallback, base::RepeatingClosure());

  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_METHOD0(GetBookmarkUndoServiceIfExists, BookmarkUndoService*());
  MOCK_METHOD0(GetInvalidationService, invalidation::InvalidationService*());
  MOCK_METHOD0(GetExtensionsActivity, scoped_refptr<ExtensionsActivity>());
  MOCK_METHOD1(GetSyncableServiceForType,
               base::WeakPtr<SyncableService>(ModelType type));
  MOCK_METHOD1(GetControllerDelegateForModelType,
               base::WeakPtr<ModelTypeControllerDelegate>(ModelType type));
  MOCK_METHOD1(CreateModelWorkerForGroup,
               scoped_refptr<ModelSafeWorker>(ModelSafeGroup group));
  MOCK_METHOD0(GetSyncApiComponentFactory, SyncApiComponentFactory*());

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncClientMock);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_MOCK_H_
