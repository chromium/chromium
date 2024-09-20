// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/model_neutral_state.h"

#include "components/sync/engine/syncer_error.h"

namespace syncer {

ModelNeutralState::ModelNeutralState() = default;

ModelNeutralState::ModelNeutralState(const ModelNeutralState& other) = default;

ModelNeutralState::~ModelNeutralState() = default;

bool HasSyncerError(const ModelNeutralState& state) {
  return state.last_get_key_failed ||
         state.last_download_updates_result.type() !=
             SyncerError::Type::kSuccess ||
         state.commit_result.type() != SyncerError::Type::kSuccess;
}

}  // namespace syncer
