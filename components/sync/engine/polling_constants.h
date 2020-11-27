// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
#define COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_

#include <stdint.h>

#include "base/time/time.h"

namespace syncer {

// Constants used by SyncScheduler when polling servers for updates.
extern const base::TimeDelta kDefaultPollInterval;
extern const base::TimeDelta kMaxBackoffTime;
extern const double kBackoffMultiplyFactor;
extern const double kBackoffJitterFactor;
extern const base::TimeDelta kInitialBackoffRetryTime;
extern const base::TimeDelta kInitialBackoffShortRetryTime;
extern const base::TimeDelta kInitialBackoffImmediateRetryTime;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
