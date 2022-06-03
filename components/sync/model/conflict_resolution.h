// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_
#define COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_

namespace syncer {

// An enum to represent the resolution of a data conflict. We either:
// 1) Use the local client data and update the server.
// 2) Use the remote server data and update the client.
enum class ConflictResolution {
  kChangesMatch,
  kUseLocal,
  kUseRemote,
  kIgnoreLocalEncryption,
  kIgnoreRemoteEncryption
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CONFLICT_RESOLUTION_H_
