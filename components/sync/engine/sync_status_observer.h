// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_STATUS_OBSERVER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_STATUS_OBSERVER_H_

#include "components/sync/engine/sync_status.h"

namespace syncer {

class SyncStatusObserver {
 public:
  SyncStatusObserver() = default;
  virtual ~SyncStatusObserver() = default;

  // This event is sent when SyncStatus changes.
  virtual void OnSyncStatusChanged(const SyncStatus& status) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_STATUS_OBSERVER_H_
