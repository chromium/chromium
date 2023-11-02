// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_BACKOFF_DELAY_PROVIDER_H_
#define COMPONENTS_SYNC_ENGINE_BACKOFF_DELAY_PROVIDER_H_

#include <memory>

#include "base/time/time.h"

namespace syncer {

struct ModelNeutralState;

// A component used to get time delays associated with exponential backoff.
class BackoffDelayProvider {
 public:
  // Factory function to create a standard BackoffDelayProvider.
  static std::unique_ptr<BackoffDelayProvider> FromDefaults();

  // Similar to above, but causes sync to retry very quickly (see
  // polling_constants.h) when it encounters an error before exponential
  // backoff.
  //
  // *** NOTE *** This should only be used if kSyncShortInitialRetryOverride
  // was passed to command line.
  static std::unique_ptr<BackoffDelayProvider> WithShortInitialRetryOverride();

  BackoffDelayProvider(const BackoffDelayProvider&) = delete;
  BackoffDelayProvider& operator=(const BackoffDelayProvider&) = delete;

  virtual ~BackoffDelayProvider();

  // DDOS avoidance function.  Calculates how long we should wait before trying
  // again after a failed sync attempt, where the last delay was |base_delay|.
  // TODO(tim): Look at URLRequestThrottlerEntryInterface.
  virtual base::TimeDelta GetDelay(const base::TimeDelta& last_delay);

  // Helper to calculate the initial value for exponential backoff.
  // See possible values and comments in polling_constants.h.
  virtual base::TimeDelta GetInitialDelay(const ModelNeutralState& state) const;

  // Test-only variant that avoids randomness in tests. |jitter_sign| must be -1
  // or 1 and determines whether the jitter in the delay will be positive or
  // negative.
  base::TimeDelta GetDelayForTesting(base::TimeDelta last_delay,
                                     int jitter_sign);

 protected:
  BackoffDelayProvider(const base::TimeDelta& default_initial_backoff,
                       const base::TimeDelta& short_initial_backoff);

 private:
  const base::TimeDelta default_initial_backoff_;
  const base::TimeDelta short_initial_backoff_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_BACKOFF_DELAY_PROVIDER_H_
