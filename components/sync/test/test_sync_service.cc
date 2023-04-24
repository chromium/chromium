// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_sync_service.h"

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/model/type_entities_count.h"

namespace syncer {

namespace {

SyncCycleSnapshot MakeDefaultCycleSnapshot() {
  return SyncCycleSnapshot(
      /*birthday=*/"", /*bag_of_chips=*/"", ModelNeutralState(),
      ProgressMarkerMap(), /*is_silenced-*/ false,
      /*num_server_conflicts=*/7, /*notifications_enabled=*/false,
      /*sync_start_time=*/base::Time::Now(),
      /*poll_finish_time=*/base::Time::Now(),
      /*get_updates_origin=*/sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*poll_interval=*/base::Minutes(30),
      /*has_remaining_local_changes=*/false);
}

}  // namespace

TestSyncService::TestSyncService()
    : user_settings_(this), last_cycle_snapshot_(MakeDefaultCycleSnapshot()) {}

TestSyncService::~TestSyncService() = default;

void TestSyncService::SetDisableReasons(DisableReasonSet disable_reasons) {
  disable_reasons_ = disable_reasons;
}

void TestSyncService::SetTransportState(TransportState transport_state) {
  transport_state_ = transport_state;
}

void TestSyncService::SetLocalSyncEnabled(bool local_sync_enabled) {
  local_sync_enabled_ = local_sync_enabled;
}

void TestSyncService::SetAccountInfo(const CoreAccountInfo& account_info) {
  account_info_ = account_info;
}

void TestSyncService::SetSetupInProgress(bool in_progress) {
  setup_in_progress_ = in_progress;
}

void TestSyncService::SetHasSyncConsent(bool has_sync_consent) {
  has_sync_consent_ = has_sync_consent;
}

void TestSyncService::SetPersistentAuthError() {
  transport_state_ = TransportState::PAUSED;
}

void TestSyncService::ClearAuthError() {
  if (transport_state_ == TransportState::PAUSED) {
    transport_state_ = TransportState::ACTIVE;
  }
}

void TestSyncService::SetFirstSetupComplete(bool first_setup_complete) {
  if (first_setup_complete)
    user_settings_.SetFirstSetupComplete();
  else
    user_settings_.ClearFirstSetupComplete();
}

void TestSyncService::SetFailedDataTypes(const ModelTypeSet& types) {
  failed_data_types_ = types;
}

void TestSyncService::SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot) {
  last_cycle_snapshot_ = snapshot;
}

void TestSyncService::SetEmptyLastCycleSnapshot() {
  SetLastCycleSnapshot(SyncCycleSnapshot());
}

void TestSyncService::SetNonEmptyLastCycleSnapshot() {
  SetLastCycleSnapshot(MakeDefaultCycleSnapshot());
}

void TestSyncService::SetDetailedSyncStatus(bool engine_available,
                                            SyncStatus status) {
  detailed_sync_status_engine_available_ = engine_available;
  detailed_sync_status_ = status;
}

void TestSyncService::SetPassphraseRequired(bool required) {
  user_settings_.SetPassphraseRequired(required);
}

void TestSyncService::SetPassphraseRequiredForPreferredDataTypes(
    bool required) {
  user_settings_.SetPassphraseRequiredForPreferredDataTypes(required);
}

void TestSyncService::SetTrustedVaultKeyRequired(bool required) {
  user_settings_.SetTrustedVaultKeyRequired(required);
}

void TestSyncService::SetTrustedVaultKeyRequiredForPreferredDataTypes(
    bool required) {
  user_settings_.SetTrustedVaultKeyRequiredForPreferredDataTypes(required);
}

void TestSyncService::SetTrustedVaultRecoverabilityDegraded(bool degraded) {
  user_settings_.SetTrustedVaultRecoverabilityDegraded(degraded);
}

void TestSyncService::SetIsUsingExplicitPassphrase(bool enabled) {
  user_settings_.SetIsUsingExplicitPassphrase(enabled);
}

void TestSyncService::FireStateChanged() {
  for (SyncServiceObserver& observer : observers_)
    observer.OnStateChanged(this);
}

void TestSyncService::FireSyncCycleCompleted() {
  for (SyncServiceObserver& observer : observers_)
    observer.OnSyncCycleCompleted(this);
}

void TestSyncService::SetSyncFeatureRequested() {
  disable_reasons_.Remove(SyncService::DISABLE_REASON_USER_CHOICE);
}

