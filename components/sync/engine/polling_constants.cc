// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/polling_constants.h"

namespace syncer {

// Server can overwrite these values via client commands.
// We use high values here to ensure that failure to receive poll updates from
// the server doesn't result in rapid-fire polling from the client due to low
// local limits.
const int64_t kDefaultPollIntervalSeconds = 3600 * 8;

// Maximum interval for exponential backoff.
const int64_t kMaxBackoffSeconds = 60 * 10;  // 10 minutes.

// Backoff interval randomization factor.
const int kBackoffRandomizationFactor = 2;

// After a failure contacting sync servers, specifies how long to wait before
// reattempting and entering exponential backoff if consecutive failures
// occur.
const int kInitialBackoffRetrySeconds = 30;  // 30 seconds.

// A dangerously short retry value that would not actually protect servers from
// DDoS if it were used as a seed for exponential backoff, although the client
// would still follow exponential backoff.  Useful for debugging and tests (when
// you don't want to wait 5 minutes).
const int kInitialBackoffShortRetrySeconds = 1;

// Similar to kInitialBackoffRetrySeconds above, but only to be used in
// certain exceptional error cases, such as MIGRATION_DONE.
const int kInitialBackoffImmediateRetrySeconds = 0;

}  // namespace syncer
