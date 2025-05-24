// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/engine/sync_status.h"
#include "google_apis/gaia/core_account_id.h"

namespace syncer {

// A fake of the SyncEngine.
//
// This class implements the bare minimum required for the SyncServiceImpl to
// get through initialization. It often returns null pointers or nonsense
// values; it is not intended to be used in tests that depend on SyncEngine
// behavior.
class FakeSyncEngine final : public SyncEngine {
 public:
  static constexpr char kTestBirthday[] = "1";

  FakeSyncEngine(bool allow_init_completion,
                 bool is_first_time_sync_configure,
                 const base::RepeatingClosure& sync_transport_data_cleared_cb);
  ~FakeSyncEngine() override;

  CoreAccountId authenticated_account_id() const {
    return authenticated_account_id_;
  }

  ConfigureReason last_configure_reason() const {
    return last_configure_reason_;
  }

  bool started_handling_invalidations() {
    return started_handling_invalidations_;
  }

  void SetPollIntervalElapsed(bool elapsed);

  // Manual completion of Initialize(), required if auto-completion was disabled
  // in the constructor.
  void TriggerInitializationCompletion(bool success);

  void SetDetailedStatus(const SyncStatus& status);

  // Immediately calls params.host->OnEngineInitialized.
  void Initialize(InitParams params) override;

  bool IsInitialized() const override;

  void TriggerRefresh(const DataTypeSet& types) override;

  void UpdateCredentials(const SyncCredentials& credentials) override;

  void InvalidateCredentials() override;

  std::string GetCacheGuid() const override;

  std::string GetBirthday() const override;

  base::Time GetLastSyncedTimeForDebugging() const override;

  void StartConfiguration() override;

  void StartHandlingInvalidations() override;

  void StartSyncingWithServer() override;

  void SetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params) override;

  void SetExplicitPassphraseDecryptionKey(std::unique_ptr<Nigori> key) override;

  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys,
      base::OnceClosure done_cb) override;

  void StopSyncingForShutdown() override;

  void Shutdown(ShutdownReason reason) override;

  void ConfigureDataTypes(ConfigureParams params) override;

  void ConnectDataType(DataType type,
                       std::unique_ptr<DataTypeActivationResponse>) override;
  void DisconnectDataType(DataType type) override;

  const SyncStatus& GetDetailedStatus() const override;

  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;
  void GetThrottledDataTypesForTest(
      base::OnceCallback<void(DataTypeSet)> cb) const override;

  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;

  void OnCookieJarChanged(bool account_mismatch,
                          base::OnceClosure callback) override;
  bool IsNextPollTimeInThePast() const override;
  void ClearNigoriDataForMigration() override;
  void GetNigoriNodeForDebugging(AllNodesCallback callback) override;
  void RecordNigoriMemoryUsageAndCountsHistograms() override;

  base::WeakPtr<FakeSyncEngine> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const bool allow_init_completion_;
  const bool is_first_time_sync_configure_;
  const base::RepeatingClosure sync_transport_data_cleared_cb_;
  // AcrossTasksDanglingUntriaged because it is assigned a
  // AcrossTasksDanglingUntriaged pointer.
  raw_ptr<SyncEngineHost, AcrossTasksDanglingUntriaged> host_ = nullptr;
  bool initialized_ = false;
  SyncStatus sync_status_;
  CoreAccountId authenticated_account_id_;
  bool started_handling_invalidations_ = false;
  bool is_next_poll_time_in_the_past_ = false;
  ConfigureReason last_configure_reason_ = CONFIGURE_REASON_UNKNOWN;
  base::WeakPtrFactory<FakeSyncEngine> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_H_
