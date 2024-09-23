// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_data_type_controller_helper.h"

#include "base/functional/bind.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

namespace history {

HistoryDataTypeControllerHelper::HistoryDataTypeControllerHelper(
    syncer::DataType data_type,
    syncer::SyncService* sync_service,
    PrefService* pref_service)
    : data_type_(data_type),
      sync_service_(sync_service),
      pref_service_(pref_service) {
  pref_registrar_.Init(pref_service_);
  // base::Unretained() is safe because `pref_registar_` is owned by `this`.
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::BindRepeating(&HistoryDataTypeControllerHelper::
                              OnSavingBrowserHistoryDisabledChanged,
                          base::Unretained(this)));
}

HistoryDataTypeControllerHelper::~HistoryDataTypeControllerHelper() = default;

syncer::DataTypeController::PreconditionState
HistoryDataTypeControllerHelper::GetPreconditionState() const {
  return pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)
             ? syncer::DataTypeController::PreconditionState::
                   kMustStopAndClearData
             : syncer::DataTypeController::PreconditionState::
                   kPreconditionsMet;
}

void HistoryDataTypeControllerHelper::OnSavingBrowserHistoryDisabledChanged() {
  sync_service_->DataTypePreconditionChanged(data_type_);
}

}  // namespace history
