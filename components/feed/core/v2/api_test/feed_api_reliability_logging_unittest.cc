// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <sstream>
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "net/http/http_status_code.h"

namespace feed {
namespace test {
namespace {

class FeedApiReliabilityLoggingTest : public FeedApiTest {};

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_LogFeedLaunchOtherStart) {
  TestForYouSurface surface(stream_.get());

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_EulaNotAccepted) {
  is_eula_accepted_ = false;
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinished result=INELIGIBLE_EULA_NOT_ACCEPTED\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_ArticlesListHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinished result=FEED_HIDDEN\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest,
       AttachSurface_DisabledByEnterprisePolicy) {
  profile_prefs_.SetBoolean(prefs::kEnableSnippets, false);
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinished "
      "result=INELIGIBLE_DISCOVER_DISABLED_BY_ENTERPRISE_POLICY\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, MultipleSurfaces_SimultaneousLoad) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n",
      surface2.reliability_logging_bridge.GetEventsString());
  // `surface2` should only have logged from SurfaceUpdater::AttachSurface().
  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest,
       MultipleSurfaces_FullyLoadThenAttachAnother) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n",
      surface.reliability_logging_bridge.GetEventsString());

  // `surface2` should only have logged from SurfaceUpdater::AttachSurface().
  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n",
      surface2.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_Success) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_ZeroCards) {
  network_.InjectRealFeedQueryResponseWithNoContent();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"
      "LogLaunchFinished result=NO_CARDS_RESPONSE_ERROR_ZERO_CARDS\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_NetworkOffline) {
  is_offline_ = true;
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"
      "LogLaunchFinished result=NO_CARDS_REQUEST_ERROR_NO_INTERNET\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_Non200) {
  network_.http_status_code = net::HttpStatusCode::HTTP_FORBIDDEN;
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"
      "LogLaunchFinished result=NO_CARDS_RESPONSE_ERROR_NON_200\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, CacheRead_Stale) {
  store_->OverwriteStream(
      kForYouStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch -
              GetFeedConfig().GetStalenessThreshold(kForYouStream) -
              base::TimeDelta::FromMinutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=STALE\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, CacheRead_StaleWithNetworkError) {
  network_.http_status_code = net::HttpStatusCode::HTTP_FORBIDDEN;
  store_->OverwriteStream(
      kForYouStream,
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch -
              GetFeedConfig().GetStalenessThreshold(kForYouStream) -
              base::TimeDelta::FromMinutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=STALE\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, CacheRead_Okay) {
  store_->OverwriteStream(kForYouStream,
                          MakeTypicalInitialModelState(
                              /*first_cluster_id=*/0),
                          base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "SendPendingLaunchEvents stream_type=ForYou\n"
      "LogFeedLaunchOtherStart\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=CACHE_READ_OK\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, GetAndResetId) {
  // If nothing changes between two calls to GetReliabilityLoggingId(), it
  // should return the same ID.
  uint64_t first_id =
      FeedService::GetReliabilityLoggingId(/*metrics_id=*/"", &profile_prefs_);
  EXPECT_EQ(first_id, FeedService::GetReliabilityLoggingId(/*metrics_id=*/"",
                                                           &profile_prefs_));

  profile_prefs_.ClearPref(prefs::kReliabilityLoggingIdSalt);
  EXPECT_NE(first_id, FeedService::GetReliabilityLoggingId(/*metrics_id=*/"",
                                                           &profile_prefs_));
}

TEST_F(FeedApiReliabilityLoggingTest, IdChangeOnMetricsIdChange) {
  const char kSomeMetricsId[] = "metrics-id-1";
  uint64_t first_id =
      FeedService::GetReliabilityLoggingId(kSomeMetricsId, &profile_prefs_);
  EXPECT_NE(first_id, FeedService::GetReliabilityLoggingId("metrics-id-2",
                                                           &profile_prefs_));

  // If we use the original metrics ID, we should get the original ID unless
  // the salt is cleared.
  EXPECT_EQ(first_id, FeedService::GetReliabilityLoggingId(kSomeMetricsId,
                                                           &profile_prefs_));
  profile_prefs_.ClearPref(prefs::kReliabilityLoggingIdSalt);
  EXPECT_NE(first_id, FeedService::GetReliabilityLoggingId(kSomeMetricsId,
                                                           &profile_prefs_));
}

}  // namespace
}  // namespace test
}  // namespace feed