// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journey_data_type_controller.h"

#include <memory>
#include <utility>

#include "components/history/core/browser/history_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace history {

namespace {

std::unique_ptr<syncer::DataTypeControllerDelegate>
GetDelegateFromHistoryService(HistoryService* history_service) {
  if (!history_service) {
    return nullptr;
  }
  return history_service->GetJourneysSyncControllerDelegate();
}

}  // namespace

JourneyDataTypeController::JourneyDataTypeController(
    syncer::SyncService* sync_service,
    HistoryService* history_service,
    PrefService* pref_service)
    : DataTypeController(syncer::JOURNEY,
                         /*delegate_for_full_sync_mode=*/
                         GetDelegateFromHistoryService(history_service),
                         /*delegate_for_transport_mode=*/
                         GetDelegateFromHistoryService(history_service)),
      helper_(syncer::JOURNEY,
              sync_service,
              pref_service,
              HistoryDataTypeControllerHelper::AccountManagedStatusPolicy::
                  kDisallowEnterprise) {}

JourneyDataTypeController::~JourneyDataTypeController() = default;

syncer::DataTypeController::PreconditionState
JourneyDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  DCHECK(CalledOnValidThread());
  // syncer::JOURNEY doesn't support custom passphrase encryption.
  if (helper_.sync_service()->GetUserSettings()->IsEncryptEverythingEnabled()) {
    return PreconditionState::kMustStopAndClearData;
  }

  return helper_.GetPreconditionState(context);
}

}  // namespace history
