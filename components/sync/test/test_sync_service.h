// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TEST_TEST_SYNC_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace syncer {

// A simple test implementation of SyncService that allows direct control over
// the returned state. By default, everything returns "enabled"/"active".
class TestSyncService : public SyncService {
 public:
  TestSyncService();

  TestSyncService(const TestSyncService&) = delete;
  TestSyncService& operator=(const TestSyncService&) = delete;

  ~TestSyncService() override;

  void SetDisableReasons(DisableReasonSet disable_reasons);
  void SetTransportState(TransportState transport_state);
  void SetLocalSyncEnabled(bool local_sync_enabled);
  void SetAccountInfo(const CoreAccountInfo& account_info);
  void SetHasSyncConsent(bool has_consent);
  void SetSetupInProgress(bool in_progress);

  // Setters to mimic common auth error scenarios. Note that these functions
  // may change the transport state, as returned by GetTransportState().
  // TODO(crbug.com/1156584): Unify the two below once all persistent auth
  // errors are treated equally.
  void SetPersistentAuthErrorOtherThanWebSignout();
  void SetPersistentAuthErrorWithWebSignout();
  void ClearAuthError();

  void SetFirstSetupComplete(bool first_setup_complete);
  void SetFailedDataTypes(const ModelTypeSet& types);

  void SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot);
  // Convenience versions of the above, for when the caller doesn't care about
  // the particular values in the snapshot, just whether there is one.
  void SetEmptyLastCycleSnapshot();
  void SetNonEmptyLastCycleSnapshot();
  void SetDetailedSyncStatus(bool engine_available, SyncStatus status);
  void SetPassphraseRequired(bool required);
  void SetPassphraseRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultKeyRequired(bool required);
  void SetTrustedVaultKeyRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultRecoverabilityDegraded(bool degraded);
  void SetIsUsingExplicitPassphrase(bool enabled);

  void FireStateChanged();
  void FireSyncCycleCompleted();

  // SyncService implementation.
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  DisableReasonSet GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAccountInfo() const override;
  bool HasSyncConsent() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;

  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;

  ModelTypeSet GetPreferredDataTypes() const override;
  ModelTypeSet GetActiveDataTypes() const override;

  void StopAndClear() override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void DataTypePreconditionChanged(syncer::ModelType type) override;

  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;

  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  base::Value::List GetTypeStatusMapForDebugging() const override;
  void GetEntityCountsForDebugging(
      base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
      const override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(base::Value::List)> callback) override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void AddTrustedVaultDecryptionKeysFromWeb(
      const std::string& gaia_id,
      const std::vector<std::vector<uint8_t>>& keys,
      int last_key_version) override;
  void AddTrustedVaultRecoveryMethodFromWeb(
      const std::string& gaia_id,
      const std::vector<uint8_t>& public_key,
      int method_type_hint,
      base::OnceClosure callback) override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  TestSyncUserSettings user_settings_;

  DisableReasonSet disable_reasons_;
  TransportState transport_state_ = TransportState::ACTIVE;
  bool local_sync_enabled_ = false;
  CoreAccountInfo account_info_;
  bool has_sync_consent_ = true;
  bool setup_in_progress_ = false;
  GoogleServiceAuthError auth_error_;

  ModelTypeSet failed_data_types_;

  bool detailed_sync_status_engine_available_ = false;
  SyncStatus detailed_sync_status_;

  SyncCycleSnapshot last_cycle_snapshot_;

  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;

  GURL sync_service_url_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_SYNC_SERVICE_H_
