// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"

namespace sync_sessions {

SessionModelTypeController::SessionModelTypeController(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate)
    : ModelTypeController(syncer::SESSIONS, std::move(delegate)),
      sync_service_(sync_service),
      pref_service_(pref_service) {
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::BindRepeating(
          &SessionModelTypeController::OnSavingBrowserHistoryPrefChanged,
          base::AsWeakPtr(this)));
}

SessionModelTypeController::~SessionModelTypeController() = default;

syncer::DataTypeController::PreconditionState
SessionModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)
             ? PreconditionState::kMustStopAndKeepData
             : PreconditionState::kPreconditionsMet;
}

void SessionModelTypeController::OnSavingBrowserHistoryPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace sync_sessions
