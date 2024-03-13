// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/model_type_state_helper.h"

namespace syncer {

bool IsInitialSyncDone(sync_pb::ModelTypeState::InitialSyncState state) {
  switch (state) {
    case sync_pb::
        ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED:
    case sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE:
      return false;
    case sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE:
    case sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY:
      return true;
  }
}

bool IsInitialSyncAtLeastPartiallyDone(
    sync_pb::ModelTypeState::InitialSyncState state) {
  switch (state) {
    case sync_pb::
        ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED:
      return false;
    case sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE:
    case sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE:
    case sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY:
      return true;
  }
}

bool MigrateLegacyInitialSyncDone(sync_pb::ModelTypeState& model_type_state,
                                  ModelType type) {
  // TODO(crbug.com/1423338): This migration logic was added in M113. Remove it
  // after a grace period of a year or so (M124ish).

  if (model_type_state.has_initial_sync_state()) {
    // Already migrated; nothing to do here.
    return false;
  }
  // Migrate from the deprecated `initial_sync_done` flag to the
  // `initial_sync_state` enum.
  if (model_type_state.initial_sync_done_deprecated()) {
    model_type_state.set_initial_sync_state(
        CommitOnlyTypes().Has(type)
            ? sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY
            : sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    return true;
  }
  return false;
}

}  // namespace syncer
