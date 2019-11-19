// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MOCK_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_ENGINE_MOCK_SYNC_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/sync_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

// A mock of the SyncEngine.
//
// Note: If you don't really care about all the exact details, FakeSyncEngine is
// probably better.
class MockSyncEngine : public SyncEngine {
 public:
  MockSyncEngine();
  ~MockSyncEngine() override;

  // ModelTypeConfigurer:
  MOCK_METHOD1(ConfigureDataTypes, void(ConfigureParams));
  MOCK_METHOD2(RegisterDirectoryDataType, void(ModelType, ModelSafeGroup));
  MOCK_METHOD1(UnregisterDirectoryDataType, void(ModelType));
  MOCK_METHOD3(ActivateDirectoryDataType,
               void(ModelType, ModelSafeGroup, ChangeProcessor*));
  MOCK_METHOD1(DeactivateDirectoryDataType, void(ModelType));
  MOCK_METHOD2(ActivateNonBlockingDataType,
               void(ModelType, std::unique_ptr<DataTypeActivationResponse>));
  MOCK_METHOD1(DeactivateNonBlockingDataType, void(ModelType));

  // SyncEngine:
  MOCK_METHOD1(Initialize, void(InitParams));
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(TriggerRefresh, void(const ModelTypeSet&));
  MOCK_METHOD1(UpdateCredentials, void(const SyncCredentials&));
  MOCK_METHOD0(InvalidateCredentials, void());
  MOCK_METHOD0(StartConfiguration, void());
  MOCK_METHOD0(StartSyncingWithServer, void());
  MOCK_METHOD1(SetEncryptionPassphrase, void(const std::string&));
  MOCK_METHOD1(SetDecryptionPassphrase, void(const std::string&));
  MOCK_METHOD2(AddTrustedVaultDecryptionKeys,
               void(const std::vector<std::string>&, base::OnceClosure));
  MOCK_METHOD0(StopSyncingForShutdown, void());
  MOCK_METHOD1(Shutdown, void(ShutdownReason));
  MOCK_METHOD0(EnableEncryptEverything, void());
  MOCK_CONST_METHOD0(GetUserShare, UserShare*());
  MOCK_METHOD0(GetDetailedStatus, SyncStatus());
  MOCK_CONST_METHOD1(HasUnsyncedItemsForTest,
                     void(base::OnceCallback<void(bool)>));
  MOCK_CONST_METHOD1(GetModelSafeRoutingInfo, void(ModelSafeRoutingInfo*));
  MOCK_CONST_METHOD0(FlushDirectory, void());
  MOCK_METHOD0(RequestBufferedProtocolEventsAndEnableForwarding, void());
  MOCK_METHOD0(DisableProtocolEventForwarding, void());
  MOCK_METHOD0(EnableDirectoryTypeDebugInfoForwarding, void());
  MOCK_METHOD0(DisableDirectoryTypeDebugInfoForwarding, void());
  MOCK_METHOD1(ClearServerData, void(const base::Closure&));
  MOCK_METHOD3(OnCookieJarChanged, void(bool, bool, const base::Closure&));
  MOCK_METHOD1(SetInvalidationsForSessionsEnabled, void(bool));
  MOCK_METHOD1(GetNigoriNodeForDebugging, void(AllNodesCallback));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MOCK_SYNC_ENGINE_H_
