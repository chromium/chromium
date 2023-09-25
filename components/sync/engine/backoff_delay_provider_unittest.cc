// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/backoff_delay_provider.h"

#include <memory>

#include "components/sync/engine/cycle/model_neutral_state.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/engine/syncer_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Gt;
using testing::Lt;

TEST(BackoffDelayProviderTest, GetRecommendedDelay) {
  std::unique_ptr<BackoffDelayProvider> delay(
      BackoffDelayProvider::FromDefaults());
  EXPECT_EQ(base::Seconds(1), delay->GetDelay(base::Seconds(0)));
  EXPECT_LE(base::Seconds(1), delay->GetDelay(base::Seconds(1)));
  EXPECT_LE(base::Seconds(50), delay->GetDelay(base::Seconds(50)));
  EXPECT_LE(base::Seconds(10), delay->GetDelay(base::Seconds(10)));
  EXPECT_EQ(kMaxBackoffTime, delay->GetDelay(kMaxBackoffTime));
  EXPECT_EQ(kMaxBackoffTime,
            delay->GetDelay(kMaxBackoffTime + base::Seconds(1)));
}

TEST(BackoffDelayProviderTest, GetInitialDelay) {
  std::unique_ptr<BackoffDelayProvider> delay(
      BackoffDelayProvider::FromDefaults());
  ModelNeutralState state;
  state.last_get_key_failed = true;
  EXPECT_EQ(kInitialBackoffRetryTime, delay->GetInitialDelay(state));

  state.last_get_key_failed = false;
  state.last_download_updates_result =
      SyncerError::ProtocolError(MIGRATION_DONE);
  EXPECT_EQ(kInitialBackoffImmediateRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result =
      SyncerError::NetworkError(net::ERR_FAILED);
  EXPECT_EQ(kInitialBackoffRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result =
      SyncerError::ProtocolError(TRANSIENT_ERROR);
  EXPECT_EQ(kInitialBackoffRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result = SyncerError::ProtocolViolationError();
  EXPECT_EQ(kInitialBackoffRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result = SyncerError::Success();
  state.commit_result = SyncerError::ProtocolError(MIGRATION_DONE);
  EXPECT_EQ(kInitialBackoffImmediateRetryTime, delay->GetInitialDelay(state));

  state.commit_result = SyncerError::NetworkError(net::ERR_FAILED);
  EXPECT_EQ(kInitialBackoffRetryTime, delay->GetInitialDelay(state));

  state.commit_result = SyncerError::ProtocolError(CONFLICT);
  EXPECT_EQ(kInitialBackoffImmediateRetryTime, delay->GetInitialDelay(state));
}

TEST(BackoffDelayProviderTest, GetInitialDelayWithOverride) {
  std::unique_ptr<BackoffDelayProvider> delay(
      BackoffDelayProvider::WithShortInitialRetryOverride());
  ModelNeutralState state;
  state.last_get_key_failed = true;
  EXPECT_EQ(kInitialBackoffShortRetryTime, delay->GetInitialDelay(state));

  state.last_get_key_failed = false;
  state.last_download_updates_result =
      SyncerError::ProtocolError(MIGRATION_DONE);
  EXPECT_EQ(kInitialBackoffImmediateRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result =
      SyncerError::ProtocolError(TRANSIENT_ERROR);
  EXPECT_EQ(kInitialBackoffShortRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result = SyncerError::ProtocolViolationError();
  EXPECT_EQ(kInitialBackoffShortRetryTime, delay->GetInitialDelay(state));

  state.last_download_updates_result = SyncerError::Success();
  state.commit_result = SyncerError::ProtocolError(MIGRATION_DONE);
  EXPECT_EQ(kInitialBackoffImmediateRetryTime, delay->GetInitialDelay(state));

  state.commit_result = SyncerError::ProtocolError(CONFLICT);
  EXPECT_EQ(kInitialBackoffImmediateRetryTime, delay->GetInitialDelay(state));
}

// This rules out accidents with the constants.
TEST(BackoffDelayProviderTest, GetExponentiallyIncreasingDelay) {
  std::unique_ptr<BackoffDelayProvider> delay_provider(
      BackoffDelayProvider::FromDefaults());

  ASSERT_THAT(kBackoffMultiplyFactor, Gt(1.0));
  // Even when the jitter is negative, the delay should grow (overall
  // multiplicative factor bigger than 1).
  ASSERT_THAT(kBackoffJitterFactor, Lt(kBackoffMultiplyFactor - 1.0));

  const base::TimeDelta delay0 = base::Seconds(1);
  const base::TimeDelta delay1_min =
      delay_provider->GetDelayForTesting(delay0, /*jitter_sign=*/-1);
  const base::TimeDelta delay2_min =
      delay_provider->GetDelayForTesting(delay1_min, /*jitter_sign=*/-1);
  const base::TimeDelta delay1_max =
      delay_provider->GetDelayForTesting(delay0, /*jitter_sign=*/1);
  const base::TimeDelta delay2_max =
      delay_provider->GetDelayForTesting(delay1_max, /*jitter_sign=*/1);

  ASSERT_THAT(delay1_min, Lt(delay1_max));
  ASSERT_THAT(delay2_min, Lt(delay2_max));

  // The minimum value should increase faster than linearly.
  EXPECT_THAT(delay1_min, Gt(delay0));
  EXPECT_THAT(delay2_min, Gt(delay1_min));
  EXPECT_THAT(delay2_min - delay1_min, Gt(delay1_min - delay0));

  // The maximum value should increase faster than linearly.
  EXPECT_THAT(delay1_max, Gt(delay0));
  EXPECT_THAT(delay2_max, Gt(delay1_max));
  EXPECT_THAT(delay2_max - delay1_max, Gt(delay1_max - delay0));
}

}  // namespace

}  // namespace syncer
