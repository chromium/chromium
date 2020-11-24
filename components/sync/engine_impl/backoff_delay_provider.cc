// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/backoff_delay_provider.h"

#include <stdint.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/engine/polling_constants.h"

using base::TimeDelta;

namespace syncer {

// static
std::unique_ptr<BackoffDelayProvider> BackoffDelayProvider::FromDefaults() {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BackoffDelayProvider(
      TimeDelta::FromSeconds(kInitialBackoffRetrySeconds),
      TimeDelta::FromSeconds(kInitialBackoffImmediateRetrySeconds)));
}

// static
std::unique_ptr<BackoffDelayProvider>
BackoffDelayProvider::WithShortInitialRetryOverride() {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BackoffDelayProvider(
      TimeDelta::FromSeconds(kInitialBackoffShortRetrySeconds),
      TimeDelta::FromSeconds(kInitialBackoffImmediateRetrySeconds)));
}

BackoffDelayProvider::BackoffDelayProvider(
    const base::TimeDelta& default_initial_backoff,
    const base::TimeDelta& short_initial_backoff)
    : default_initial_backoff_(default_initial_backoff),
      short_initial_backoff_(short_initial_backoff) {}

BackoffDelayProvider::~BackoffDelayProvider() {}

TimeDelta BackoffDelayProvider::GetDelay(const base::TimeDelta& last_delay) {
  if (last_delay.InSeconds() >= kMaxBackoffSeconds)
    return TimeDelta::FromSeconds(kMaxBackoffSeconds);

  // This calculates approx. base_delay_seconds * 2 +/- base_delay_seconds / 2
  int64_t backoff_s =
      std::max(static_cast<int64_t>(1),
               last_delay.InSeconds() * kBackoffRandomizationFactor);

  // Flip a coin to randomize backoff interval by +/- 50%.
  int rand_sign = base::RandInt(0, 1) * 2 - 1;

  // Truncation is adequate for rounding here.
  backoff_s =
      backoff_s +
      (rand_sign * (last_delay.InSeconds() / kBackoffRandomizationFactor));

  // Cap the backoff interval.
  backoff_s = std::max(static_cast<int64_t>(1),
                       std::min(backoff_s, kMaxBackoffSeconds));

  return TimeDelta::FromSeconds(backoff_s);
}

TimeDelta BackoffDelayProvider::GetInitialDelay(
    const ModelNeutralState& state) const {
  // NETWORK_CONNECTION_UNAVAILABLE implies we did not receive HTTP response
  // from server because of some network error. If network is unavailable then
  // on next connection type or address change scheduler will run canary job.
  // Otherwise we'll retry in 30 seconds.
  if (state.commit_result.value() ==
          SyncerError::NETWORK_CONNECTION_UNAVAILABLE ||
      state.last_download_updates_result.value() ==
          SyncerError::NETWORK_CONNECTION_UNAVAILABLE) {
    return default_initial_backoff_;
  }

  if (state.last_get_key_result.IsActualError())
    return default_initial_backoff_;

  // Note: If we received a MIGRATION_DONE on download updates, then commit
  // should not have taken place.  Moreover, if we receive a MIGRATION_DONE
  // on commit, it means that download updates succeeded.  Therefore, we only
  // need to check if either code is equal to SERVER_RETURN_MIGRATION_DONE,
  // and not if there were any more serious errors requiring the long retry.
  if (state.last_download_updates_result.value() ==
          SyncerError::SERVER_RETURN_MIGRATION_DONE ||
      state.commit_result.value() ==
          SyncerError::SERVER_RETURN_MIGRATION_DONE) {
    return short_initial_backoff_;
  }

  // If a datatype decides the GetUpdates must be retried (e.g. because the
  // context has been updated since the request), use the short delay.
  if (state.last_download_updates_result.value() ==
      SyncerError::DATATYPE_TRIGGERED_RETRY)
    return short_initial_backoff_;

  // When the server tells us we have a conflict, then we should download the
  // latest updates so we can see the conflict ourselves, resolve it locally,
  // then try again to commit.  Running another sync cycle will do all these
  // things.  There's no need to back off, we can do this immediately.
  //
  // TODO(sync): We shouldn't need to handle this in BackoffDelayProvider.
  // There should be a way to deal with protocol errors before we get to this
  // point.
  if (state.commit_result.value() == SyncerError::SERVER_RETURN_CONFLICT)
    return short_initial_backoff_;

  return default_initial_backoff_;
}

}  // namespace syncer
