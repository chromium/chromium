// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_service.h"

#include <utility>

#include "base/values.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/service/sync_token_status.h"

namespace syncer {

// Dummy methods

FakeSyncService::FakeSyncService() = default;

FakeSyncService::~FakeSyncService() = default;

void FakeSyncService::SetSyncFeatureRequested() {}

syncer::SyncUserSettings* FakeSyncService::GetUserSettings() {
  return nullptr;
}

const syncer::SyncUserSettings* FakeSyncService::GetUserSettings() const {
  return nullptr;
}

syncer::SyncService::DisableReasonSet FakeSyncService::GetDisableReasons()
    const {
  return {DISABLE_REASON_NOT_SIGNED_IN};
}

syncer::SyncService::TransportState FakeSyncService::GetTransportState() const {
  return TransportState::DISABLED;
}

SyncService::UserActionableError FakeSyncService::GetUserActionableError()
    const {
  return UserActionableError::kNone;
}

CoreAccountInfo FakeSyncService::GetAccountInfo() const {
  return CoreAccountInfo();
}

bool FakeSyncService::HasSyncConsent() const {
  return true;
}

bool FakeSyncService::IsLocalSyncEnabled() const {
  return false;
}

void FakeSyncService::TriggerRefresh(const ModelTypeSet& types) {}

ModelTypeSet FakeSyncService::GetActiveDataTypes() const {
  return ModelTypeSet();
}

ModelTypeSet FakeSyncService::GetTypesWithPendingDownloadForInitialSync()
    const {
  return ModelTypeSet();
}

void FakeSyncService::AddObserver(SyncServiceObserver* observer) {}

void FakeSyncService::RemoveObserver(SyncServiceObserver* observer) {}

bool FakeSyncService::HasObserver(const SyncServiceObserver* observer) const {
  return false;
}

void FakeSyncService::StopAndClear() {}

void FakeSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {}

ModelTypeSet FakeSyncService::GetPreferredDataTypes() const {
  return ModelTypeSet();
}

std::unique_ptr<SyncSetupInProgressHandle>
FakeSyncService::GetSetupInProgressHandle() {
  return nullptr;
}

bool FakeSyncService::IsSetupInProgress() const {
  return false;
}

GoogleServiceAuthError FakeSyncService::GetAuthError() const {
  return GoogleServiceAuthError();
}

base::Time FakeSyncService::GetAuthErrorTime() const {
  return base::Time();
}

bool FakeSyncService::RequiresClientUpgrade() const {
  return false;
}

bool FakeSyncService::IsSyncFeatureDisabledViaDashboard() const {
  return false;
}

void FakeSyncService::DataTypePreconditionChanged(ModelType type) {}

syncer::SyncTokenStatus FakeSyncService::GetSyncTokenStatusForDebugging()
    const {
  return syncer::SyncTokenStatus();
}

bool FakeSyncService::QueryDetailedSyncStatusForDebugging(
    SyncStatus* result) const {
  return false;
}

base::Time FakeSyncService::GetLastSyncedTimeForDebugging() const {
  return base::Time();
}

SyncCycleSnapshot FakeSyncService::GetLastCycleSnapshotForDebugging() const {
  return SyncCycleSnapshot();
}

base::Value::List FakeSyncService::GetTypeStatusMapForDebugging() const {
  return base::Value::List();
}

void FakeSyncService::GetEntityCountsForDebugging(
    base::OnceCallback<void(const std::vector<TypeEntitiesCount>&)> callback)
    const {
  return std::move(callback).Run({});
}

const GURL& FakeSyncService::GetSyncServiceUrlForDebugging() const {
  return sync_service_url_;
}

std::string FakeSyncService::GetUnrecoverableErrorMessageForDebugging() const {
  return std::string();
}

base::Location FakeSyncService::GetUnrecoverableErrorLocationForDebugging()
    const {
  return base::Location();
}

void FakeSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void FakeSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void FakeSyncService::GetAllNodesForDebugging(
    base::OnceCallback<void(base::Value::List)> callback) {}

SyncService::ModelTypeDownloadStatus FakeSyncService::GetDownloadStatusFor(
    ModelType type) const {
  return ModelTypeDownloadStatus::kUpToDate;
}

void FakeSyncService::SetInvalidationsForSessionsEnabled(bool enabled) {}

void FakeSyncService::AddTrustedVaultDecryptionKeysFromWeb(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {}

void FakeSyncService::AddTrustedVaultRecoveryMethodFromWeb(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {}

bool FakeSyncService::IsSyncFeatureConsideredRequested() const {
  return HasSyncConsent();
}

void FakeSyncService::Shutdown() {}

}  // namespace syncer
