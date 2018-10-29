// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/fake_sync_engine.h"

#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/sync_engine_host.h"

namespace syncer {
namespace {

const char kTestCacheGuid[] = "test-guid";
const char kTestSessionName[] = "test-session-name";

}  // namespace

FakeSyncEngine::FakeSyncEngine() : fail_initial_download_(false) {}
FakeSyncEngine::~FakeSyncEngine() {}

void FakeSyncEngine::Initialize(InitParams params) {
  params.host->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                                   WeakHandle<DataTypeDebugInfoListener>(),
                                   kTestCacheGuid, kTestSessionName,
                                   !fail_initial_download_);
}

void FakeSyncEngine::TriggerRefresh(const ModelTypeSet& types) {}

void FakeSyncEngine::UpdateCredentials(const SyncCredentials& credentials) {}

void FakeSyncEngine::InvalidateCredentials() {}

void FakeSyncEngine::StartConfiguration() {}

void FakeSyncEngine::StartSyncingWithServer() {}

void FakeSyncEngine::SetEncryptionPassphrase(const std::string& passphrase) {}

void FakeSyncEngine::SetDecryptionPassphrase(const std::string& passphrase) {}

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

SyncEngine::Status FakeSyncEngine::GetDetailedStatus() {
  return SyncEngine::Status();
}

void FakeSyncEngine::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {}

bool FakeSyncEngine::IsCryptographerReady(const BaseTransaction* trans) const {
  return false;
}

void FakeSyncEngine::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const {}

void FakeSyncEngine::FlushDirectory() const {}

void FakeSyncEngine::RequestBufferedProtocolEventsAndEnableForwarding() {}

void FakeSyncEngine::DisableProtocolEventForwarding() {}

void FakeSyncEngine::EnableDirectoryTypeDebugInfoForwarding() {}

void FakeSyncEngine::DisableDirectoryTypeDebugInfoForwarding() {}

void FakeSyncEngine::set_fail_initial_download(bool should_fail) {
  fail_initial_download_ = should_fail;
}

void FakeSyncEngine::ClearServerData(const base::Closure& callback) {
  callback.Run();
}

void FakeSyncEngine::OnCookieJarChanged(bool account_mismatch,
                                        bool empty_jar,
                                        const base::Closure& callback) {
  if (!callback.is_null()) {
    callback.Run();
  }
}

}  // namespace syncer
