// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_engine.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/engine/sync_engine_host.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace syncer {

constexpr char FakeSyncEngine::kTestBirthday[];

FakeSyncEngine::FakeSyncEngine(
    bool allow_init_completion,
    bool is_first_time_sync_configure,
    const base::RepeatingClosure& sync_transport_data_cleared_cb)
    : allow_init_completion_(allow_init_completion),
      is_first_time_sync_configure_(is_first_time_sync_configure),
      sync_transport_data_cleared_cb_(sync_transport_data_cleared_cb) {}

FakeSyncEngine::~FakeSyncEngine() = default;

void FakeSyncEngine::TriggerInitializationCompletion(bool success) {
  DCHECK(host_) << "Initialize() not called.";
  DCHECK(!initialized_);

  initialized_ = success;

  host_->OnEngineInitialized(success, is_first_time_sync_configure_);
}

void FakeSyncEngine::SetPollIntervalElapsed(bool elapsed) {
  is_next_poll_time_in_the_past_ = elapsed;
}

void FakeSyncEngine::SetDetailedStatus(const SyncStatus& status) {
  sync_status_ = status;
}

void FakeSyncEngine::Initialize(InitParams params) {
  DCHECK(params.host);

  authenticated_account_id_ = params.authenticated_account_info.account_id;
  host_ = params.host;

  if (allow_init_completion_) {
    TriggerInitializationCompletion(/*success=*/true);
  }
}

bool FakeSyncEngine::IsInitialized() const {
  return initialized_;
}

void FakeSyncEngine::TriggerRefresh(const DataTypeSet& types) {}

void FakeSyncEngine::UpdateCredentials(const SyncCredentials& credentials) {}

void FakeSyncEngine::InvalidateCredentials() {}

std::string FakeSyncEngine::GetCacheGuid() const {
  return "fake_engine_cache_guid";
}

std::string FakeSyncEngine::GetBirthday() const {
  // The birthday becomes known the very first time sync completes.
  return (initialized_ || !is_first_time_sync_configure_) ? kTestBirthday
                                                          : std::string();
}

base::Time FakeSyncEngine::GetLastSyncedTimeForDebugging() const {
  return base::Time();
}

void FakeSyncEngine::StartConfiguration() {}

void FakeSyncEngine::StartSyncingWithServer() {}

void FakeSyncEngine::StartHandlingInvalidations() {
  started_handling_invalidations_ = true;
}

void FakeSyncEngine::SetEncryptionPassphrase(
    const std::string& passphrase,
    const KeyDerivationParams& key_derivation_params) {}

void FakeSyncEngine::SetExplicitPassphraseDecryptionKey(
    std::unique_ptr<Nigori> key) {}

void FakeSyncEngine::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& keys,
    base::OnceClosure done_cb) {
  std::move(done_cb).Run();
}

void FakeSyncEngine::StopSyncingForShutdown() {}

void FakeSyncEngine::Shutdown(ShutdownReason reason) {
  if (reason == ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA) {
    sync_transport_data_cleared_cb_.Run();
  }
}

void FakeSyncEngine::ConfigureDataTypes(ConfigureParams params) {
  last_configure_reason_ = params.reason;
  std::move(params.ready_task)
      .Run(/*succeeded_configuration_types=*/params.to_download,
           /*failed_configuration_types=*/DataTypeSet());
}

void FakeSyncEngine::ConnectDataType(
    DataType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {}

void FakeSyncEngine::DisconnectDataType(DataType type) {}

const SyncStatus& FakeSyncEngine::GetDetailedStatus() const {
  return sync_status_;
}

void FakeSyncEngine::HasUnsyncedItemsForTest(
    base::OnceCallback<void(bool)> cb) const {}

void FakeSyncEngine::GetThrottledDataTypesForTest(
    base::OnceCallback<void(DataTypeSet)> cb) const {}

void FakeSyncEngine::RequestBufferedProtocolEventsAndEnableForwarding() {}

void FakeSyncEngine::DisableProtocolEventForwarding() {}

void FakeSyncEngine::OnCookieJarChanged(bool account_mismatch,
                                        base::OnceClosure callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

bool FakeSyncEngine::IsNextPollTimeInThePast() const {
  return is_next_poll_time_in_the_past_;
}

void FakeSyncEngine::ClearNigoriDataForMigration() {}

void FakeSyncEngine::GetNigoriNodeForDebugging(AllNodesCallback callback) {}

void FakeSyncEngine::RecordNigoriMemoryUsageAndCountsHistograms() {}

}  // namespace syncer
