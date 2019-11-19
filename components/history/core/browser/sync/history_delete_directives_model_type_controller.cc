// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_delete_directives_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/model/model_type_store_service.h"

namespace browser_sync {

HistoryDeleteDirectivesModelTypeController::
    HistoryDeleteDirectivesModelTypeController(
        const base::RepeatingClosure& dump_stack,
        syncer::SyncService* sync_service,
        syncer::ModelTypeStoreService* model_type_store_service,
        syncer::SyncClient* sync_client)
    : SyncableServiceBasedModelTypeController(
          syncer::HISTORY_DELETE_DIRECTIVES,
          model_type_store_service->GetStoreFactory(),
          sync_client->GetSyncableServiceForType(
              syncer::HISTORY_DELETE_DIRECTIVES),
          dump_stack),
      sync_service_(sync_service) {}

HistoryDeleteDirectivesModelTypeController::
    ~HistoryDeleteDirectivesModelTypeController() {}

syncer::DataTypeController::PreconditionState
HistoryDeleteDirectivesModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return sync_service_->GetUserSettings()->IsEncryptEverythingEnabled()
             ? PreconditionState::kMustStopAndClearData
             : PreconditionState::kPreconditionsMet;
}

void HistoryDeleteDirectivesModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(NOT_RUNNING, state());

  sync_service_->AddObserver(this);
  SyncableServiceBasedModelTypeController::LoadModels(configure_context,
                                                      model_load_callback);
}

void HistoryDeleteDirectivesModelTypeController::Stop(
    syncer::ShutdownReason shutdown_reason,
    StopCallback callback) {
  DCHECK(CalledOnValidThread());

  sync_service_->RemoveObserver(this);

  SyncableServiceBasedModelTypeController::Stop(shutdown_reason,
                                                std::move(callback));
}

void HistoryDeleteDirectivesModelTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  // Most of these calls will be no-ops but SyncService handles that just fine.
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace browser_sync
