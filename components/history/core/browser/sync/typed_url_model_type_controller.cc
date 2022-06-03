// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_model_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_client.h"

using syncer::SyncClient;

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
    HistoryService* history_service,
    PrefService* pref_service)
    : ModelTypeController(syncer::TYPED_URLS,
                          GetDelegateFromHistoryService(history_service)),
      history_service_(history_service),
      pref_service_(pref_service) {
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::BindRepeating(
          &TypedURLModelTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::AsWeakPtr(this)));
}

TypedURLModelTypeController::~TypedURLModelTypeController() {}

syncer::DataTypeController::PreconditionState
TypedURLModelTypeController::GetPreconditionState() const {
  return (history_service_ &&
          !pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled))
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void TypedURLModelTypeController::OnSavingBrowserHistoryDisabledChanged() {
  if (pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    // TODO(crbug.com/990802): Adopt DataTypePreconditionChanged().
    if (state() != NOT_RUNNING && state() != STOPPING) {
      ReportModelError(
          syncer::SyncError::DATATYPE_POLICY_ERROR,
          {FROM_HERE, "History saving is now disabled by policy."});
    }
  }
}

}  // namespace history
