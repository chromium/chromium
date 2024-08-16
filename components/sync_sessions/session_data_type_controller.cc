// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_data_type_controller.h"

#include <utility>

namespace sync_sessions {

SessionDataTypeController::SessionDataTypeController(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode)
    : DataTypeController(syncer::SESSIONS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      helper_(syncer::SESSIONS, sync_service, pref_service) {}

SessionDataTypeController::~SessionDataTypeController() = default;

syncer::DataTypeController::PreconditionState
SessionDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return helper_.GetPreconditionState();
}

}  // namespace sync_sessions
