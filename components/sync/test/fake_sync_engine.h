// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
class FakeSyncEngine : public SyncEngine,
                       public base::SupportsWeakPtr<FakeSyncEngine> {
 public:
  static constexpr char kTestBirthday[] = "1";

  FakeSyncEngine(bool allow_init_completion,
                 bool is_first_time_sync_configure,
                 const base::RepeatingClosure& sync_transport_data_cleared_cb);
  ~FakeSyncEngine() override;

  CoreAccountId authenticated_account_id() const {
    return authenticated_account_id_;
  }

  bool started_handling_invalidations() {
    return started_handling_invalidations_;
  }

  // Manual completion of Initialize(), required if auto-completion was disabled
  // in the constructor.
  void TriggerInitializationCompletion(bool success);

  // Immediately calls params.host->OnEngineInitialized.
  void Initialize(InitParams params) override;

  bool IsInitialized() const override;

  void TriggerRefresh(const ModelTypeSet& types) override;

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

  void ConnectDataType(ModelType type,
                       std::unique_ptr<DataTypeActivationResponse>) override;
  void DisconnectDataType(ModelType type) override;

  void SetProxyTabsDatatypeEnabled(bool enabled) override;

  const SyncStatus& GetDetailedStatus() const override;

  void HasUnsyncedItemsForTest(
      base::OnceCallback<void(bool)> cb) const override;
  void GetThrottledDataTypesForTest(
      base::OnceCallback<void(ModelTypeSet)> cb) const override;

  void RequestBufferedProtocolEventsAndEnableForwarding() override;
  void DisableProtocolEventForwarding() override;

  void OnCookieJarChanged(bool account_mismatch,
                          base::OnceClosure callback) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void GetNigoriNodeForDebugging(AllNodesCallback callback) override;

 private:
  const bool allow_init_completion_;
  const bool is_first_time_sync_configure_;
  const base::RepeatingClosure sync_transport_data_cleared_cb_;
  raw_ptr<SyncEngineHost> host_ = nullptr;
  bool initialized_ = false;
  const SyncStatus default_sync_status_;
  CoreAccountId authenticated_account_id_;
  bool started_handling_invalidations_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_H_
