// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/backoff_settings.h"

#include <memory>

#include "net/base/backoff_entry.h"

namespace reporting {

std::unique_ptr<::net::BackoffEntry> GetBackoffEntry() {
  // Retry starts with 10 second delay and is doubled with every failure.
  static const net::BackoffEntry::Policy kDefaultUploadBackoffPolicy = {
      // Number of initial errors to ignore before applying
      // exponential back-off rules.
      /*num_errors_to_ignore=*/0,

      // Initial delay is 10 seconds.
      /*initial_delay_ms=*/10 * 1000,

      // Factor by which the waiting time will be multiplied.
      /*multiply_factor=*/2,

      // Fuzzing percentage.
      /*jitter_factor=*/0.1,

      // Maximum delay is 90 seconds.
      /*maximum_backoff_ms=*/90 * 1000,

      // It's up to the caller to reset the backoff time.
      /*entry_lifetime_ms=*/-1,

      /*always_use_initial_delay=*/true,
  };
  return std::make_unique<::net::BackoffEntry>(&kDefaultUploadBackoffPolicy);
}

}  // namespace reporting
