// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
#define COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_

#include <stdint.h>

namespace syncer {

// Constants used by SyncScheduler when polling servers for updates.
extern const int64_t kDefaultPollIntervalSeconds;
extern const int64_t kMaxBackoffSeconds;
extern const int kBackoffRandomizationFactor;
extern const int kInitialBackoffRetrySeconds;
extern const int kInitialBackoffShortRetrySeconds;
extern const int kInitialBackoffImmediateRetrySeconds;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
