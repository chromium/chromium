// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_backoff_policy.h"

namespace gcm {

namespace {

// Backoff policy. Shared across GCM requests.
// Note: In order to ensure a minimum of 20 seconds between server errors (for
// server reasons), we have a 30s +- 10s (33%) jitter initial backoff.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  30 * 1000,  // 30 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.33,  // 33%.

  // Maximum amount of time we are willing to delay our request in ms.
  10 * 60 * 1000, // 10 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

const net::BackoffEntry::Policy& GetGCMBackoffPolicy() {
  return kDefaultBackoffPolicy;
}

}  // namespace gcm
