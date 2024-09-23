// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_DATA_TYPE_STATE_HELPER_H_
#define COMPONENTS_SYNC_PROTOCOL_DATA_TYPE_STATE_HELPER_H_

#include "components/sync/base/data_type.h"

namespace sync_pb {
enum DataTypeState_InitialSyncState : int;
}  // namespace sync_pb

namespace syncer {

bool IsInitialSyncDone(sync_pb::DataTypeState_InitialSyncState state);

bool IsInitialSyncAtLeastPartiallyDone(
    sync_pb::DataTypeState_InitialSyncState state);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_DATA_TYPE_STATE_HELPER_H_
