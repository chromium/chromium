// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_sync_service.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/sync_token_status.h"

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

CoreAccountInfo GetDefaultAccountInfo() {
  CoreAccountInfo account;
  account.email = "foo@bar.com";
  account.gaia = "foo-gaia-id";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  return account;
}

}  // namespace

TestSyncService::TestSyncService()
    : user_settings_(this),
      account_info_(GetDefaultAccountInfo()),
      last_cycle_snapshot_(MakeDefaultCycleSnapshot()) {}

TestSyncService::~TestSyncService() = default;

void TestSyncService::SetDisableReasons(DisableReasonSet disable_reasons) {
  disable_reasons_ = disable_reasons;
  if (!disable_reasons_.Empty()) {
    transport_state_ = TransportState::DISABLED;
  } else if (transport_state_ == TransportState::DISABLED) {
    transport_state_ = TransportState::ACTIVE;
  }
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

void TestSyncService::SetInitialSyncFeatureSetupComplete(
    bool initial_sync_feature_setup_complete) {
  if (initial_sync_feature_setup_complete) {
    user_settings_.SetInitialSyncFeatureSetupComplete();
  } else {
    user_settings_.ClearInitialSyncFeatureSetupComplete();
  }
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

void TestSyncService::SetDownloadStatusFor(
    const ModelTypeSet& types,
    ModelTypeDownloadStatus download_status) {
  for (const auto type : types) {
    download_statuses_[type] = download_status;
  }
}

void TestSyncService::FireStateChanged() {
  for (SyncServiceObserver& observer : observers_) {
    observer.OnStateChanged(this);
  }
}

void TestSyncService::FirePaymentsIntegrationEnabledChanged() {
  for (SyncServiceObserver& observer : observers_) {
    observer.OnSyncPaymentsIntegrationEnabledChanged(this);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_settings_.IsSyncFeatureDisabledViaDashboard()) {
    return ModelTypeSet();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return Difference(GetPreferredDataTypes(), failed_data_types_);
}

ModelTypeSet TestSyncService::GetTypesWithPendingDownloadForInitialSync()
    const {
  DCHECK_NE(transport_state_, TransportState::INITIALIZING)
      << "Realistic behavior not implemented for INITIALIZING";
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

SyncService::ModelTypeDownloadStatus TestSyncService::GetDownloadStatusFor(
    ModelType type) const {
  if (base::Contains(download_statuses_, type)) {
    return download_statuses_.at(type);
  }
  return ModelTypeDownloadStatus::kUpToDate;
}

void TestSyncService::RecordReasonIfWaitingForUpdates(
    ModelType type,
    const std::string& histogram_name) const {}

void TestSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {}

bool TestSyncService::IsSyncFeatureConsideredRequested() const {
  return HasSyncConsent();
}

void TestSyncService::Shutdown() {
  for (SyncServiceObserver& observer : observers_)
    observer.OnSyncShutdown(this);
}

void TestSyncService::SetTypesWithUnsyncedData(const ModelTypeSet& types) {
  unsynced_types_ = types;
}

void TestSyncService::GetTypesWithUnsyncedData(
    base::OnceCallback<void(ModelTypeSet)> cb) const {
  std::move(cb).Run(unsynced_types_);
}

void TestSyncService::SetLocalDataDescriptions(
    const std::map<ModelType, LocalDataDescription>& local_data_descriptions) {
  local_data_descriptions_ = local_data_descriptions;
}

void TestSyncService::GetLocalDataDescriptions(
    ModelTypeSet types,
    base::OnceCallback<void(std::map<ModelType, LocalDataDescription>)>
        callback) {
  std::map<ModelType, LocalDataDescription> result;
  for (ModelType type : types) {
    if (auto it = local_data_descriptions_.find(type);
        it != local_data_descriptions_.end()) {
      result.insert(*it);
    }
  }
  std::move(callback).Run(std::move(result));
}

void TestSyncService::TriggerLocalDataMigration(ModelTypeSet types) {}

}  // namespace syncer
