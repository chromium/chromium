// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_model_type_controller_helper.h"

#include "base/bind.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"

namespace history {

HistoryModelTypeControllerHelper::HistoryModelTypeControllerHelper(
    syncer::ModelType model_type,
    syncer::SyncService* sync_service,
    PrefService* pref_service)
    : model_type_(model_type),
      sync_service_(sync_service),
      pref_service_(pref_service) {
  pref_registrar_.Init(pref_service_);
  // base::Unretained() is safe because `pref_registar_` is owned by `this`.
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::BindRepeating(&HistoryModelTypeControllerHelper::
                              OnSavingBrowserHistoryDisabledChanged,
                          base::Unretained(this)));
}

HistoryModelTypeControllerHelper::~HistoryModelTypeControllerHelper() = default;

syncer::DataTypeController::PreconditionState
HistoryModelTypeControllerHelper::GetPreconditionState() const {
  return pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)
             ? syncer::DataTypeController::PreconditionState::
                   kMustStopAndClearData
             : syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void HistoryModelTypeControllerHelper::OnSavingBrowserHistoryDisabledChanged() {
  sync_service_->DataTypePreconditionChanged(model_type_);
}

}  // namespace history
