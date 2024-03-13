// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_MODEL_TYPE_STATE_HELPER_H_
#define COMPONENTS_SYNC_PROTOCOL_MODEL_TYPE_STATE_HELPER_H_

#include "components/sync/base/model_type.h"
#include "components/sync/protocol/model_type_state.pb.h"

namespace syncer {

bool IsInitialSyncDone(sync_pb::ModelTypeState::InitialSyncState state);

bool IsInitialSyncAtLeastPartiallyDone(
    sync_pb::ModelTypeState::InitialSyncState state);

// Migrates `model_type_state` in-place from the deprecated `initial_sync_done`
// flag to the new `initial_sync_state` enum. Returns whether a migration
// actually happened and `model_type_state` was modified.
bool MigrateLegacyInitialSyncDone(sync_pb::ModelTypeState& model_type_state,
                                  ModelType type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_MODEL_TYPE_STATE_HELPER_H_
