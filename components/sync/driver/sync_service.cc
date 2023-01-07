// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service.h"

#include <utility>

#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

namespace syncer {

SyncSetupInProgressHandle::SyncSetupInProgressHandle(
    base::OnceClosure on_destroy)
    : on_destroy_(std::move(on_destroy)) {}

SyncSetupInProgressHandle::~SyncSetupInProgressHandle() {
  std::move(on_destroy_).Run();
}

bool SyncService::HasCompletedSyncCycle() const {
  // Stats on the last Sync cycle are only available in internal "for debugging"
  // information. Better to access that here than making clients do it.
  return GetLastCycleSnapshotForDebugging().is_initialized();
}

bool SyncService::IsSyncFeatureEnabled() const {
  // Note: IsFirstSetupComplete() shouldn't usually be true if we don't have a
  // primary account, but it could happen if the account changes from primary to
  // secondary.
  return CanSyncFeatureStart() && GetUserSettings()->IsFirstSetupComplete();
}

bool SyncService::CanSyncFeatureStart() const {
  return GetDisableReasons().Empty() && HasSyncConsent();
}

bool SyncService::IsEngineInitialized() const {
  switch (GetTransportState()) {
    case TransportState::DISABLED:
    case TransportState::PAUSED:
    case TransportState::START_DEFERRED:
    case TransportState::INITIALIZING:
      return false;
    case TransportState::PENDING_DESIRED_CONFIGURATION:
    case TransportState::CONFIGURING:
    case TransportState::ACTIVE:
      return true;
  }
  NOTREACHED();
  return false;
}

bool SyncService::IsSyncFeatureActive() const {
  if (!IsSyncFeatureEnabled()) {
    return false;
  }
  switch (GetTransportState()) {
    case TransportState::DISABLED:
    case TransportState::PAUSED:
    case TransportState::START_DEFERRED:
    case TransportState::INITIALIZING:
    case TransportState::PENDING_DESIRED_CONFIGURATION:
      return false;
    case TransportState::CONFIGURING:
    case TransportState::ACTIVE:
      return true;
  }
  NOTREACHED();
  return false;
}

bool SyncService::HasUnrecoverableError() const {
  return HasDisableReason(DISABLE_REASON_UNRECOVERABLE_ERROR);
}

}  // namespace syncer
