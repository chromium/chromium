// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/backoff_delay_provider.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/engine/syncer_error.h"

namespace syncer {

namespace {

// This calculates approx. last_delay * kBackoffMultiplyFactor +/- last_delay
// * kBackoffJitterFactor. |jitter_sign| must be -1 or 1 and determines whether
// the jitter in the delay will be positive or negative.
base::TimeDelta GetDelayImpl(base::TimeDelta last_delay, int jitter_sign) {
  DCHECK(jitter_sign == -1 || jitter_sign == 1);

  if (last_delay >= kMaxBackoffTime) {
    return kMaxBackoffTime;
  }

  const base::TimeDelta backoff =
      std::max(kMinBackoffTime, last_delay * kBackoffMultiplyFactor) +
      jitter_sign * kBackoffJitterFactor * last_delay;

  // Clamp backoff between 1 second and |kMaxBackoffTime|.
  return std::max(kMinBackoffTime, std::min(backoff, kMaxBackoffTime));
}

}  // namespace

// static
std::unique_ptr<BackoffDelayProvider> BackoffDelayProvider::FromDefaults() {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BackoffDelayProvider(
      kInitialBackoffRetryTime, kInitialBackoffImmediateRetryTime));
}

// static
std::unique_ptr<BackoffDelayProvider>
BackoffDelayProvider::WithShortInitialRetryOverride() {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new BackoffDelayProvider(
      kInitialBackoffShortRetryTime, kInitialBackoffImmediateRetryTime));
}

BackoffDelayProvider::BackoffDelayProvider(
    const base::TimeDelta& default_initial_backoff,
    const base::TimeDelta& short_initial_backoff)
    : default_initial_backoff_(default_initial_backoff),
      short_initial_backoff_(short_initial_backoff) {}

BackoffDelayProvider::~BackoffDelayProvider() = default;

base::TimeDelta BackoffDelayProvider::GetDelay(
    const base::TimeDelta& last_delay) {
  // Flip a coin to randomize backoff interval by +/- kBackoffJitterFactor.
  const int jitter_sign = base::RandInt(0, 1) * 2 - 1;
  return GetDelayImpl(last_delay, jitter_sign);
}

base::TimeDelta BackoffDelayProvider::GetInitialDelay(
    const ModelNeutralState& state) const {
  // kNetworkError implies we did not receive HTTP response from server because
  // of some network error. If network is unavailable then on next connection
  // type or address change scheduler will run canary job. Otherwise we'll retry
  // in 30 seconds.
  if (state.commit_result.type() == SyncerError::Type::kNetworkError ||
      state.last_download_updates_result.type() ==
          SyncerError::Type::kNetworkError) {
    return default_initial_backoff_;
  }

  if (state.last_get_key_failed) {
    return default_initial_backoff_;
  }

  // Note: If we received a MIGRATION_DONE on download updates, then commit
  // should not have taken place.  Moreover, if we receive a MIGRATION_DONE
  // on commit, it means that download updates succeeded.  Therefore, we only
  // need to check if either code is equal to SERVER_RETURN_MIGRATION_DONE,
  // and not if there were any more serious errors requiring the long retry.
  if (state.last_download_updates_result.type() ==
          SyncerError::Type::kProtocolError &&
      state.last_download_updates_result.GetProtocolErrorOrDie() ==
          SyncProtocolErrorType::MIGRATION_DONE) {
    return short_initial_backoff_;
  }
  if (state.commit_result.type() == SyncerError::Type::kProtocolError &&
      state.commit_result.GetProtocolErrorOrDie() ==
          SyncProtocolErrorType::MIGRATION_DONE) {
    return short_initial_backoff_;
  }

  // When the server tells us we have a conflict, then we should download the
  // latest updates so we can see the conflict ourselves, resolve it locally,
  // then try again to commit.  Running another sync cycle will do all these
  // things.  There's no need to back off, we can do this immediately.
  //
  // TODO(sync): We shouldn't need to handle this in BackoffDelayProvider.
  // There should be a way to deal with protocol errors before we get to this
  // point.
  if (state.commit_result.type() == SyncerError::Type::kProtocolError &&
      state.commit_result.GetProtocolErrorOrDie() ==
          SyncProtocolErrorType::CONFLICT) {
    return short_initial_backoff_;
  }

  return default_initial_backoff_;
}

base::TimeDelta BackoffDelayProvider::GetDelayForTesting(
    base::TimeDelta last_delay,
    int jitter_sign) {
  return GetDelayImpl(last_delay, jitter_sign);
}

}  // namespace syncer
