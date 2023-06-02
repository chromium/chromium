// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_model_type_controller.h"

#include <utility>

namespace sync_sessions {

// TODO(crbug.com/1448887): Create a `delegate_for_transport_mode`.
SessionModelTypeController::SessionModelTypeController(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate)
    : ModelTypeController(syncer::SESSIONS,
                          std::move(delegate),
                          /*delegate_for_transport_mode=*/nullptr),
      helper_(syncer::SESSIONS, sync_service, pref_service) {}

SessionModelTypeController::~SessionModelTypeController() = default;

syncer::DataTypeController::PreconditionState
SessionModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return helper_.GetPreconditionState();
}

}  // namespace sync_sessions
