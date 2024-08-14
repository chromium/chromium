// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/data_type_state_helper.h"

#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer {

bool IsInitialSyncDone(sync_pb::DataTypeState::InitialSyncState state) {
  switch (state) {
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED:
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE:
      return false;
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE:
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY:
      return true;
  }
}

bool IsInitialSyncAtLeastPartiallyDone(
    sync_pb::DataTypeState::InitialSyncState state) {
  switch (state) {
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED:
      return false;
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE:
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE:
    case sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY:
      return true;
  }
}

}  // namespace syncer
