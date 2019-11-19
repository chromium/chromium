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

  MOCK_METHOD0(GetPrefService, PrefService*());
  MOCK_METHOD0(GetSyncDataPath, base::FilePath());
  MOCK_METHOD0(GetLocalSyncBackendFolder, base::FilePath());
  MOCK_METHOD1(CreateDataTypeControllers,
               DataTypeController::TypeVector(SyncService* sync_service));
  MOCK_METHOD0(GetPasswordStateChangedCallback, base::RepeatingClosure());

  MOCK_METHOD0(GetInvalidationService, invalidation::InvalidationService*());
  MOCK_METHOD0(GetTrustedVaultClient, TrustedVaultClient*());
  MOCK_METHOD0(GetExtensionsActivity, scoped_refptr<ExtensionsActivity>());
  MOCK_METHOD1(GetSyncableServiceForType,
               base::WeakPtr<SyncableService>(ModelType type));
  MOCK_METHOD1(CreateModelWorkerForGroup,
               scoped_refptr<ModelSafeWorker>(ModelSafeGroup group));
  MOCK_METHOD0(GetSyncApiComponentFactory, SyncApiComponentFactory*());
  MOCK_METHOD0(GetPreferenceProvider, SyncTypePreferenceProvider*());

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncClientMock);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_MOCK_H_
