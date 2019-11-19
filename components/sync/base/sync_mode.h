// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_MODE_H_
#define COMPONENTS_SYNC_BASE_SYNC_MODE_H_

namespace syncer {

// Specifies whether the sync machinery is running in full-Sync mode (aka
// Sync-the-feature) or transport-only mode. In transport-only mode, only a
// subset of data types is allowed, and any local data is removed on sign-out.
// Passed as an argument when configuring sync / individual data types.
enum class SyncMode { kTransportOnly, kFull };

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_MODE_H_
