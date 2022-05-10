// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_model_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"

namespace history {

namespace {

std::unique_ptr<syncer::ModelTypeControllerDelegate>
GetDelegateFromHistoryService(HistoryService* history_service) {
  if (history_service) {
    return history_service->GetTypedURLSyncControllerDelegate();
  }
  return nullptr;
}

}  // namespace

TypedURLModelTypeController::TypedURLModelTypeController(
    syncer::SyncService* sync_service,
    HistoryService* history_service,
    PrefService* pref_service)
    : ModelTypeController(syncer::TYPED_URLS,
                          GetDelegateFromHistoryService(history_service)),
      helper_(syncer::TYPED_URLS, sync_service, pref_service) {}

TypedURLModelTypeController::~TypedURLModelTypeController() = default;

syncer::DataTypeController::PreconditionState
TypedURLModelTypeController::GetPreconditionState() const {
  return helper_.GetPreconditionState();
}

}  // namespace history
