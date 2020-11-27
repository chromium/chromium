// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/polling_constants.h"

namespace syncer {

// Server can overwrite these values via client commands.
// We use high values here to ensure that failure to receive poll updates from
// the server doesn't result in rapid-fire polling from the client due to low
// local limits.
const base::TimeDelta kDefaultPollInterval = base::TimeDelta::FromHours(8);

// Maximum interval for exponential backoff.
const base::TimeDelta kMaxBackoffTime = base::TimeDelta::FromMinutes(10);

// Factor by which the backoff time will be multiplied.
const double kBackoffMultiplyFactor = 2.0;

// Backoff interval randomization factor.
const double kBackoffJitterFactor = 0.5;

// After a failure contacting sync servers, specifies how long to wait before
// reattempting and entering exponential backoff if consecutive failures
// occur.
const base::TimeDelta kInitialBackoffRetryTime =
    base::TimeDelta::FromSeconds(30);

// A dangerously short retry value that would not actually protect servers from
// DDoS if it were used as a seed for exponential backoff, although the client
// would still follow exponential backoff.  Useful for debugging and tests (when
// you don't want to wait 5 minutes).
const base::TimeDelta kInitialBackoffShortRetryTime =
    base::TimeDelta::FromSeconds(1);

// Similar to kInitialBackoffRetryTime above, but only to be used in
// certain exceptional error cases, such as MIGRATION_DONE.
const base::TimeDelta kInitialBackoffImmediateRetryTime =
    base::TimeDelta::FromSeconds(0);

}  // namespace syncer
