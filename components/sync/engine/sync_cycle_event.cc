// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_cycle_event.h"

namespace syncer {

SyncCycleEvent::SyncCycleEvent(EventCause cause) : what_happened(cause) {}

SyncCycleEvent::~SyncCycleEvent() = default;

}  // namespace syncer
