// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
#define COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_

#include <stdint.h>

#include "base/time/time.h"

namespace syncer {

// Constants used by SyncScheduler when polling servers for updates.

// Factor by which the backoff time will be multiplied.
constexpr inline double kBackoffMultiplyFactor = 2.0;

// Backoff interval randomization factor.
constexpr inline double kBackoffJitterFactor = 0.5;

// Server can overwrite these values via client commands.
// We use high values here to ensure that failure to receive poll updates from
// the server doesn't result in rapid-fire polling from the client due to low
// local limits.
constexpr inline base::TimeDelta kDefaultPollInterval = base::Hours(8);

// Minimum and maximum interval for exponential backoff.
constexpr inline base::TimeDelta kMinBackoffTime = base::Seconds(1);
constexpr inline base::TimeDelta kMaxBackoffTime = base::Minutes(10);

// After a failure contacting sync servers, specifies how long to wait before
// reattempting and entering exponential backoff if consecutive failures
// occur.
constexpr inline base::TimeDelta kInitialBackoffRetryTime = base::Seconds(30);

// A dangerously short retry value that would not actually protect servers from
// DDoS if it were used as a seed for exponential backoff, although the client
// would still follow exponential backoff.  Useful for debugging and tests (when
// you don't want to wait 5 minutes).
constexpr inline base::TimeDelta kInitialBackoffShortRetryTime =
    base::Seconds(1);

// Similar to kInitialBackoffRetryTime above, but only to be used in
// certain exceptional error cases, such as MIGRATION_DONE.
constexpr inline base::TimeDelta kInitialBackoffImmediateRetryTime =
    base::Seconds(0);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_POLLING_CONSTANTS_H_
