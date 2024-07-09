// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <sstream>
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "build/buildflag.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "net/http/http_status_code.h"

namespace feed {
namespace test {
namespace {

class FeedApiReliabilityLoggingTest : public FeedApiTest {};

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_LogFeedLaunchOtherStart) {
  TestForYouSurface surface(stream_.get());

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_EulaNotAccepted) {
  is_eula_accepted_ = false;
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinishedAfterStreamUpdate "
      "result=INELIGIBLE_EULA_NOT_ACCEPTED\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_ArticlesListHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinishedAfterStreamUpdate result=FEED_HIDDEN\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest,
       AttachSurface_DisabledByEnterprisePolicy) {
  profile_prefs_.SetBoolean(prefs::kEnableSnippets, false);
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinishedAfterStreamUpdate "
      "result=INELIGIBLE_DISCOVER_DISABLED_BY_ENTERPRISE_POLICY\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_DisabledByDse) {
  profile_prefs_.SetBoolean(prefs::kEnableSnippetsByDse, false);
  CreateStream(/*wait_for_initialization=*/true,
               /*is_new_tab_search_engine_url_android_enabled*/ true);
  TestForYouSurface surface(stream_.get());
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLaunchFinishedAfterStreamUpdate "
      "result=INELIGIBLE_DISCOVER_DISABLED_BY_DSE\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_ClearAllInProgress) {
  TestForYouSurface surface(stream_.get());
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      // First load attempt from attaching surface.
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogLaunchFinishedAfterStreamUpdate result=CLEAR_ALL_IN_PROGRESS\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n"

      // Second load attempt triggered by clear all.
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogLaunchFinishedAfterStreamUpdate "
      "result=NO_CARDS_REQUEST_ERROR_OTHER\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, AttachSurface_DataInStoreForAnotherUser) {
  stream_->SetMetadata(feedstore::MakeMetadata("some user id"));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      // First load attempt from attaching surface.
      "LogFeedLaunchOtherStart\n"

      "LogLaunchFinishedAfterStreamUpdate "
      "result=DATA_IN_STORE_IS_FOR_ANOTHER_USER\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n"

      // Second load attempt triggered by clear all.
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogLaunchFinishedAfterStreamUpdate "
      "result=NO_CARDS_REQUEST_ERROR_OTHER\n"
      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, MultipleSurfaces_SimultaneousLoad) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
      surface2.reliability_logging_bridge.GetEventsString());
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
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
      // `surface` attached
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
      surface.reliability_logging_bridge.GetEventsString());

  // `surface2` should only have logged from SurfaceUpdater::AttachSurface().
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogAboveTheFoldRender result=SUCCESS\n",
      surface2.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_Success) {
  base::Time request_recv_timestamp = base::Time::Now();
  task_environment_.AdvanceClock(base::Seconds(100));
  base::Time response_sent_timestamp = base::Time::Now();
  RefreshResponseData response;
  response.model_update_request = MakeTypicalInitialModelState();
  response.server_request_received_timestamp = request_recv_timestamp;
  response.server_response_sent_timestamp = response_sent_timestamp;
  response.last_fetch_timestamp = base::Time::Now();
  response_translator_.InjectResponse(std::move(response));

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(base::StrCat({"LogFeedLaunchOtherStart\n"
                          "LogLoadingIndicatorShown\n"

                          "LogCacheReadStart\n"
                          "LogCacheReadEnd result=EMPTY_SESSION\n"

                          "LogFeedRequestStart id=1\n"
                          "LogRequestSent id=1\n"
                          "LogResponseReceived id=1 receive_timestamp=",
                          base::NumberToString(feedstore::ToTimestampNanos(
                              request_recv_timestamp)),
                          " send_timestamp=",
                          base::NumberToString(feedstore::ToTimestampNanos(
                              response_sent_timestamp)),
                          "\nLogRequestFinished result=200 id=1\n"

                          "LogAboveTheFoldRender result=SUCCESS\n"}),
            surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_ZeroCards) {
  network_.InjectRealFeedQueryResponseWithNoContent();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=123456000 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogLoadingIndicatorShown\n"
      "LogLaunchFinishedAfterStreamUpdate "
      "result=NO_CARDS_RESPONSE_ERROR_ZERO_CARDS\n"

      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_NetworkOffline) {
  is_offline_ = true;
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogLaunchFinishedAfterStreamUpdate "
      "result=NO_CARDS_REQUEST_ERROR_NO_INTERNET\n"

      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadStreamComplete_NoResponseReceived) {
  network_.error = net::Error::ERR_TIMED_OUT;
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      // Should not call LogResponseReceived.
      "LogRequestFinished result=-7 id=1\n"

      "LogLaunchFinishedAfterStreamUpdate "
      "result=NO_CARDS_REQUEST_ERROR_OTHER\n"

      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest,
       LoadStreamComplete_ResponseReceivedWithHttpError) {
  network_.http_status_code = net::HttpStatusCode::HTTP_FORBIDDEN;
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=403 id=1\n"

      "LogLaunchFinishedAfterStreamUpdate "
      "result=NO_CARDS_RESPONSE_ERROR_NON_200\n"

      "LogAboveTheFoldRender result=FULL_FEED_ERROR\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, CacheRead_Stale) {
  store_->OverwriteStream(
      StreamType(StreamKind::kForYou),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().GetStalenessThreshold(
                                          StreamType(StreamKind::kForYou),
                                          /*is_web_feed_subscriber=*/true) -
                                      base::Minutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=STALE\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, CacheRead_StaleWithNetworkError) {
  network_.http_status_code = net::HttpStatusCode::HTTP_FORBIDDEN;
  store_->OverwriteStream(
      StreamType(StreamKind::kForYou),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().GetStalenessThreshold(
                                          StreamType(StreamKind::kForYou),
                                          /*is_web_feed_subscriber=*/true) -
                                      base::Minutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=STALE\n"

      "LogFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=403 id=1\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, CacheRead_Okay) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  WaitForIdleTaskQueue();

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=CACHE_READ_OK\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, UploadActions) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->UploadAction(MakeFeedAction(1ul), CreateLoggingParameters(),
                        /*upload_now=*/false, base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"

      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"

      "LogActionsUploadRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"

      "LogFeedRequestStart id=2\n"
      "LogRequestSent id=2\n"
      "LogResponseReceived id=2 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=2\n"

      "LogAboveTheFoldRender result=SUCCESS\n",
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

TEST_F(FeedApiReliabilityLoggingTest, WebFeedLoad) {
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> refresh_result;
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  stream_->subscriptions().RefreshSubscriptions(refresh_result.Bind());
  refresh_result.RunUntilCalled();
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"
      "LogWebFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"
      "LogAboveTheFoldRender result=SUCCESS\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, SingleWebFeedLoad) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSingleWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ(
      "LogFeedLaunchOtherStart\n"
      "LogLoadingIndicatorShown\n"
      "LogCacheReadStart\n"
      "LogCacheReadEnd result=EMPTY_SESSION\n"
      "LogSingleWebFeedRequestStart id=1\n"
      "LogRequestSent id=1\n"
      "LogResponseReceived id=1 receive_timestamp=0 send_timestamp=0\n"
      "LogRequestFinished result=200 id=1\n"
      "LogAboveTheFoldRender result=SUCCESS\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadMoreSucceeds) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.reliability_logging_bridge.ClearEventsString();

  base::Time request_recv_timestamp = base::Time::Now();
  task_environment_.AdvanceClock(base::Seconds(100));
  base::Time response_sent_timestamp = base::Time::Now();
  RefreshResponseData response;
  response.model_update_request = MakeTypicalNextPageState();
  response.server_request_received_timestamp = request_recv_timestamp;
  response.server_response_sent_timestamp = response_sent_timestamp;
  response.last_fetch_timestamp = base::Time::Now();
  response_translator_.InjectResponse(std::move(response));

  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(base::StrCat({"LogLoadMoreStarted\n"
                          "LogLoadMoreRequestSent\n"
                          "LogLoadMoreResponseReceived "
                          "receive_timestamp=",
                          base::NumberToString(feedstore::ToTimestampNanos(
                              request_recv_timestamp)),
                          " send_timestamp=",
                          base::NumberToString(feedstore::ToTimestampNanos(
                              response_sent_timestamp)),
                          "\nLogLoadMoreRequestFinished result=200\n"
                          "LogLoadMoreEnded success=true\n"}),
            surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadMoreFails) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.reliability_logging_bridge.ClearEventsString();

  // Don't inject another response, which results in a proto translation
  // failure.
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "LogLoadMoreStarted\n"
      "LogLoadMoreRequestSent\n"
      "LogLoadMoreResponseReceived receive_timestamp=0 send_timestamp=0\n"
      "LogLoadMoreRequestFinished result=200\n"
      "LogLoadMoreEnded success=false\n",
      surface.reliability_logging_bridge.GetEventsString());
}

TEST_F(FeedApiReliabilityLoggingTest, LoadMoreAbortsIfNoNextPageToken) {
  {
    std::unique_ptr<StreamModelUpdateRequest> initial_state =
        MakeTypicalInitialModelState();
    initial_state->stream_data.clear_next_page_token();
    response_translator_.InjectResponse(std::move(initial_state));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.reliability_logging_bridge.ClearEventsString();

  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ("", surface.reliability_logging_bridge.GetEventsString());
}

}  // namespace
}  // namespace test
}  // namespace feed
