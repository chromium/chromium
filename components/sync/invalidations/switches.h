// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SWITCHES_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SWITCHES_H_

#include "base/feature_list.h"

namespace switches {

// If enabled, interested data types will be sent to the Sync Server as part of
// DeviceInfo.
extern const base::Feature kSyncSendInterestedDataTypes;

// If enabled, the device will register with FCM and listen to new
// invalidations. Also, FCM token will be set in DeviceInfo, which signals to
// the server that device listens to new invalidations.
// SyncSendInterestedDataTypes must be enabled for this to take effect.
extern const base::Feature kUseSyncInvalidations;

}  // namespace switches

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SWITCHES_H_
