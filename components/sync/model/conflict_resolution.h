// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_
#define COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_

namespace syncer {

// An enum to represent the resolution of a data conflict. We either:
// 1) Use the local client data and update the server.
// 2) Use the remote server data and update the client.
// We use this enum for UMA and values shouldn't change.
enum class ConflictResolution {
  kChangesMatch,  // Exists for logging purposes.
  kUseLocal,
  kUseRemote,
  kUseNewDEPRECATED,  // Deprecated because it's not used in production code.
  kIgnoreLocalEncryption,   // Exists for logging purposes.
  kIgnoreRemoteEncryption,  // Exists for logging purposes.
  kTypeSize,
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_
