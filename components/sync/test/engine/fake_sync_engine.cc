// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/fake_sync_engine.h"

#include <utility>

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

constexpr char FakeSyncEngine::kTestBirthday[];

FakeSyncEngine::FakeSyncEngine() {}
FakeSyncEngine::~FakeSyncEngine() {}

void FakeSyncEngine::Initialize(InitParams params) {
  bool success = !fail_initial_download_;
  initialized_ = success;
  params.host->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                                   WeakHandle<DataTypeDebugInfoListener>(),
                                   kTestBirthday,
                                   /*bag_of_chips=*/"", success);
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
    const std::vector<std::vector<uint8_t>>& keys,
    base::OnceClosure done_cb) {
  std::move(done_cb).Run();
}

void FakeSyncEngine::StopSyncingForShutdown() {}

void FakeSyncEngine::Shutdown(ShutdownReason reason) {}

void FakeSyncEngine::ConfigureDataTypes(ConfigureParams params) {}

void FakeSyncEngine::ActivateDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {}

void FakeSyncEngine::DeactivateDataType(ModelType type) {}

void FakeSyncEngine::ActivateProxyDataType(ModelType type) {}

void FakeSyncEngine::DeactivateProxyDataType(ModelType type) {}

const SyncStatus& FakeSyncEngine::GetDetailedStatus() const {
  return default_sync_status_;
}

void FakeSyncEngine::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {}

void FakeSyncEngine::RequestBufferedProtocolEventsAndEnableForwarding() {}

void FakeSyncEngine::DisableProtocolEventForwarding() {}

void FakeSyncEngine::set_fail_initial_download(bool should_fail) {
  fail_initial_download_ = should_fail;
}

void FakeSyncEngine::OnCookieJarChanged(bool account_mismatch,
                                        base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void FakeSyncEngine::SetInvalidationsForSessionsEnabled(bool enabled) {}

void FakeSyncEngine::GetNigoriNodeForDebugging(AllNodesCallback callback) {}

}  // namespace syncer
