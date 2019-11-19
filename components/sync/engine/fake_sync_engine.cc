// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/fake_sync_engine.h"

#include <utility>

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

constexpr char FakeSyncEngine::kTestCacheGuid[];
constexpr char FakeSyncEngine::kTestBirthday[];
constexpr char FakeSyncEngine::kTestKeystoreKey[];

FakeSyncEngine::FakeSyncEngine() {}
FakeSyncEngine::~FakeSyncEngine() {}

void FakeSyncEngine::Initialize(InitParams params) {
  bool success = !fail_initial_download_;
  initialized_ = success;
  params.host->OnEngineInitialized(
      ModelTypeSet(), WeakHandle<JsBackend>(),
      WeakHandle<DataTypeDebugInfoListener>(), kTestCacheGuid, kTestBirthday,
      /*bag_of_chips=*/"", kTestKeystoreKey, success);
}

bool FakeSyncEngine::IsInitialized() const {
  return initialized_;
}

void FakeSyncEngine::TriggerRefresh(const ModelTypeSet& types) {}

void FakeSyncEngine::UpdateCredentials(const SyncCredentials& credentials) {}

void FakeSyncEngine::InvalidateCredentials() {}

void FakeSyncEngine::StartConfiguration() {}

void FakeSyncEngine::StartSyncingWithServer() {}

void FakeSyncEngine::SetEncryptionPassphrase(const std::string& passphrase) {}

void FakeSyncEngine::SetDecryptionPassphrase(const std::string& passphrase) {}

void FakeSyncEngine::AddTrustedVaultDecryptionKeys(
    const std::vector<std::string>& keys,
    base::OnceClosure done_cb) {
  std::move(done_cb).Run();
}

void FakeSyncEngine::StopSyncingForShutdown() {}

void FakeSyncEngine::Shutdown(ShutdownReason reason) {}

void FakeSyncEngine::ConfigureDataTypes(ConfigureParams params) {}

void FakeSyncEngine::RegisterDirectoryDataType(ModelType type,
                                               ModelSafeGroup group) {}

void FakeSyncEngine::UnregisterDirectoryDataType(ModelType type) {}

void FakeSyncEngine::EnableEncryptEverything() {}

void FakeSyncEngine::ActivateDirectoryDataType(
    ModelType type,
    ModelSafeGroup group,
    ChangeProcessor* change_processor) {}
void FakeSyncEngine::DeactivateDirectoryDataType(ModelType type) {}

void FakeSyncEngine::ActivateNonBlockingDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {}

void FakeSyncEngine::DeactivateNonBlockingDataType(ModelType type) {}

UserShare* FakeSyncEngine::GetUserShare() const {
  return nullptr;
}

SyncStatus FakeSyncEngine::GetDetailedStatus() {
  return SyncStatus();
}

void FakeSyncEngine::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {}

void FakeSyncEngine::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const {}

void FakeSyncEngine::FlushDirectory() const {}

void FakeSyncEngine::RequestBufferedProtocolEventsAndEnableForwarding() {}

void FakeSyncEngine::DisableProtocolEventForwarding() {}

void FakeSyncEngine::EnableDirectoryTypeDebugInfoForwarding() {}

void FakeSyncEngine::DisableDirectoryTypeDebugInfoForwarding() {}

void FakeSyncEngine::set_fail_initial_download(bool should_fail) {
  fail_initial_download_ = should_fail;
}

void FakeSyncEngine::OnCookieJarChanged(bool account_mismatch,
                                        bool empty_jar,
                                        const base::Closure& callback) {
  if (!callback.is_null()) {
    callback.Run();
  }
}

void FakeSyncEngine::SetInvalidationsForSessionsEnabled(bool enabled) {}

void FakeSyncEngine::GetNigoriNodeForDebugging(AllNodesCallback callback) {}

}  // namespace syncer
