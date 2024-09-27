// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_sync_service.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_token_status.h"

namespace syncer {

namespace {

SyncCycleSnapshot MakeDefaultCycleSnapshot() {
  return SyncCycleSnapshot(
      /*birthday=*/"", /*bag_of_chips=*/"", ModelNeutralState(),
      ProgressMarkerMap(), /*is_silenced=*/false,
      /*num_server_conflicts=*/7, /*notifications_enabled=*/false,
      /*sync_start_time=*/base::Time::Now(),
      /*poll_finish_time=*/base::Time::Now(),
      /*get_updates_origin=*/sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*poll_interval=*/base::Minutes(30),
      /*has_remaining_local_changes=*/false);
}

CoreAccountInfo GetDefaultAccountInfo() {
  CoreAccountInfo account;
  account.email = "foo@bar.com";
  account.gaia = "foo-gaia-id";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  return account;
}

}  // namespace

TestSyncService::TestSyncService()
    : user_settings_(this), last_cycle_snapshot_(MakeDefaultCycleSnapshot()) {
  SetSignedIn(signin::ConsentLevel::kSync);
}

TestSyncService::~TestSyncService() = default;

void TestSyncService::SetSignedIn(signin::ConsentLevel consent_level) {
  SetSignedIn(consent_level, GetDefaultAccountInfo());
}

void TestSyncService::SetSignedIn(signin::ConsentLevel consent_level,
                                  const CoreAccountInfo& account_info) {
  disable_reasons_.Remove(DISABLE_REASON_NOT_SIGNED_IN);
  account_info_ = account_info;
  if (consent_level == signin::ConsentLevel::kSync) {
    has_sync_consent_ = true;
    user_settings_.SetInitialSyncFeatureSetupComplete();
  } else {
    has_sync_consent_ = false;
    user_settings_.ClearInitialSyncFeatureSetupComplete();
  }
}

void TestSyncService::SetSignedOut() {
  has_sync_consent_ = false;
  user_settings_.ClearInitialSyncFeatureSetupComplete();
  account_info_ = CoreAccountInfo();
  has_persistent_auth_error_ = false;
  disable_reasons_.Put(DISABLE_REASON_NOT_SIGNED_IN);
  CHECK_EQ(GetTransportState(), TransportState::DISABLED);
}

void TestSyncService::MimicDashboardClear() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Clearing sync from the dashboard results in
  // IsSyncFeatureDisabledViaDashboard() returning true.
  user_settings_.SetSyncFeatureDisabledViaDashboard(true);
#else
  SetSignedIn(signin::ConsentLevel::kSignin);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void TestSyncService::SetAllowedByEnterprisePolicy(bool allowed) {
  disable_reasons_.PutOrRemove(DISABLE_REASON_ENTERPRISE_POLICY, !allowed);
}

void TestSyncService::SetHasUnrecoverableError(bool has_error) {
  disable_reasons_.PutOrRemove(DISABLE_REASON_UNRECOVERABLE_ERROR, has_error);
}

void TestSyncService::SetMaxTransportState(TransportState max_transport_state) {
  CHECK_NE(max_transport_state, TransportState::DISABLED)
      << "DISABLED should be set via one of SetSignedOut(), "
         "SetAllowedByEnterprisePolicy(false) or "
         "SetHasUnrecoverableError(true)";
  CHECK_NE(max_transport_state, TransportState::PAUSED)
      << "PAUSED should be set via SetPersistentAuthError()";
  max_transport_state_ = max_transport_state;
}

void TestSyncService::SetLocalSyncEnabled(bool local_sync_enabled) {
  local_sync_enabled_ = local_sync_enabled;
}

void TestSyncService::SetPersistentAuthError() {
  CHECK(!account_info_.IsEmpty()) << "Attempting to set persistent auth error "
                                     "when there is no signed-in account";
  has_persistent_auth_error_ = true;
}

void TestSyncService::ClearAuthError() {
  has_persistent_auth_error_ = false;
}

void TestSyncService::SetInitialSyncFeatureSetupComplete(
    bool initial_sync_feature_setup_complete) {
  if (initial_sync_feature_setup_complete) {
    user_settings_.SetInitialSyncFeatureSetupComplete();
  } else {
    user_settings_.ClearInitialSyncFeatureSetupComplete();
  }
}

void TestSyncService::SetFailedDataTypes(const DataTypeSet& types) {
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

void TestSyncService::SetPassphraseRequired() {
  user_settings_.SetPassphraseRequired();
}

void TestSyncService::SetTrustedVaultKeyRequired(bool required) {
  user_settings_.SetTrustedVaultKeyRequired(required);
}

void TestSyncService::SetTrustedVaultRecoverabilityDegraded(bool degraded) {
  user_settings_.SetTrustedVaultRecoverabilityDegraded(degraded);
}

void TestSyncService::SetIsUsingExplicitPassphrase(bool enabled) {
  user_settings_.SetIsUsingExplicitPassphrase(enabled);
}

void TestSyncService::SetDownloadStatusFor(
    const DataTypeSet& types,
    DataTypeDownloadStatus download_status) {
  for (const auto type : types) {
    download_statuses_[type] = download_status;
  }
}

void TestSyncService::SetSetupInProgress() {
  outstanding_setup_in_progress_handles_++;
}

void TestSyncService::FireStateChanged() {
  for (SyncServiceObserver& observer : observers_) {
    observer.OnStateChanged(this);
  }
}

void TestSyncService::FireSyncCycleCompleted() {
  for (SyncServiceObserver& observer : observers_) {
    observer.OnSyncCycleCompleted(this);
  }
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> TestSyncService::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>();
}
#endif  // BUILDFLAG(IS_ANDROID)

void TestSyncService::SetSyncFeatureRequested() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_settings_.SetSyncFeatureDisabledViaDashboard(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
  if (!disable_reasons_.empty()) {
    return TransportState::DISABLED;
  }

  if (has_persistent_auth_error_) {
    CHECK(!account_info_.IsEmpty())
        << "Detected persistent auth error when there is no signed-in account";
    return TransportState::PAUSED;
  }

  return max_transport_state_;
}

SyncService::UserActionableError TestSyncService::GetUserActionableError()
    const {
  if (GetTransportState() == TransportState::PAUSED) {
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
  outstanding_setup_in_progress_handles_++;
  return std::make_unique<SyncSetupInProgressHandle>(
      base::BindRepeating(&TestSyncService::OnSetupInProgressHandleDestroyed,
                          weak_factory_.GetWeakPtr()));
}

bool TestSyncService::IsSetupInProgress() const {
  return outstanding_setup_in_progress_handles_ > 0;
}

DataTypeSet TestSyncService::GetPreferredDataTypes() const {
  return user_settings_.GetPreferredDataTypes();
}

DataTypeSet TestSyncService::GetActiveDataTypes() const {
  if (GetTransportState() != TransportState::ACTIVE) {
    return DataTypeSet();
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_settings_.IsSyncFeatureDisabledViaDashboard()) {
    return DataTypeSet();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return Difference(GetPreferredDataTypes(), failed_data_types_);
}

DataTypeSet TestSyncService::GetTypesWithPendingDownloadForInitialSync() const {
  DCHECK_NE(GetTransportState(), TransportState::INITIALIZING)
      << "Realistic behavior not implemented for INITIALIZING";
  if (GetTransportState() != TransportState::CONFIGURING) {
    return DataTypeSet();
  }
  return Difference(GetPreferredDataTypes(), failed_data_types_);
}

void TestSyncService::OnDataTypeRequestsSyncStartup(DataType type) {}

void TestSyncService::TriggerRefresh(const DataTypeSet& types) {
  if (trigger_refresh_cb_) {
    trigger_refresh_cb_.Run(types);
  }
}

void TestSyncService::DataTypePreconditionChanged(DataType type) {}

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

TypeStatusMapForDebugging TestSyncService::GetTypeStatusMapForDebugging()
    const {
  return TypeStatusMapForDebugging();
}

void TestSyncService::GetEntityCountsForDebugging(
    base::RepeatingCallback<void(const TypeEntitiesCount&)> callback) const {}

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

SyncService::DataTypeDownloadStatus TestSyncService::GetDownloadStatusFor(
    DataType type) const {
  if (download_statuses_.contains(type)) {
    return download_statuses_.at(type);
  }
  return DataTypeDownloadStatus::kUpToDate;
}

void TestSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {}

void TestSyncService::SendExplicitPassphraseToPlatformClient() {
  if (send_passphrase_to_platform_client_cb_) {
    send_passphrase_to_platform_client_cb_.Run();
  }
}

void TestSyncService::Shutdown() {
  for (SyncServiceObserver& observer : observers_) {
    observer.OnSyncShutdown(this);
  }
}

void TestSyncService::SetTypesWithUnsyncedData(const DataTypeSet& types) {
  unsynced_types_ = types;
}

void TestSyncService::GetTypesWithUnsyncedData(
    DataTypeSet requested_types,
    base::OnceCallback<void(DataTypeSet)> cb) const {
  std::move(cb).Run(base::Intersection(requested_types, unsynced_types_));
}

void TestSyncService::SetLocalDataDescriptions(
    const std::map<DataType, LocalDataDescription>& local_data_descriptions) {
  local_data_descriptions_ = local_data_descriptions;
}

void TestSyncService::SetPassphrasePlatformClientCallback(
    const base::RepeatingClosure& send_passphrase_to_platform_client_cb) {
  send_passphrase_to_platform_client_cb_ =
      send_passphrase_to_platform_client_cb;
}

void TestSyncService::GetLocalDataDescriptions(
    DataTypeSet types,
    base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
        callback) {
  std::map<DataType, LocalDataDescription> result;
  for (DataType type : types) {
    if (auto it = local_data_descriptions_.find(type);
        it != local_data_descriptions_.end()) {
      result.insert(*it);
    }
  }
  std::move(callback).Run(std::move(result));
}

void TestSyncService::TriggerLocalDataMigration(DataTypeSet types) {}

void TestSyncService::SetTriggerRefreshCallback(
    const base::RepeatingCallback<void(syncer::DataTypeSet)>&
        trigger_refresh_cb) {
  trigger_refresh_cb_ = trigger_refresh_cb;
}

void TestSyncService::OnSetupInProgressHandleDestroyed() {
  outstanding_setup_in_progress_handles_--;
}

}  // namespace syncer
