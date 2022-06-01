// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_model_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/base/features.h"

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

TypedURLModelTypeController::TypedURLModelTypeController(
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

TypedURLModelTypeController::~TypedURLModelTypeController() = default;

syncer::DataTypeController::PreconditionState
TypedURLModelTypeController::GetPreconditionState() const {
  if (base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType)) {
    // If the feature flag is enabled, syncer::HISTORY replaces
    // syncer::TYPED_URLS.
    // TODO(crbug.com/1318028): Consider whether this is the best way to go
    // about things - maybe we'll want to keep the TypedURLs (meta)data for now?
    if (type() == syncer::TYPED_URLS) {
      return PreconditionState::kMustStopAndClearData;
    }
  }
  return helper_.GetPreconditionState();
}

}  // namespace history