TestSyncUserSettings* TestSyncService::GetUserSettings() {
  return &user_settings_;
}

const TestSyncUserSettings* TestSyncService::GetUserSettings() const {
  return &user_settings_;
}

SyncService::DisableReasonSet TestSyncService::GetDisableReasons() const {
  return disable_reasons_;
}

SyncService::TransportState TestSyncService::GetTransportState() const {
  return transport_state_;
}

SyncService::UserActionableError TestSyncService::GetUserActionableError()
    const {
  if (transport_state_ == TransportState::PAUSED) {
    return UserActionableError::kSignInNeedsUpdate;
  }
  if (user_settings_.IsPassphraseRequiredForPreferredDataTypes()) {
    return UserActionableError::kNeedsPassphrase;
  }
  return UserActionableError::kNone;
}

bool TestSyncService::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

CoreAccountInfo TestSyncService::GetAccountInfo() const {
  return account_info_;
}

bool TestSyncService::HasSyncConsent() const {
  return has_sync_consent_;
}

GoogleServiceAuthError TestSyncService::GetAuthError() const {
  return GoogleServiceAuthError();
}

base::Time TestSyncService::GetAuthErrorTime() const {
  return base::Time();
}

bool TestSyncService::RequiresClientUpgrade() const {
  return detailed_sync_status_.sync_protocol_error.action ==
         syncer::UPGRADE_CLIENT;
}

std::unique_ptr<SyncSetupInProgressHandle>
TestSyncService::GetSetupInProgressHandle() {
  return nullptr;
}

bool TestSyncService::IsSetupInProgress() const {
  return setup_in_progress_;
}

ModelTypeSet TestSyncService::GetPreferredDataTypes() const {
  return user_settings_.GetPreferredDataTypes();
}

ModelTypeSet TestSyncService::GetActiveDataTypes() const {
  if (transport_state_ != TransportState::ACTIVE) {
    return ModelTypeSet();
  }
  return Difference(GetPreferredDataTypes(), failed_data_types_);
}

ModelTypeSet TestSyncService::GetTypesWithPendingDownloadForInitialSync()
    const {
  if (transport_state_ != TransportState::CONFIGURING) {
    return ModelTypeSet();
  }
  return Difference(GetPreferredDataTypes(), failed_data_types_);
}

void TestSyncService::StopAndClear() {}

void TestSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {}

void TestSyncService::TriggerRefresh(const ModelTypeSet& types) {}

void TestSyncService::DataTypePreconditionChanged(ModelType type) {}

void TestSyncService::AddObserver(SyncServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void TestSyncService::RemoveObserver(SyncServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestSyncService::HasObserver(const SyncServiceObserver* observer) const {
  return observers_.HasObserver(observer);
}

SyncTokenStatus TestSyncService::GetSyncTokenStatusForDebugging() const {
  return SyncTokenStatus();
}

bool TestSyncService::QueryDetailedSyncStatusForDebugging(
    SyncStatus* result) const {
  *result = detailed_sync_status_;
  return detailed_sync_status_engine_available_;
}

base::Time TestSyncService::GetLastSyncedTimeForDebugging() const {
  return base::Time();
}

SyncCycleSnapshot TestSyncService::GetLastCycleSnapshotForDebugging() const {
  return last_cycle_snapshot_;
}

base::Value::List TestSyncService::GetTypeStatusMapForDebugging() const {
  return base::Value::List();
}

void TestSyncService::GetEntityCountsForDebugging(
    base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
    const {
  std::move(callback).Run({});
}

const GURL& TestSyncService::GetSyncServiceUrlForDebugging() const {
  return sync_service_url_;
}

std::string TestSyncService::GetUnrecoverableErrorMessageForDebugging() const {
  return std::string();
}

base::Location TestSyncService::GetUnrecoverableErrorLocationForDebugging()
    const {
  return base::Location();
}

void TestSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void TestSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void TestSyncService::GetAllNodesForDebugging(
    base::OnceCallback<void(base::Value::List)> callback) {}

void TestSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {}

void TestSyncService::AddTrustedVaultDecryptionKeysFromWeb(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {}

void TestSyncService::AddTrustedVaultRecoveryMethodFromWeb(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {}

void TestSyncService::Shutdown() {
  for (SyncServiceObserver& observer : observers_)
    observer.OnSyncShutdown(this);
}

}  // namespace syncer
