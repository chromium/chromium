// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_model_type_controller.h"

#include <memory>

#include "components/history/core/browser/history_service.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace history {

namespace {

std::unique_ptr<syncer::ModelTypeControllerDelegate>
GetDelegateFromHistoryService(syncer::ModelType model_type,
                              HistoryService* history_service) {
  if (!history_service) {
    return nullptr;
  }

  if (model_type == syncer::TYPED_URLS) {
    return history_service->GetTypedURLSyncControllerDelegate();
  }
  DCHECK_EQ(model_type, syncer::HISTORY);
  return history_service->GetHistorySyncControllerDelegate();
}

}  // namespace

HistoryModelTypeController::HistoryModelTypeController(
    syncer::ModelType model_type,
    syncer::SyncService* sync_service,
    HistoryService* history_service,
    PrefService* pref_service)
    : ModelTypeController(
          model_type,
          GetDelegateFromHistoryService(model_type, history_service)),
      helper_(model_type, sync_service, pref_service) {
  DCHECK(model_type == syncer::TYPED_URLS || model_type == syncer::HISTORY);
  DCHECK(model_type == syncer::TYPED_URLS ||
         base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType));
}

HistoryModelTypeController::~HistoryModelTypeController() = default;

syncer::DataTypeController::PreconditionState
HistoryModelTypeController::GetPreconditionState() const {
  if (base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType)) {
    // If the feature flag is enabled, syncer::HISTORY replaces
    // syncer::TYPED_URLS.
    // TODO(crbug.com/1318028): Consider whether this is the best way to go
    // about things - maybe we'll want to keep the TypedURLs (meta)data for now?
    if (type() == syncer::TYPED_URLS) {
      return PreconditionState::kMustStopAndClearData;
    }
    DCHECK_EQ(type(), syncer::HISTORY);
    // syncer::HISTORY doesn't support custom passphrase encryption.
    if (helper_.sync_service()
            ->GetUserSettings()
            ->IsEncryptEverythingEnabled()) {
      return PreconditionState::kMustStopAndClearData;
    }
  }
  return helper_.GetPreconditionState();
}

void HistoryModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(NOT_RUNNING, state());

  if (type() == syncer::HISTORY) {
    helper_.sync_service()->AddObserver(this);
  }

  syncer::ModelTypeController::LoadModels(configure_context,
                                          model_load_callback);
}

void HistoryModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                      StopCallback callback) {
  DCHECK(CalledOnValidThread());

  if (type() == syncer::HISTORY) {
    helper_.sync_service()->RemoveObserver(this);
  }

  syncer::ModelTypeController::Stop(shutdown_reason, std::move(callback));
}

void HistoryModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(type(), syncer::HISTORY);
  // Most of these calls will be no-ops but SyncService handles that just fine.
  helper_.sync_service()->DataTypePreconditionChanged(type());
}

}  // namespace history
