// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/feed_entry_point_source.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"
#include "components/feed/core/proto/v2/wire/info_card.pb.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is primarily for testing access to the Feed content, though it
// contains some other miscellaneous tests.

namespace feed {
namespace test {
namespace {

using ::feedwire::webfeed::WebFeedChangeReason;

const int kTestInfoCardType1 = 101;
const int kTestInfoCardType2 = 8888;
const int kMinimumViewIntervalSeconds = 5 * 60;

TEST_F(FeedApiTest, IsArticlesListVisibleByDefault) {
  EXPECT_TRUE(stream_->IsArticlesListVisible());
}

TEST_F(FeedApiTest, DoNotRefreshIfArticlesListIsHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  EXPECT_FALSE(refresh_scheduler_.scheduled_run_times.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(std::set<RefreshTaskId>({RefreshTaskId::kRefreshForYouFeed}),
            refresh_scheduler_.completed_tasks);
}

TEST_F(FeedApiTest,
       DoNotRefreshIfSnippetsByDseDisabled_ignoredWithoutFlagEnabled) {
  profile_prefs_.SetBoolean(prefs::kEnableSnippetsByDse, false);
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();
  EXPECT_TRUE(refresh_scheduler_.scheduled_run_times.count(
      RefreshTaskId::kRefreshForYouFeed));
}

TEST_F(FeedApiTest, DoNotRefreshIfSnippetsByDseDisabled) {
  profile_prefs_.SetBoolean(prefs::kEnableSnippetsByDse, false);
  CreateStream(/*wait_for_initialization=*/true,
               /*is_new_tab_search_engine_url_android_enabled*/ true);
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(refresh_scheduler_.scheduled_run_times.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(std::set<RefreshTaskId>({RefreshTaskId::kRefreshForYouFeed}),
            refresh_scheduler_.completed_tasks);
#else
  WaitForIdleTaskQueue();
  EXPECT_TRUE(refresh_scheduler_.scheduled_run_times.count(
      RefreshTaskId::kRefreshForYouFeed));
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(FeedApiTest, BackgroundRefreshForYouSuccess) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify the refresh happened and that we can load a stream without the
  // network.
  ASSERT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->background_refresh_status);
  EXPECT_TRUE(network_.query_request_sent);
  EXPECT_EQ(feedwire::FeedQuery::SCHEDULED_REFRESH,
            network_.query_request_sent->feed_request().feed_query().reason());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  EXPECT_FALSE(stream_->GetModel(StreamType(StreamKind::kForYou)));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, WebFeedDoesNotBackgroundRefresh) {
  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::Seconds(12), base::Seconds(48)};

    injected_response.request_schedule = schedule;
    response_translator_.InjectResponse(std::move(injected_response));
  }

  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // The request schedule should be ignored.
  EXPECT_TRUE(refresh_scheduler_.scheduled_run_times.empty());
}

TEST_F(FeedApiTest, BackgroundRefreshPrefetchesImages) {
  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  EXPECT_EQ(0, prefetch_image_call_count_);
  WaitForIdleTaskQueue();

  std::vector<GURL> expected_fetches(
      {GURL("http://image0/"), GURL("http://favicon0/"), GURL("http://image1/"),
       GURL("http://favicon1/")});
  // Verify that images were prefetched.
  EXPECT_EQ(4, prefetch_image_call_count_);
  EXPECT_EQ(expected_fetches, prefetched_images_);
}

TEST_F(FeedApiTest, BackgroundRefreshNotAttemptedWhenModelIsLoading) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_->Stream(StreamType(StreamKind::kForYou))
                .background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedApiTest, BackgroundRefreshNotAttemptedAfterModelIsLoaded) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  EXPECT_EQ(metrics_reporter_->background_refresh_status,
            LoadStreamStatus::kModelAlreadyLoaded);
}

TEST_F(FeedApiTest, SurfaceReceivesInitialContent) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  // Use `the_first_surface` to force loading content.
  TestForYouSurface the_first_surface(stream_.get());
  WaitForIdleTaskQueue();

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(surface.initial_state);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());

  EXPECT_TRUE(initial_state.logging_parameters().logging_enabled());
  EXPECT_EQ(MakeRootEventId(),
            initial_state.logging_parameters().root_event_id());
}

TEST_F(FeedApiTest, SurfaceReceivesInitialContentLoadedAfterAttach) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  ASSERT_FALSE(surface.initial_state);
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();

  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:0", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:1", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  ASSERT_EQ(1, initial_state.new_shared_states().size());
  EXPECT_EQ("ss:0",
            initial_state.new_shared_states()[0].xsurface_shared_state());
}

TEST_F(FeedApiTest, SurfaceReceivesUpdatedContent) {
  {
    auto model = CreateStreamModel();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(StreamType(StreamKind::kForYou),
                                 std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  // Remove #1, add #2.
  stream_->ExecuteOperations(
      surface.GetSurfaceId(),
      {
          MakeOperation(MakeRemove(MakeClusterId(1))),
          MakeOperation(MakeCluster(2, MakeRootId())),
          MakeOperation(MakeContentNode(2, MakeClusterId(2))),
          MakeOperation(MakeContent(2)),
      });
  ASSERT_TRUE(surface.update);
  const feedui::StreamUpdate& initial_state = surface.initial_state.value();
  const feedui::StreamUpdate& update = surface.update.value();

  ASSERT_EQ("2 slices -> 2 slices", surface.DescribeUpdates());
  // First slice is just an ID that matches the old 1st slice ID.
  EXPECT_EQ(initial_state.updated_slices(0).slice().slice_id(),
            update.updated_slices(0).slice_id());
  // Second slice is a new xsurface slice.
  EXPECT_NE("", update.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:2",
            update.updated_slices(1).slice().xsurface_slice().xsurface_frame());
}

TEST_F(FeedApiTest, SurfaceReceivesSecondUpdatedContent) {
  {
    auto model = CreateStreamModel();
    model->ExecuteOperations(MakeTypicalStreamOperations());
    stream_->LoadModelForTesting(StreamType(StreamKind::kForYou),
                                 std::move(model));
  }
  TestForYouSurface surface(stream_.get());
  // Add #2.
  stream_->ExecuteOperations(
      surface.GetSurfaceId(),
      {
          MakeOperation(MakeCluster(2, MakeRootId())),
          MakeOperation(MakeContentNode(2, MakeClusterId(2))),
          MakeOperation(MakeContent(2)),
      });

  // Clear the last update and add #3.
  stream_->ExecuteOperations(
      surface.GetSurfaceId(),
      {
          MakeOperation(MakeCluster(3, MakeRootId())),
          MakeOperation(MakeContentNode(3, MakeClusterId(3))),
          MakeOperation(MakeContent(3)),
      });

  // The last update should have only one new piece of content.
  // This verifies the current content set is tracked properly.
  ASSERT_EQ("2 slices -> 3 slices -> 4 slices", surface.DescribeUpdates());

  ASSERT_EQ(4, surface.update->updated_slices().size());
  EXPECT_FALSE(surface.update->updated_slices(0).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(1).has_slice());
  EXPECT_FALSE(surface.update->updated_slices(2).has_slice());
  EXPECT_EQ("f:3", surface.update->updated_slices(3)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedApiTest, RemoveAllContentResultsInZeroState) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Remove both pieces of content.
  stream_->ExecuteOperations(surface.GetSurfaceId(),
                             {
                                 MakeOperation(MakeRemove(MakeClusterId(0))),
                                 MakeOperation(MakeRemove(MakeClusterId(1))),
                             });

  ASSERT_EQ("loading -> [user@foo] 2 slices -> no-cards",
            surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DetachSurface) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(surface.initial_state);
  surface.Detach();
  // Subsequent DetachSurface() calls are ignored.
  stream_->DetachSurface(surface.GetSurfaceId());
  surface.Clear();

  // Arbitrary stream change. Surface should not see the update.
  stream_->ExecuteOperations(surface.GetSurfaceId(),
                             {
                                 MakeOperation(MakeRemove(MakeClusterId(1))),
                             });
  EXPECT_FALSE(surface.update);
}

TEST_F(FeedApiTest, DetachSurfaceBeforeReceivingContent) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  surface.Detach();
  EXPECT_EQ("loading", surface.DescribeUpdates());
  WaitForIdleTaskQueue();
  EXPECT_EQ("", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, FetchImage) {
  CallbackReceiver<NetworkResponse> receiver;
  stream_->FetchImage(GURL("https://example.com"), receiver.Bind());

  EXPECT_EQ("dummyresponse", receiver.GetResult()->response_bytes);
}

TEST_F(FeedApiTest, FetchAsyncData) {
  FeedNetwork::RawResponse raw_response;
  raw_response.response_info.status_code = 200;
  raw_response.response_info.response_header_names_and_values = {
      "name1", "value1", "name2", "value2"};
  raw_response.response_bytes = "dummyresponse";
  network_.InjectRawResponse(raw_response);

  CallbackReceiver<NetworkResponse> receiver;
  stream_->FetchResource(GURL("https://example.com"), "POST", {}, "post data",
                         receiver.Bind());
  EXPECT_EQ(200, receiver.RunAndGetResult().status_code);
  EXPECT_EQ(raw_response.response_info.response_header_names_and_values,
            receiver.GetResult()->response_header_names_and_values);
  EXPECT_EQ("dummyresponse", receiver.GetResult()->response_bytes);
}

TEST_F(FeedStreamTestForAllStreamTypes,
       ReportContentLifetimeMetricsViaOnLoadStream) {
  base::HistogramTester histograms;
  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime;
    content_lifetime.set_stale_age_ms(10);
    content_lifetime.set_invalid_age_ms(11);
    injected_response.content_lifetime = std::move(content_lifetime);
    response_translator_.InjectResponse(std::move(injected_response));

    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_TRUE(response_translator_.InjectedResponseConsumed());
  }
  ASSERT_TRUE(network_.query_request_sent);

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.StaleAgeIsPresent", true, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAgeIsPresent", true, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.StaleAge", 10, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAge", 11, 1);
}

TEST_F(FeedStreamTestForAllStreamTypes,
       DoNotReportContentLifetimeMetricsWhenStreamMetadataNotPopulated) {
  base::HistogramTester histograms;
  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    response_translator_.InjectResponse(std::move(injected_response));

    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_TRUE(response_translator_.InjectedResponseConsumed());
  }
  ASSERT_TRUE(network_.query_request_sent);

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.StaleAgeIsPresent", false, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAgeIsPresent", false, 1);
}

TEST_F(FeedStreamTestForAllStreamTypes,
       DoNotReportContentLifetimeStaleAgeWhenNotPopulated) {
  base::HistogramTester histograms;
  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime;
    content_lifetime.set_invalid_age_ms(11);
    injected_response.content_lifetime = std::move(content_lifetime);
    response_translator_.InjectResponse(std::move(injected_response));

    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_TRUE(response_translator_.InjectedResponseConsumed());
  }
  ASSERT_TRUE(network_.query_request_sent);

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.StaleAgeIsPresent", false, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAgeIsPresent", true, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAge", 11, 1);
}

TEST_F(FeedStreamTestForAllStreamTypes,
       DoNotReportContentLifetimeInvalidAgeWhenNotPopulated) {
  base::HistogramTester histograms;
  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime;
    content_lifetime.set_stale_age_ms(10);
    injected_response.content_lifetime = std::move(content_lifetime);
    response_translator_.InjectResponse(std::move(injected_response));

    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_TRUE(response_translator_.InjectedResponseConsumed());
  }
  ASSERT_TRUE(network_.query_request_sent);

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.StaleAgeIsPresent", true, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.InvalidAgeIsPresent", false, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ContentLifetime.StaleAge", 10, 1);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFromNetwork) {
  {
    WaitForIdleTaskQueue();
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }

  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ(0, network_.GetApiRequestCount<QueryInteractiveFeedDiscoverApi>());
  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());

  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState()),
      stream_->GetModel(GetStreamType())->DumpStateForTesting());
  // Verify the data was written to the store.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       ModelStateFor(GetStreamType(), store_.get()));
}

TEST_P(FeedStreamTestForAllStreamTypes, UseFeedQueryOverride) {
  Config config = GetFeedConfig();
  config.use_feed_query_requests = true;
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(network_.query_request_sent);
  // There should be no API refresh requests when using use_feed_query_requests.
  auto api_request_counts = network_.GetApiRequestCounts();
  api_request_counts.erase(NetworkRequestType::kListWebFeeds);  // ignore
  EXPECT_EQ((std::map<NetworkRequestType, int>()), api_request_counts);
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, FetchAfterStartup) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // There should have been a fetch, even though there are no subscriptions.
  ASSERT_TRUE(network_.query_request_sent);
  ASSERT_EQ(1, network_.GetWebFeedListContentsCount());
}

TEST_F(FeedApiTest, WebFeedLoadWithNoSubscriptionsWithoutOnboarding) {
  // Disable the onboarding feature.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kWebFeedOnboarding);

  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> no-subscriptions", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, WebFeedLoadWithNoSubscriptions) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  // Scopes are to control the lifetime of the surface object.
  {
    TestWebFeedSurface surface(stream_.get());
    WaitForIdleTaskQueue();
  }
  // The initial fetch should work fine.
  ASSERT_EQ(1, network_.GetWebFeedListContentsCount());

  // Prepare the next fetch response.
  response_translator_.InjectResponse(MakeTypicalNextPageState());

  // Make the content a bit less than the stale threshold, and make sure we
  // don't fetch.
  task_environment_.FastForwardBy(base::Days(6));
  {
    TestWebFeedSurface surface(stream_.get());
    WaitForIdleTaskQueue();
  }
  ASSERT_EQ(1, network_.GetWebFeedListContentsCount());

  // Make the content as stale as the threshold, and make sure we do fetch.
  task_environment_.FastForwardBy(base::Days(2));
  {
    TestWebFeedSurface surface(stream_.get());
    WaitForIdleTaskQueue();
  }
  ASSERT_EQ(2, network_.GetWebFeedListContentsCount());
}

TEST_F(FeedApiTest, WebFeedContentExprirationWithNoSubscriptions) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  // Scopes are to control the lifetime of the surface object.
  {
    TestWebFeedSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    // The initial fetch should work fine.
    ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  }

  // Don't prepare a fetch response to simulate fetch failure.

  // Make the content a bit less than the expired threshold, and make sure we
  // load the stale, but expired content.
  task_environment_.FastForwardBy(base::Days(13));
  {
    TestWebFeedSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  }

  // Make the content expired, and make sure we don't load it.
  task_environment_.FastForwardBy(base::Days(2));
  {
    TestWebFeedSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
  }
}

// Test that we use QueryInteractiveFeedDiscoverApi and QueryNextPageDiscoverApi
// when kDiscoFeedEndpoint is enabled.
TEST_F(FeedApiTest, LoadFromNetworkDiscoFeedEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kDiscoFeedEndpoint);
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalNextPageState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetApiRequestCount<QueryInteractiveFeedDiscoverApi>());

  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetApiRequestCount<QueryNextPageDiscoverApi>());
  EXPECT_EQ("loading -> [user@foo] 2 slices -> 2 slices +spinner -> 4 slices",
            surface.DescribeUpdates());
}

TEST_P(FeedNetworkEndpointTest, TestAllNetworkEndpointConfigs) {
  SetUseFeedQueryRequests(GetUseFeedQueryRequests());

  // Subscribe to a page, so that we can check if the WebFeed is refreshed by
  // ForceRefreshForDebugging.
  base::test::ScopedFeatureList features;
  if (GetDiscoFeedEnabled()) {
    features.InitAndEnableFeature(kDiscoFeedEndpoint);
  } else {
    features.InitAndDisableFeature(kDiscoFeedEndpoint);
  }

  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Force a refresh that results in a successful load of both feed types.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("[user@foo] 2 slices", surface.DescribeState());
  EXPECT_EQ("[user@foo] 2 slices", web_feed_surface.DescribeState());

  // Total 2 queries (Web + For You).
  EXPECT_EQ(2, network_.send_query_call_count);
  // API request to DiscoFeed (For You) - if enabled by feature
  EXPECT_EQ((!GetUseFeedQueryRequests() && GetDiscoFeedEnabled()) ? 1 : 0,
            network_.GetApiRequestCount<QueryInteractiveFeedDiscoverApi>());
  // API request to WebFeedList if FeedQuery not enabled by config.
  EXPECT_EQ(GetUseFeedQueryRequests() ? 0 : 1,
            network_.GetApiRequestCount<WebFeedListContentsDiscoverApi>());
}

// Perform a background refresh when DiscoFeedEndpoint is enabled. A
// QueryBackgroundFeedDiscoverApi request should be made.
TEST_F(FeedApiTest, BackgroundRefreshDiscoFeedEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kDiscoFeedEndpoint);

  // Trigger a background refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify the refresh happened and that we can load a stream without the
  // network.
  ASSERT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(1, network_.GetApiRequestCount<QueryBackgroundFeedDiscoverApi>());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->background_refresh_status);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_P(FeedStreamTestForAllStreamTypes, ForceRefreshForDebugging) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  stream_->ForceRefreshForDebugging(GetStreamType());

  WaitForIdleTaskQueue();

  is_offline_ = true;

  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("[user@foo] 2 slices", surface.DescribeState());
}

TEST_F(FeedApiTest, RefreshScheduleFlow) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::Seconds(12), base::Seconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));

    // Load the stream, and then destroy the surface to allow background
    // refresh.
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    UnloadModel(surface.GetStreamType());
  }

  // Verify the first refresh was scheduled.
  EXPECT_EQ(base::Seconds(12),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Simulate executing the background task.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::Seconds(12));
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(base::Seconds(48 - 12),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Simulate executing the background task again.
  refresh_scheduler_.Clear();
  task_environment_.AdvanceClock(base::Seconds(48 - 12));
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // Verify |RefreshTaskComplete()| was called and next refresh was scheduled.
  EXPECT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(GetFeedConfig().default_background_refresh_interval,
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedApiTest, ForceRefreshIfMissedScheduledRefresh) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::Seconds(12), base::Seconds(48)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;

    response_translator_.InjectResponse(std::move(response_data));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  surface.Detach();
  stream_->UnloadModel(surface.GetStreamType());

  // Ensure a refresh is foreced only after a scheduled refresh was missed.
  // First, load the stream after 11 seconds.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  task_environment_.AdvanceClock(base::Seconds(11));
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);  // no refresh yet

  // Load the stream after 13 seconds. We missed the scheduled refresh at
  // 12 seconds.
  surface.Detach();
  stream_->UnloadModel(surface.GetStreamType());
  task_environment_.AdvanceClock(base::Seconds(2));
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(LoadStreamStatus::kDataInStoreStaleMissedLastRefresh,
            metrics_reporter_->load_stream_from_store_status);
}

TEST_F(FeedApiTest, LoadFromNetworkBecauseStoreIsStale_NetworkStaleAge) {
  base::TimeDelta default_staleness_threshold =
      GetFeedConfig().GetStalenessThreshold(StreamType(StreamKind::kForYou),
                                            /*is_web_feed_subscriber=*/true);
  base::TimeDelta server_staleness_threshold = default_staleness_threshold / 2;

  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime;
    content_lifetime.set_stale_age_ms(
        server_staleness_threshold.InMilliseconds());
    injected_response.content_lifetime = std::move(content_lifetime);
    response_translator_.InjectResponse(std::move(injected_response));

    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_TRUE(response_translator_.InjectedResponseConsumed());
  }

  // Fast forward enough to pass the server stale age but not the default stale
  // age.
  task_environment_.FastForwardBy(server_staleness_threshold +
                                  base::Seconds(1));

  // Set up the response translator to be prepared for another request (which we
  // expect to happen).
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  ASSERT_FALSE(response_translator_.InjectedResponseConsumed());
  CreateStream();

  // Store is stale, so we should fallback to a network request.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
}

TEST_F(FeedApiTest, LoadFromNetworkBecauseStoreIsExpired_NetworkExpiredAge) {
  base::HistogramTester histograms;

  base::TimeDelta default_content_expiration_threshold =
      GetFeedConfig().content_expiration_threshold;
  base::TimeDelta server_content_expiration_threshold =
      default_content_expiration_threshold / 2;

  {
    RefreshResponseData injected_response;
    injected_response.model_update_request = MakeTypicalInitialModelState();
    feedstore::Metadata::StreamMetadata::ContentLifetime content_lifetime;
    content_lifetime.set_invalid_age_ms(
        server_content_expiration_threshold.InMilliseconds());
    injected_response.content_lifetime = std::move(content_lifetime);
    response_translator_.InjectResponse(std::move(injected_response));

    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_TRUE(response_translator_.InjectedResponseConsumed());
  }

  base::TimeDelta content_age =
      server_content_expiration_threshold + base::Seconds(1);

  // Fast forward enough to pass the server expiration age but not the default
  // expiration age.
  task_environment_.FastForwardBy(content_age);

  // Set up the response translator to be prepared for another request (which we
  // expect to happen).
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  ASSERT_FALSE(response_translator_.InjectedResponseConsumed());
  CreateStream();

  // Store is stale, so we should fallback to a network request.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);

  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", content_age,
      1);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFromNetworkBecauseStoreIsStale) {
  // Fill the store with stream data that is just barely stale, and verify we
  // fetch new data over the network.
  store_->OverwriteStream(
      GetStreamType(),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch -
              GetFeedConfig().GetStalenessThreshold(
                  GetStreamType(), /*is_web_feed_subscriber=*/true) -
              base::Minutes(1)),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
}

// Same as LoadFromNetworkBecauseStoreIsStale, but with expired content.
TEST_F(FeedApiTest, LoadFromNetworkBecauseStoreIsExpired) {
  base::HistogramTester histograms;
  const base::TimeDelta kContentAge =
      GetFeedConfig().content_expiration_threshold + base::Minutes(1);
  store_->OverwriteStream(
      StreamType(StreamKind::kForYou),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch - kContentAge),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_TRUE(surface.initial_state);
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.BlockingRefresh", kContentAge,
      1);
}

TEST_F(FeedApiTest, LoadStaleDataBecauseNetworkRequestFails) {
  // Fill the store with stream data that is just barely stale.
  base::HistogramTester histograms;
  const base::TimeDelta kContentAge =
      GetFeedConfig().stale_content_threshold + base::Minutes(1);
  store_->OverwriteStream(
      StreamType(StreamKind::kForYou),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch - kContentAge),
      base::DoNothing());

  // Store is stale, so we should fallback to a network request. Since we didn't
  // inject a network response, the network update will fail.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsStale,
            metrics_reporter_->load_stream_from_store_status);
  EXPECT_EQ(LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure,
            metrics_reporter_->load_stream_status);
  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ContentAgeOnLoad.NotRefreshed", kContentAge, 1);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadFailsStoredDataIsExpired) {
  // Fill the store with stream data that is just barely expired.
  store_->OverwriteStream(
      GetStreamType(),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0,
          kTestTimeEpoch - GetFeedConfig().content_expiration_threshold -
              base::Minutes(1)),
      base::DoNothing());

  // Store contains expired content, so we should fallback to a network request.
  // Since we didn't inject a network response, the network update will fail.
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(network_.query_request_sent);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kDataInStoreIsExpired,
            metrics_reporter_->load_stream_from_store_status);
  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
}

TEST_F(FeedApiTest, LoadFromNetworkFailsDueToProtoTranslation) {
  // No data in the store, so we should fetch from the network.
  // The network will respond with an empty response, which should fail proto
  // translation.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
}
TEST_F(FeedApiTest, DoNotLoadFromNetworkWhenOffline) {
  is_offline_ = true;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kCannotLoadFromNetworkOffline,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DoNotLoadStreamWhenArticleListIsHidden) {
  profile_prefs_.SetBoolean(prefs::kArticlesListVisible, false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedArticlesListHidden,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DoNotLoadStreamWhenEulaIsNotAccepted) {
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(LoadStreamStatus::kLoadNotAllowedEulaNotAccepted,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ("no-cards", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, LoadStreamAfterEulaIsAccepted) {
  // Connect a surface before the EULA is accepted.
  is_eula_accepted_ = false;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("no-cards", surface.DescribeUpdates());

  // Accept EULA, our surface should receive data.
  is_eula_accepted_ = true;
  stream_->OnEulaAccepted();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, WebFeedUsesSignedInRequestAfterHistoryIsDeleted) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  stream_->OnAllHistoryDeleted();

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_NE(AccountInfo{}, network_.last_account_info);
}

TEST_F(FeedApiTest, ShouldMakeFeedQueryRequestConsumesQuota) {
  LoadStreamStatus status = LoadStreamStatus::kNoStatus;
  for (; status == LoadStreamStatus::kNoStatus;
       status = stream_
                    ->ShouldMakeFeedQueryRequest(
                        StreamType(StreamKind::kForYou), LoadType::kInitialLoad)
                    .load_stream_status) {
  }

  ASSERT_EQ(LoadStreamStatus::kCannotLoadFromNetworkThrottled, status);
}

TEST_F(FeedApiTest, LoadStreamFromStore) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  // Fill the store with stream data that is just barely fresh, and verify it
  // loads.
  store_->OverwriteStream(
      StreamType(StreamKind::kForYou),
      MakeTypicalInitialModelState(
          /*first_cluster_id=*/0, kTestTimeEpoch -
                                      GetFeedConfig().stale_content_threshold +
                                      base::Minutes(1)),
      base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_FALSE(network_.query_request_sent);
  // Verify the model is filled correctly.
  EXPECT_STRINGS_EQUAL(ModelStateFor(MakeTypicalInitialModelState()),
                       stream_->GetModel(StreamType(StreamKind::kForYou))
                           ->DumpStateForTesting());
}

TEST_F(FeedApiTest, LoadStreamFromStoreValidatesUser) {
  // Fill the store with stream data for another user.
  {
    auto state = MakeTypicalInitialModelState();
    state->stream_data.set_email("other@gmail.com");
    store_->OverwriteStream(StreamType(StreamKind::kForYou), std::move(state),
                            base::DoNothing());
  }

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> cant-refresh", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, LoadingSpinnerIsSentInitially) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  TestForYouSurface surface(stream_.get());

  ASSERT_EQ("loading", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, DetachSurfaceWhileLoadingModel) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  surface.Detach();
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading", surface.DescribeUpdates());
  EXPECT_TRUE(network_.query_request_sent);
}

TEST_F(FeedApiTest, AttachMultipleSurfacesLoadsModelOnce) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestForYouSurface other_surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  ASSERT_EQ("loading -> [user@foo] 2 slices", other_surface.DescribeUpdates());

  // After load, another surface doesn't trigger any tasks,
  // and immediately has content.
  TestForYouSurface later_surface(stream_.get());

  ASSERT_EQ("[user@foo] 2 slices", later_surface.DescribeUpdates());
  EXPECT_TRUE(IsTaskQueueIdle());
}

TEST_P(FeedStreamTestForAllStreamTypes, ModelChangesAreSavedToStorage) {
  store_->OverwriteStream(GetStreamType(), MakeTypicalInitialModelState(),
                          base::DoNothing());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #1, add #2.
  const std::vector<feedstore::DataOperation> operations = {
      MakeOperation(MakeRemove(MakeClusterId(1))),
      MakeOperation(MakeCluster(2, MakeRootId())),
      MakeOperation(MakeContentNode(2, MakeClusterId(2))),
      MakeOperation(MakeContent(2)),
  };
  stream_->ExecuteOperations(surface.GetSurfaceId(), operations);

  WaitForIdleTaskQueue();

  // Verify changes are applied to storage.
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations),
      ModelStateFor(GetStreamType(), store_.get()));

  // Unload and reload the model from the store, and verify we can still apply
  // operations correctly.
  surface.Detach();
  surface.Clear();
  UnloadModel(GetStreamType());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(surface.initial_state);

  // Remove #2, add #3.
  const std::vector<feedstore::DataOperation> operations2 = {
      MakeOperation(MakeRemove(MakeClusterId(2))),
      MakeOperation(MakeCluster(3, MakeRootId())),
      MakeOperation(MakeContentNode(3, MakeClusterId(3))),
      MakeOperation(MakeContent(3)),
  };
  stream_->ExecuteOperations(surface.GetSurfaceId(), operations2);

  WaitForIdleTaskQueue();
  EXPECT_STRINGS_EQUAL(
      ModelStateFor(MakeTypicalInitialModelState(), operations, operations2),
      ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedApiTest, ReportActionAfterSurfaceDestroyed) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  SurfaceId surface_id;
  {
    TestForYouSurface surface(stream_.get());
    surface_id = surface.GetSurfaceId();
    WaitForIdleTaskQueue();
  }

  base::HistogramTester histograms;
  stream_->ReportOtherUserAction(surface_id, FeedUserActionType::kClosedDialog);

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserActions",
      static_cast<int>(FeedUserActionType::kClosedDialog), 1, FROM_HERE);
}

TEST_F(FeedApiTest, ReportActionAfterSurfaceDestroyedAndCleanedUp) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  SurfaceId surface_id;
  {
    TestForYouSurface surface(stream_.get());
    surface_id = surface.GetSurfaceId();
    WaitForIdleTaskQueue();
  }

  // Trigger cleanup of internal FeedStreamSurface.
  task_environment_.FastForwardBy(base::Hours(2));
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Because the surface was removed, ReportOtherUserAction() ignored.
  base::HistogramTester histograms;
  stream_->ReportOtherUserAction(surface_id, FeedUserActionType::kClosedDialog);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.UserActions",
      static_cast<int>(FeedUserActionType::kClosedDialog), 0, FROM_HERE);
}

TEST_F(FeedApiTest, ReportSliceViewedIdentifiesCorrectIndex) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ReportSliceViewed(
      surface.GetSurfaceId(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  EXPECT_EQ(1, metrics_reporter_->slice_viewed_index);
}

TEST_F(FeedApiTest, ReportSliceViewed_AddViewedContentHashes) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->ReportSliceViewed(
      surface.GetSurfaceId(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  const feedstore::Metadata::StreamMetadata* stream_metadata =
      feedstore::FindMetadataForStream(stream_->GetMetadata(),
                                       StreamType(StreamKind::kForYou));
  EXPECT_EQ(1, stream_metadata->viewed_content_hashes().size());

  stream_->ReportSliceViewed(
      surface.GetSurfaceId(),
      surface.initial_state->updated_slices(0).slice().slice_id());
  stream_metadata = feedstore::FindMetadataForStream(
      stream_->GetMetadata(), StreamType(StreamKind::kForYou));
  EXPECT_EQ(2, stream_metadata->viewed_content_hashes().size());

  // Reporting the slice viewed before will not be counted again.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  stream_metadata = feedstore::FindMetadataForStream(
      stream_->GetMetadata(), StreamType(StreamKind::kForYou));
  EXPECT_EQ(2, stream_metadata->viewed_content_hashes().size());
}

TEST_F(FeedApiTest, ReportOpenInNewTabAction) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  base::UserActionTester user_actions;

  stream_->ReportOpenAction(
      GURL(), surface.GetSurfaceId(),
      surface.initial_state->updated_slices(1).slice().slice_id(),
      OpenActionType::kNewTab);

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTab"));
}

TEST_F(FeedApiTest, ReportOpenInNewTabInGroupAction) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  base::UserActionTester user_actions;

  stream_->ReportOpenAction(
      GURL(), surface.GetSurfaceId(),
      surface.initial_state->updated_slices(1).slice().slice_id(),
      OpenActionType::kNewTabInGroup);

  EXPECT_EQ(1, user_actions.GetActionCount(
                   "ContentSuggestions.Feed.CardAction.OpenInNewTabInGroup"));
}

TEST_F(FeedApiTest, HasUnreadContentAfterLoadFromNetwork) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);
  TestForYouSurface surface(stream_.get());

  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false, true}), observer.calls);
}

TEST_F(FeedApiTest, HasUnreadContentInitially) {
  // Prime the feed with new content.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
  }

  // Reload FeedStream. Add an observer before initialization completes.
  // After initialization, the observer will be informed about unread content.
  CreateStream(/*wait_for_initialization*/ false);
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({true}), observer.calls);
}

TEST_F(FeedApiTest, NetworkFetchWithNoNewContentDoesNotProvideUnreadContent) {
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);
  // Load content from the network, and view it.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();

    stream_->ReportFeedViewed(surface.GetSurfaceId());
    stream_->ReportSliceViewed(
        surface.GetSurfaceId(),
        surface.initial_state->updated_slices(1).slice().slice_id());
  }
  // Wait until the feed content is stale.

  task_environment_.FastForwardBy(base::Hours(100));

  // Load content from the network again. This time there is no new content.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false, true, false}), observer.calls);
}

TEST_F(FeedApiTest, RemovedUnreadContentObserverDoesNotReceiveCalls) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);
  stream_->RemoveUnreadContentObserver(StreamType(StreamKind::kForYou),
                                       &observer);
  TestForYouSurface surface(stream_.get());

  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false}), observer.calls);
}

TEST_F(FeedApiTest, DeletedUnreadContentObserverDoesNotCrash) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  {
    TestUnreadContentObserver observer;
    stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou),
                                      &observer);
  }
  TestForYouSurface surface(stream_.get());

  WaitForIdleTaskQueue();
}

TEST_F(FeedApiTest, HasUnreadContentAfterLoadFromStore) {
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());
  TestForYouSurface surface(stream_.get());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);

  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({true}), observer.calls);
}

TEST_F(FeedApiTest, FollowForcesRefreshWhileSurfaceAttached_NotWorking) {
  // Load the web feed stream.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Follow a web feed.
  network_.InjectResponse(SuccessfulFollowResponse("dogs"));
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("http://dogs.com");
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  stream_->subscriptions().FollowWebFeed(
      page_info, WebFeedChangeReason::WEB_PAGE_MENU, callback.Bind());

  ASSERT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);

  // Content should refresh, but this is not implemented yet.
  // ASSERT_EQ("loading -> 3 slices", surface.DescribeUpdates());
  ASSERT_EQ("", surface.DescribeUpdates());
}

// Verify that following a web feed triggers a refresh.
// Also verify the unread content observer events.
TEST_F(FeedApiTest, FollowForcesRefresh) {
  TestUnreadContentObserver unread_observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kFollowing),
                                    &unread_observer);

  // Load the web feed stream and view it.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  stream_->ReportFeedViewed(surface.GetSurfaceId());

  // Detach the surface.
  surface.Detach();

  // Follow a web feed.
  network_.InjectResponse(SuccessfulFollowResponse("dogs"));
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("http://dogs.com");
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  stream_->subscriptions().FollowWebFeed(
      page_info, WebFeedChangeReason::WEB_PAGE_MENU, callback.Bind());

  ASSERT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);

  // Wait for model to unload and reattach surface. New content is loaded.
  WaitForModelToAutoUnload();
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> 3 slices", surface.DescribeUpdates());
  EXPECT_EQ(std::vector<bool>({false, true, false, true}),
            unread_observer.calls);
}

TEST_F(FeedApiTest, ReportFeedViewedUpdatesObservers) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);
  WaitForIdleTaskQueue();

  stream_->ReportFeedViewed(surface.GetSurfaceId());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(std::vector<bool>({true, false}), observer.calls);

  // Verify that the fact the stream was viewed persists.
  CreateStream();

  TestUnreadContentObserver observer2;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou),
                                    &observer2);
  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::vector<bool>({false}), observer2.calls);
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMoreIndicatorSliceId) {
  // The load-more spinner's slice ID must change for each load.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  size_t num_of_updates = surface.all_updates.size();
  size_t num_of_cards = surface.update->updated_slices().size();

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ("load-more-spinner1", surface.all_updates[num_of_updates]
                                      .updated_slices(num_of_cards)
                                      .slice()
                                      .slice_id());
  num_of_updates = surface.all_updates.size();
  num_of_cards = surface.update->updated_slices().size();

  // Load page 3.
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ("load-more-spinner2", surface.all_updates[num_of_updates]
                                      .updated_slices(num_of_cards)
                                      .slice()
                                      .slice_id());
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMoreAppendsContent) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());
  // Ensure metrics reporter was informed at the start of the operation.
  EXPECT_EQ(surface.GetSurfaceId(), metrics_reporter_->load_more_surface_id);
  WaitForIdleTaskQueue();
  ASSERT_EQ(std::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());

  // Load page 3.
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(std::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
  // The root ID should not change for next-page content.
  EXPECT_EQ(MakeRootEventId(),
            surface.update->logging_parameters().root_event_id());
}

TEST_P(FeedStreamTestForAllStreamTypes, LoadMorePersistsData) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(std::optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(GetStreamType())->DumpStateForTesting(),
      ModelStateFor(GetStreamType(), store_.get()));
}

TEST_F(FeedApiTest, LoadMorePersistAndLoadMore) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  // Verify we can persist a LoadMore, and then do another LoadMore after
  // reloading state.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Load page 2.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  ASSERT_EQ(std::optional<bool>(true), callback.GetResult());

  surface.Detach();
  UnloadModel(StreamType(StreamKind::kForYou));

  // Load page 3.
  surface.Attach(stream_.get());
  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  WaitForIdleTaskQueue();
  callback.Clear();
  surface.Clear();
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_EQ(std::optional<bool>(true), callback.GetResult());
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(StreamType(StreamKind::kForYou), store_.get()));
}

TEST_F(FeedApiTest, LoadMoreSendsTokens) {
  {
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("2 slices +spinner -> 4 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-2", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());

  response_translator_.InjectResponse(MakeTypicalNextPageState(3));
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ("4 slices +spinner -> 6 slices", surface.DescribeUpdates());

  EXPECT_EQ(
      "token",
      network_.query_request_sent->feed_request().consistency_token().token());
  EXPECT_EQ("page-3", network_.query_request_sent->feed_request()
                          .feed_query()
                          .next_page_token()
                          .next_page_token()
                          .next_page_token());
}

TEST_F(FeedApiTest, LoadMoreAbortsIfNoNextPageToken) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  {
    std::unique_ptr<StreamModelUpdateRequest> initial_state =
        MakeTypicalInitialModelState();
    initial_state->stream_data.clear_next_page_token();
    response_translator_.InjectResponse(std::move(initial_state));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();

  // LoadMore fails, and does not make an additional request.
  EXPECT_EQ(std::optional<bool>(false), callback.GetResult());
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(std::nullopt, metrics_reporter_->load_more_surface_id)
      << "metrics reporter was informed about a load more operation which "
         "didn't begin";
}

TEST_F(FeedApiTest, LoadMoreFail) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Don't inject another response, which results in a proto translation
  // failure.
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();

  EXPECT_EQ(std::optional<bool>(false), callback.GetResult());
  EXPECT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, LoadMoreWithClearAllInResponse) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Use a different initial state (which includes a CLEAR_ALL).
  response_translator_.InjectResponse(MakeTypicalInitialModelState(5));
  CallbackReceiver<bool> callback;
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());

  WaitForIdleTaskQueue();
  ASSERT_EQ(std::optional<bool>(true), callback.GetResult());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(StreamType(StreamKind::kForYou), store_.get()));

  // Verify the new state has been pushed to |surface|.
  ASSERT_EQ("2 slices +spinner -> 2 slices", surface.DescribeUpdates());

  const feedui::StreamUpdate& initial_state = surface.update.value();
  ASSERT_EQ(2, initial_state.updated_slices().size());
  EXPECT_NE("", initial_state.updated_slices(0).slice().slice_id());
  EXPECT_EQ("f:5", initial_state.updated_slices(0)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
  EXPECT_NE("", initial_state.updated_slices(1).slice().slice_id());
  EXPECT_EQ("f:6", initial_state.updated_slices(1)
                       .slice()
                       .xsurface_slice()
                       .xsurface_frame());
}

TEST_F(FeedApiTest, LoadMoreBeforeLoad) {
  CallbackReceiver<bool> callback;
  TestForYouSurface surface;
  surface.CreateWithoutAttach(stream_.get());
  stream_->LoadMore(surface.GetSurfaceId(), callback.Bind());

  EXPECT_EQ(std::optional<bool>(false), callback.GetResult());
}

TEST_F(FeedApiTest, ReadNetworkResponse) {
  base::HistogramTester histograms;
  network_.InjectRealFeedQueryResponse();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 10 slices", surface.DescribeUpdates());

  // Verify we're processing some of the data on the request.

  // The response has a privacy_notice_fulfilled=true.
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ActivityLoggingEnabled", 1, 1);

  // A request schedule with two entries was in the response. The first entry
  // should have already been scheduled/consumed, leaving only the second
  // entry still in the the refresh_offsets vector.
  RequestSchedule schedule = prefs::GetRequestSchedule(
      RefreshTaskId::kRefreshForYouFeed, profile_prefs_);
  EXPECT_EQ(std::vector<base::TimeDelta>({
                base::Seconds(86308) + base::Nanoseconds(822963644),
                base::Seconds(120000),
            }),
            schedule.refresh_offsets);

  // The stream's user attributes are set, so activity logging is enabled.
  EXPECT_TRUE(surface.update->logging_parameters().logging_enabled());
  // This network response has content.
  EXPECT_TRUE(stream_->HasUnreadContent(StreamType(StreamKind::kForYou)));
}

TEST_F(FeedApiTest, ReadNetworkResponseWithNoContent) {
  base::HistogramTester histograms;
  network_.InjectRealFeedQueryResponseWithNoContent();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> loading -> no-cards", surface.DescribeUpdates());

  // This network response has no content.
  EXPECT_FALSE(stream_->HasUnreadContent(StreamType(StreamKind::kForYou)));
}

TEST_F(FeedApiTest, ClearAllAfterLoadResultsInRefresh) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->OnCacheDataCleared();  // triggers ClearAll().

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices -> loading -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ClearAllWithNoSurfacesAttachedDoesNotReload) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ClearAllWhileLoadingMoreDoesNotLoadMore) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  CallbackReceiver<bool> cr;
  stream_->LoadMore(surface.GetSurfaceId(), cr.Bind());
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ(false, cr.GetResult());
  EXPECT_EQ(
      "loading -> [user@foo] 2 slices -> 2 slices +spinner -> 2 slices -> "
      "loading -> 2 slices",
      surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ClearAllWipesAllState) {
  // Trigger saving a consistency token, so it can be cleared later.
  network_.consistency_token = "token-11";
  stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  // Trigger saving a feed stream, so it can be cleared later.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Enqueue an action, so it can be cleared later.
  stream_->UploadAction(MakeFeedAction(43ul), CreateLoggingParameters(), false,
                        base::DoNothing());

  // Trigger ClearAll, this should erase everything.
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices -> loading -> cant-refresh",
            surface.DescribeUpdates());
  EXPECT_EQ(R"("m": {
}
"recommendedIndex": {
}
"subs": {
}
)",
            DumpStoreState(true));
  EXPECT_EQ("", stream_->GetMetadata().consistency_token());
  EXPECT_FALSE(surface.update->logging_parameters().logging_enabled());
}

TEST_F(FeedApiTest, StorePendingAction) {
  stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  WaitForIdleTaskQueue();

  std::vector<feedstore::StoredAction> result =
      ReadStoredActions(stream_->GetStore());
  ASSERT_EQ(1ul, result.size());

  EXPECT_EQ(ToTextProto(MakeFeedAction(42ul).action_payload()),
            ToTextProto(result[0].action().action_payload()));
}

TEST_F(FeedApiTest, UploadActionWhileSignedOutIsNoOp) {
  account_info_ = {};
  ASSERT_EQ(stream_->GetAccountInfo(), AccountInfo{});
  stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(0ul, ReadStoredActions(stream_->GetStore()).size());
}

TEST_F(FeedApiTest, SignOutWhileUploadActionDoesNotUpload) {
  stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  account_info_ = {};

  WaitForIdleTaskQueue();

  EXPECT_EQ(UploadActionsStatus::kAbortUploadForSignedOutUser,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, ClearAllWhileUploadActionDoesNotUpload) {
  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), true,
                        cr.Bind());
  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  EXPECT_EQ(UploadActionsStatus::kAbortUploadActionsWithPendingClearAll,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(0ul, cr.GetResult()->upload_attempt_count);
}

TEST_F(FeedApiTest, WrongUserUploadActionDoesNotUpload) {
  CallbackReceiver<UploadActionsTask::Result> cr;
  LoggingParameters logging_parameters = CreateLoggingParameters();
  logging_parameters.email = "someothergaia";
  stream_->UploadAction(MakeFeedAction(42ul), logging_parameters, true,
                        cr.Bind());

  WaitForIdleTaskQueue();

  // Action should not upload.
  EXPECT_EQ(UploadActionsStatus::kAbortUploadForWrongUser,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(0ul, cr.GetResult()->upload_attempt_count);
}

TEST_F(FeedApiTest, LoggingPropertiesWithNoAccountDoesNotUpload) {
  CallbackReceiver<UploadActionsTask::Result> cr;
  LoggingParameters logging_parameters = CreateLoggingParameters();
  logging_parameters.email.clear();
  stream_->UploadAction(MakeFeedAction(42ul), logging_parameters, true,
                        cr.Bind());

  WaitForIdleTaskQueue();

  // Action should not upload.
  EXPECT_EQ(UploadActionsStatus::kAbortUploadForSignedOutUser,
            metrics_reporter_->upload_action_status);
  EXPECT_EQ(0, network_.GetActionRequestCount());
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(0ul, cr.GetResult()->upload_attempt_count);
}

TEST_F(FeedApiTest, StorePendingActionAndUploadNow) {
  network_.consistency_token = "token-11";

  // Call |ProcessThereAndBackAgain()|, which triggers Upload() with
  // upload_now=true.
  {
    feedwire::ThereAndBackAgainData msg;
    *msg.mutable_action_payload() = MakeFeedAction(42ul).action_payload();
    stream_->ProcessThereAndBackAgain(msg.SerializeAsString(),
                                      CreateLoggingParameters());
  }
  WaitForIdleTaskQueue();

  // Verify the action was uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
  std::vector<feedstore::StoredAction> result =
      ReadStoredActions(stream_->GetStore());
  ASSERT_EQ(0ul, result.size());
}

TEST_F(FeedApiTest, ProcessViewActionResultsInDelayedUpload) {
  network_.consistency_token = "token-11";

  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString(),
                             CreateLoggingParameters());
  WaitForIdleTaskQueue();
  // Verify it's not uploaded immediately.
  ASSERT_EQ(0, network_.GetActionRequestCount());

  // Trigger a network refresh.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the action was uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, ProcessViewActionDroppedBecauseNotEnabled) {
  network_.consistency_token = "token-11";
  LoggingParameters logging_parameters = CreateLoggingParameters();
  logging_parameters.view_actions_enabled = false;
  stream_->ProcessViewAction(MakeFeedAction(42ul).SerializeAsString(),
                             logging_parameters);
  WaitForIdleTaskQueue();
  // Verify it's not uploaded, and not stored.
  ASSERT_EQ(0, network_.GetActionRequestCount());
  ASSERT_EQ(0ull, ReadStoredActions(stream_->GetStore()).size());
}

TEST_F(FeedApiTest, ActionsUploadWithoutConditionsWhenFeatureDisabled) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  stream_->ProcessViewAction(
      feedwire::FeedAction::default_instance().SerializeAsString(),
      surface.GetLoggingParameters());
  WaitForIdleTaskQueue();
  stream_->ProcessThereAndBackAgain(
      MakeThereAndBackAgainData(42ul).SerializeAsString(),
      surface.GetLoggingParameters());
  WaitForIdleTaskQueue();

  // Verify the actions were uploaded.
  ASSERT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(2, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, LoadStreamFromNetworkUploadsActions) {
  stream_->UploadAction(MakeFeedAction(99ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  WaitForIdleTaskQueue();

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());

  // Uploaded action should have been erased from store.
  stream_->UploadAction(MakeFeedAction(100ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(2, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, UploadedActionsHaveSequentialNumbers) {
  // Send 3 actions.
  stream_->UploadAction(MakeFeedAction(1ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  stream_->UploadAction(MakeFeedAction(2ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  stream_->UploadAction(MakeFeedAction(3ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetActionRequestCount());
  feedwire::UploadActionsRequest request1 = *network_.GetActionRequestSent();

  // Send another action in a new request.
  stream_->UploadAction(MakeFeedAction(4ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.GetActionRequestCount());
  feedwire::UploadActionsRequest request2 = *network_.GetActionRequestSent();

  // Verify that sent actions have sequential numbers.
  ASSERT_EQ(3, request1.feed_actions_size());
  ASSERT_EQ(1, request2.feed_actions_size());

  EXPECT_EQ(1, request1.feed_actions(0).client_data().sequence_number());
  EXPECT_EQ(2, request1.feed_actions(1).client_data().sequence_number());
  EXPECT_EQ(3, request1.feed_actions(2).client_data().sequence_number());
  EXPECT_EQ(4, request2.feed_actions(0).client_data().sequence_number());
}

TEST_F(FeedApiTest, LoadMoreUploadsActions) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->UploadAction(MakeFeedAction(99ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  WaitForIdleTaskQueue();

  network_.consistency_token = "token-12";

  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ("token-12", stream_->GetMetadata().consistency_token());

  // Uploaded action should have been erased from the store.
  network_.ClearTestData();
  stream_->UploadAction(MakeFeedAction(100ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());

  EXPECT_EQ(
      ToTextProto(MakeFeedAction(100ul).action_payload()),
      ToTextProto(
          network_.GetActionRequestSent()->feed_actions(0).action_payload()));
}

TEST_F(FeedApiTest, LoadMoreDoesNotUpdateLoggingEnabled) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(surface.update->logging_parameters().logging_enabled());

  int page = 2;
  // A NextPage request will not work when signed-out.
  const bool signed_in = true;

  // Logging parameters are not updated on LoadMore(), so logging remains
  // enabled until the next refresh.
  for (bool waa_on : {true, false}) {
    for (bool privacy_notice_fulfilled : {true, false}) {
      response_translator_.InjectResponse(MakeTypicalNextPageState(
          page++, kTestTimeEpoch, signed_in, waa_on, privacy_notice_fulfilled));
      stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
      WaitForIdleTaskQueue();
      EXPECT_TRUE(surface.update->logging_parameters().logging_enabled());
    }
  }
}

TEST_F(FeedApiTest, LoadStreamWithLoggingEnabled) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_TRUE(surface.update->logging_parameters().logging_enabled());
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, BackgroundingAppUploadsActions) {
  stream_->UploadAction(MakeFeedAction(1ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(
      ToTextProto(MakeFeedAction(1ul).action_payload()),
      ToTextProto(
          network_.GetActionRequestSent()->feed_actions(0).action_payload()));
}

TEST_F(FeedApiTest, BackgroundingAppDoesNotUploadActions) {
  Config config;
  config.upload_actions_on_enter_background = false;
  SetFeedConfigForTesting(config);

  stream_->UploadAction(MakeFeedAction(1ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(0, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, UploadedActionsAreNotSentAgain) {
  stream_->UploadAction(MakeFeedAction(1ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetActionRequestCount());

  stream_->OnEnterBackground();
  WaitForIdleTaskQueue();
  EXPECT_EQ(1, network_.GetActionRequestCount());
}

TEST_F(FeedApiTest, UploadActionsOneBatch) {
  UploadActions(
      {MakeFeedAction(97ul), MakeFeedAction(98ul), MakeFeedAction(99ul)});
  WaitForIdleTaskQueue();

  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(3, network_.GetActionRequestSent()->feed_actions_size());

  stream_->UploadAction(MakeFeedAction(99ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(2, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, UploadActionsMultipleBatches) {
  UploadActions({
      // Batch 1: One really big action.
      MakeFeedAction(100ul, /*pad_size=*/20001ul),

      // Batch 2
      MakeFeedAction(101ul, 10000ul),
      MakeFeedAction(102ul, 9000ul),

      // Batch 3. Trigger upload.
      MakeFeedAction(103ul, 2000ul),
  });
  WaitForIdleTaskQueue();

  EXPECT_EQ(3, network_.GetActionRequestCount());

  stream_->UploadAction(MakeFeedAction(99ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_EQ(4, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
}

TEST_F(FeedApiTest, UploadActionsSkipsStaleActionsByTimestamp) {
  stream_->UploadAction(MakeFeedAction(2ul), CreateLoggingParameters(), false,
                        base::DoNothing());
  WaitForIdleTaskQueue();
  task_environment_.FastForwardBy(base::Hours(25));

  // Trigger upload
  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(3ul), CreateLoggingParameters(), true,
                        cr.Bind());
  WaitForIdleTaskQueue();

  // Just one action should have been uploaded.
  EXPECT_EQ(1, network_.GetActionRequestCount());
  EXPECT_EQ(1, network_.GetActionRequestSent()->feed_actions_size());
  EXPECT_EQ(
      ToTextProto(MakeFeedAction(3ul).action_payload()),
      ToTextProto(
          network_.GetActionRequestSent()->feed_actions(0).action_payload()));

  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(1ul, cr.GetResult()->upload_attempt_count);
  EXPECT_EQ(1ul, cr.GetResult()->stale_count);
}

TEST_F(FeedApiTest, UploadActionsErasesStaleActionsByAttempts) {
  // Three failed uploads, plus one more to cause the first action to be erased.
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(0ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(1ul), CreateLoggingParameters(), true,
                        base::DoNothing());
  network_.InjectEmptyActionRequestResult();
  stream_->UploadAction(MakeFeedAction(2ul), CreateLoggingParameters(), true,
                        base::DoNothing());

  CallbackReceiver<UploadActionsTask::Result> cr;
  stream_->UploadAction(MakeFeedAction(3ul), CreateLoggingParameters(), true,
                        cr.Bind());
  WaitForIdleTaskQueue();

  // Four requests, three pending actions in the last request.
  EXPECT_EQ(4, network_.GetActionRequestCount());
  EXPECT_EQ(3, network_.GetActionRequestSent()->feed_actions_size());

  // Action 0 should have been erased.
  ASSERT_TRUE(cr.GetResult());
  EXPECT_EQ(3ul, cr.GetResult()->upload_attempt_count);
  EXPECT_EQ(1ul, cr.GetResult()->stale_count);
}

TEST_F(FeedApiTest, MetadataLoadedWhenDatabaseInitialized) {
  const auto kExpiry = kTestTimeEpoch + base::Days(1234);
  {
    // Write some metadata so it can be loaded when FeedStream starts up.
    feedstore::Metadata initial_metadata;
    feedstore::SetSessionId(initial_metadata, "session-id", kExpiry);
    initial_metadata.set_consistency_token("token");
    initial_metadata.set_gaia(GetAccountInfo().gaia);
    store_->WriteMetadata(initial_metadata, base::DoNothing());
  }

  // Creating a stream should load metadata.
  CreateStream();

  EXPECT_EQ("session-id", stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kExpiry,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));
  EXPECT_EQ("token", stream_->GetMetadata().consistency_token());
  // Verify the schema has been updated to the current version.
  EXPECT_EQ((int)FeedStore::kCurrentStreamSchemaVersion,
            stream_->GetMetadata().stream_schema_version());
}

TEST_F(FeedApiTest, ClearAllWhenDatabaseInitializedForWrongUser) {
  {
    // Write some metadata so it can be loaded when FeedStream starts up.
    feedstore::Metadata initial_metadata;
    initial_metadata.set_consistency_token("token");
    initial_metadata.set_gaia("someotherusergaia");
    store_->WriteMetadata(initial_metadata, base::DoNothing());
  }

  // Creating a stream should init database.
  CreateStream();

  EXPECT_EQ("{\n}\n", DumpStoreState());
  EXPECT_EQ("", stream_->GetMetadata().consistency_token());
}

TEST_F(FeedApiTest, ModelUnloadsAfterTimeout) {
  Config config;
  config.model_unload_timeout = base::Seconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::Milliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  task_environment_.FastForwardBy(base::Milliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedApiTest, ModelDoesNotUnloadIfSurfaceIsAttached) {
  Config config;
  config.model_unload_timeout = base::Seconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::Milliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  surface.Attach(stream_.get());

  task_environment_.FastForwardBy(base::Milliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedApiTest, ModelUnloadsAfterSecondTimeout) {
  Config config;
  config.model_unload_timeout = base::Seconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  surface.Detach();

  task_environment_.FastForwardBy(base::Milliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  // Attaching another surface will prolong the unload time for another second.
  surface.Attach(stream_.get());
  surface.Detach();

  task_environment_.FastForwardBy(base::Milliseconds(999));
  WaitForIdleTaskQueue();
  EXPECT_TRUE(stream_->GetModel(surface.GetStreamType()));

  task_environment_.FastForwardBy(base::Milliseconds(2));
  WaitForIdleTaskQueue();
  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
}

TEST_F(FeedApiTest, SendsClientInstanceId) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  // Store is empty, so we should fallback to a network request.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ(1, network_.send_query_call_count);
  ASSERT_TRUE(network_.query_request_sent);

  // Instance ID is a random token. Verify it is not empty.
  std::string first_instance_id = network_.query_request_sent->feed_request()
                                      .client_info()
                                      .client_instance_id();
  EXPECT_NE("", first_instance_id);

  // No signed-out session id was in the request.
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // LoadMore, and verify the same token is used.
  response_translator_.InjectResponse(MakeTypicalNextPageState(2));
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(first_instance_id, network_.query_request_sent->feed_request()
                                   .client_info()
                                   .client_instance_id());

  // No signed-out session id was in the request.
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .chrome_client_info()
                  .session_id()
                  .empty());

  // Trigger a ClearAll to verify the instance ID changes.
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();

  EXPECT_FALSE(stream_->GetModel(surface.GetStreamType()));
  const bool is_for_next_page = false;  // No model so no first page yet.
  const std::string new_instance_id =
      stream_
          ->GetRequestMetadata(StreamType(StreamKind::kForYou),
                               is_for_next_page)
          .client_instance_id;
  ASSERT_NE("", new_instance_id);
  ASSERT_NE(first_instance_id, new_instance_id);
}

TEST_F(FeedApiTest, SignedOutSessionIdConsistency) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  const std::string kSessionToken1("session-token-1");
  const std::string kSessionToken2("session-token-2");

  account_info_ = {};

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should not include a session-id
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken1);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .chrome_client_info()
                   .has_session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());
  const base::Time kSessionToken1ExpiryTime =
      feedstore::GetSessionIdExpiryTime(stream_->GetMetadata());

  // (2) LoadMore: the server returns the same session-id token
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the first session-id
  //     - the session-id's expiry time should be unchanged
  task_environment_.FastForwardBy(base::Seconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(2),
                                      kSessionToken1);
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));

  // (3) LoadMore: the server omits returning a session-id token
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the first session-id
  //     - the session-id's expiry time should be unchanged
  task_environment_.FastForwardBy(base::Seconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(3));
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));

  // (4) LoadMore: the server returns new session id.
  //     - this should trigger a network request
  //     - the request should not include client-instance-id
  //     - the request should include the first session-id
  //     - the stream should retain the second session-id
  //     - the new session-id's expiry time should be updated
  task_environment_.FastForwardBy(base::Seconds(1));
  response_translator_.InjectResponse(model_generator.MakeNextPage(4),
                                      kSessionToken2);
  stream_->LoadMore(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();
  ASSERT_EQ(4, network_.send_query_call_count);
  EXPECT_TRUE(network_.query_request_sent->feed_request()
                  .client_info()
                  .client_instance_id()
                  .empty());
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken2, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kSessionToken1ExpiryTime + base::Seconds(3),
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));
}

TEST_F(FeedApiTest, ClearAllResetsSessionId) {
  account_info_ = {};

  // Initialize a session id.
  feedstore::Metadata metadata = stream_->GetMetadata();
  feedstore::MaybeUpdateSessionId(metadata, "session-id");
  stream_->SetMetadata(metadata);

  // Trigger a ClearAll.
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  // Session-ID should be wiped.
  EXPECT_TRUE(stream_->GetMetadata().session_id().token().empty());
  EXPECT_TRUE(
      feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()).is_null());
}

TEST_F(FeedApiTest, SignedOutSessionIdExpiry) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  const std::string kSessionToken1("session-token-1");
  const std::string kSessionToken2("session-token-2");

  account_info_ = {};

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the request should not include a session-id
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken1);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .chrome_client_info()
                   .has_session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());

  // (2) Reload the stream from the network:
  //     - Detach the surface, advance the clock beyond the stale content
  //       threshold, re-attach the surface.
  //     - this should trigger a network request
  //     - the request should include kSessionToken1
  //     - the stream should retain the original session-id
  surface.Detach();
  task_environment_.FastForwardBy(GetFeedConfig().stale_content_threshold +
                                  base::Seconds(1));
  response_translator_.InjectResponse(model_generator.MakeFirstPage());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ(kSessionToken1, network_.query_request_sent->feed_request()
                                .client_info()
                                .chrome_client_info()
                                .session_id());
  EXPECT_EQ(kSessionToken1, stream_->GetMetadata().session_id().token());

  // (3) Reload the stream from the network:
  //     - Detach the surface, advance the clock beyond the session id max age
  //       threshold, re-attach the surface.
  //     - this should trigger a network request
  //     - the request should not include a session-id
  //     - the stream should get a new session-id
  surface.Detach();
  task_environment_.FastForwardBy(GetFeedConfig().session_id_max_age -
                                  GetFeedConfig().stale_content_threshold);
  ASSERT_LT(feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()),
            base::Time::Now());
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken2);
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(3, network_.send_query_call_count);
  EXPECT_FALSE(network_.query_request_sent->feed_request()
                   .client_info()
                   .chrome_client_info()
                   .has_session_id());
  EXPECT_EQ(kSessionToken2, stream_->GetMetadata().session_id().token());
}

TEST_F(FeedApiTest, SessionIdPersistsAcrossStreamLoads) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  const std::string kSessionToken("session-token-ftw");
  const base::Time kExpiryTime =
      kTestTimeEpoch + GetFeedConfig().session_id_max_age;

  StreamModelUpdateRequestGenerator model_generator;
  model_generator.signed_in = false;
  account_info_ = {};

  // (1) Do an initial load of the store
  //     - this should trigger a network request
  //     - the stream should capture the session-id token from the response
  response_translator_.InjectResponse(model_generator.MakeFirstPage(),
                                      kSessionToken);
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);

  // (2) Reload the metadata from disk.
  //     - the network query call count should be unchanged
  //     - the session token and expiry time should have been reloaded.
  surface.Detach();
  CreateStream();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ(kSessionToken, stream_->GetMetadata().session_id().token());
  EXPECT_TIME_EQ(kExpiryTime,
                 feedstore::GetSessionIdExpiryTime(stream_->GetMetadata()));
}

TEST_F(FeedApiTest, PersistentKeyValueStoreIsClearedOnClearAll) {
  // Store some data and verify it exists.
  PersistentKeyValueStore& store = stream_->GetPersistentKeyValueStore();
  store.Put("x", "y", base::DoNothing());
  CallbackReceiver<PersistentKeyValueStore::Result> get_result;
  store.Get("x", get_result.Bind());
  ASSERT_EQ("y", *get_result.RunAndGetResult().get_result);

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  // Verify ClearAll() deleted the data.
  get_result.Clear();
  store.Get("x", get_result.Bind());
  EXPECT_FALSE(get_result.RunAndGetResult().get_result);
}

TEST_F(FeedApiTest, LoadMultipleStreams) {
  // TODO(crbug.com/40869325) Add support for single web feed.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  TestForYouSurface for_you_surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());

  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices",
            for_you_surface.DescribeUpdates());
  ASSERT_EQ("loading -> [user@foo] 2 slices",
            web_feed_surface.DescribeUpdates());
}

TEST_F(FeedApiTest, UnloadOnlyOneOfMultipleModels) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  Config config;
  config.model_unload_timeout = base::Seconds(1);
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface for_you_surface(stream_.get());
  TestWebFeedSurface web_feed_surface(stream_.get());

  WaitForIdleTaskQueue();

  for_you_surface.Detach();

  WaitForModelToAutoUnload();
  WaitForIdleTaskQueue();

  EXPECT_TRUE(stream_->GetModel(StreamType(StreamKind::kFollowing)));
  EXPECT_FALSE(stream_->GetModel(StreamType(StreamKind::kForYou)));
}

TEST_F(FeedApiTest, ExperimentsAreClearedOnClearAll) {
  Experiments e;
  std::vector<ExperimentGroup> group_list1{{"Group1", 123}};
  std::vector<ExperimentGroup> group_list2{{"Group2", 9999}};
  e["Trial1"] = group_list1;
  e["Trial2"] = group_list2;
  prefs::SetExperiments(e, profile_prefs_);

  stream_->OnCacheDataCleared();  // triggers ClearAll().
  WaitForIdleTaskQueue();

  Experiments empty;
  Experiments got = prefs::GetExperiments(profile_prefs_);
  EXPECT_EQ(got, empty);
}

TEST_F(FeedApiTest, CreateAndCommitEphemeralChange) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EphemeralChangeId change_id = stream_->CreateEphemeralChange(
      surface.GetSurfaceId(), {MakeOperation(MakeClearAll())});
  stream_->CommitEphemeralChange(surface.GetSurfaceId(), change_id);
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices -> no-cards -> no-cards",
            surface.DescribeUpdates());
}

TEST_F(FeedApiTest, CreateAndCommitEphemeralChangeOnNoOperation) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EphemeralChangeId change_id =
      stream_->CreateEphemeralChange(surface.GetSurfaceId(), {});
  stream_->CommitEphemeralChange(surface.GetSurfaceId(), change_id);
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices -> 2 slices -> 2 slices",
            surface.DescribeUpdates());
}

TEST_F(FeedApiTest, RejectEphemeralChange) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EphemeralChangeId change_id = stream_->CreateEphemeralChange(
      surface.GetSurfaceId(), {MakeOperation(MakeClearAll())});
  stream_->RejectEphemeralChange(surface.GetSurfaceId(), change_id);
  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices -> no-cards -> 2 slices",
            surface.DescribeUpdates());
}

// Test that we overwrite stored stream data, even if ContentId's do not change.
TEST_F(FeedApiTest, StreamDataOverwritesOldStream) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  // Inject two FeedQuery responses with some different data.
  {
    std::unique_ptr<StreamModelUpdateRequest> new_state =
        MakeTypicalInitialModelState();
    new_state->shared_states[0].set_shared_state_data("new-shared-data");
    new_state->content[0].set_frame("new-frame-data");

    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    response_translator_.InjectResponse(std::move(new_state));
  }

  // Trigger stream load, unload stream, and wait until the stream data is
  // stale.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();
  task_environment_.FastForwardBy(base::Days(20));

  // Trigger stream load again, it should refersh from the network.
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();

  // Verify the new data was stored.
  std::unique_ptr<StreamModelUpdateRequest> stored_data =
      StoredModelData(StreamType(StreamKind::kForYou), store_.get());
  ASSERT_TRUE(stored_data);
  EXPECT_EQ("new-shared-data",
            stored_data->shared_states[0].shared_state_data());
  EXPECT_EQ("new-frame-data", stored_data->content[0].frame());
}

TEST_F(FeedApiTest, HasUnreadContentIsFalseAfterFeedViewed) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  ASSERT_TRUE(stream_->HasUnreadContent(StreamType(StreamKind::kForYou)));
  stream_->ReportFeedViewed(surface.GetSurfaceId());

  EXPECT_FALSE(stream_->HasUnreadContent(StreamType(StreamKind::kForYou)));
}

TEST_F(FeedApiTest, HasUnreadContentRemainsFalseIfFeedViewedBeforeRefresh) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_TRUE(stream_->HasUnreadContent(StreamType(StreamKind::kForYou)));
  stream_->ReportFeedViewed(surface.GetSurfaceId());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());

  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices -> 3 slices",
            surface.DescribeUpdates());
  EXPECT_FALSE(stream_->HasUnreadContent(StreamType(StreamKind::kForYou)));
}

TEST_F(FeedApiTest,
       LoadingForYouStreamTriggersWebFeedRefreshIfNoUnreadContent) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Both streams should be fetched.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);

  // Attach a Web Feed surface, and verify content is loaded from the store.
  TestWebFeedSurface web_feed_surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices",
            web_feed_surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromStore,
            metrics_reporter_->load_stream_status);
}

TEST_F(
    FeedApiTest,
    LoadingForYouStreamDoesNotTriggerWebFeedRefreshIfNoSubscriptionsWhithoutOnboarding) {
  // With WebFeedOnboarding disabled, WebFeed should not fetch without
  // subscriptions.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kWebFeedOnboarding);
  // Only for-you feed is fetched on load.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.send_query_call_count);
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  EXPECT_EQ(LoadStreamStatus::kNotAWebFeedSubscriber,
            metrics_reporter_->Stream(StreamType(StreamKind::kFollowing))
                .background_refresh_status);
}

TEST_F(
    FeedApiTest,
    LoadForYouStreamDoesNotTriggerWebFeedRefreshContentIfIsAlreadyAvailable) {
  // WebFeed stream is only fetched when there's a subscription.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Both streams should be fetched because there is no unread web-feed content.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ(2, network_.send_query_call_count);

  // Detach and re-attach the surface. The for-you feed should be loaded again,
  // but this time the web-feed is not refreshed.
  surface.Detach();
  UnloadModel(surface.GetStreamType());
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  // Neither stream type should be refreshed.
  ASSERT_EQ(2, network_.send_query_call_count);
}

TEST_F(FeedApiTest, WasUrlRecentlyNavigatedFromFeed) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  const GURL url1("https://someurl1");
  const GURL url2("https://someurl2");
  EXPECT_FALSE(stream_->WasUrlRecentlyNavigatedFromFeed(url1));
  EXPECT_FALSE(stream_->WasUrlRecentlyNavigatedFromFeed(url2));

  stream_->ReportOpenAction(url1, surface.GetSurfaceId(), "slice",
                            OpenActionType::kDefault);
  stream_->ReportOpenAction(url2, surface.GetSurfaceId(), "slice",
                            OpenActionType::kNewTab);

  EXPECT_TRUE(stream_->WasUrlRecentlyNavigatedFromFeed(url1));
  EXPECT_TRUE(stream_->WasUrlRecentlyNavigatedFromFeed(url2));
}

// After 10 URLs are navigated, they are forgotten in FIFO order.
TEST_F(FeedApiTest, WasUrlRecentlyNavigatedFromFeedMaxHistory) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  std::vector<GURL> urls;
  for (int i = 0; i < 11; ++i)
    urls.emplace_back("https://someurl" + base::NumberToString(i));

  for (const GURL& url : urls)
    stream_->ReportOpenAction(url, surface.GetSurfaceId(), "slice",
                              OpenActionType::kDefault);

  EXPECT_FALSE(stream_->WasUrlRecentlyNavigatedFromFeed(urls[0]));
  for (size_t i = 1; i < urls.size(); ++i) {
    EXPECT_TRUE(stream_->WasUrlRecentlyNavigatedFromFeed(urls[i]));
  }
}

TEST_F(FeedApiTest, ClearAllOnStartupIfFeedIsDisabled) {
  CallbackReceiver<> on_clear_all;
  on_clear_all_ = on_clear_all.BindRepeating();

  // Fetch a feed, so that there's stored data.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Turn off the feed, and re-create FeedStream. It should perform a ClearAll.
  profile_prefs_.SetBoolean(feed::prefs::kEnableSnippets, false);
  CreateStream();
  EXPECT_TRUE(on_clear_all.called());

  // Re-create the feed, and verify ClearAll isn't called again.
  on_clear_all.Clear();
  base::HistogramTester histograms;
  CreateStream();
  EXPECT_FALSE(on_clear_all.called());
  histograms.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kFeedNotEnabledByPolicy,
                                1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(FeedApiTest, ClearAllOnStartupIfFeedIsDisabledByDse) {
  CallbackReceiver<> on_clear_all;
  on_clear_all_ = on_clear_all.BindRepeating();

  // Fetch a feed, so that there's stored data.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Turn off the feed, and re-create FeedStream. It should perform a ClearAll.
  profile_prefs_.SetBoolean(feed::prefs::kEnableSnippetsByDse, false);
  CreateStream(/*wait_for_initialization=*/true,
               /*is_new_tab_search_engine_url_android_enabled*/ true);
  EXPECT_TRUE(on_clear_all.called());

  // Re-create the feed, and verify ClearAll isn't called again.
  on_clear_all.Clear();
  base::HistogramTester histograms;
  CreateStream(/*wait_for_initialization=*/true,
               /*is_new_tab_search_engine_url_android_enabled*/ true);

  EXPECT_FALSE(on_clear_all.called());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(FeedApiTest, ReportUserSettingsFromMetadataWaaOnDpOff) {
  // Fetch a feed, so that there's stored data.
  {
    RefreshResponseData response;
    response.model_update_request = MakeTypicalInitialModelState();
    response.web_and_app_activity_enabled = true;
    response.last_fetch_timestamp = base::Time::Now();
    response_translator_.InjectResponse(std::move(response));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Simulate a Chrome restart.
  base::HistogramTester histograms;
  CreateStream();
  histograms.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInWaaOnDpOff, 1);
  EXPECT_EQ(
      std::vector<std::string>({"SignedInNoRecentData", "SignedInWaaOnDpOff"}),
      register_feed_user_settings_field_trial_calls_);
}

TEST_F(FeedApiTest, ReportUserSettingsFromMetadataWaaOffDpOn) {
  // Fetch a feed, so that there's stored data.
  {
    RefreshResponseData response;
    response.model_update_request = MakeTypicalInitialModelState();
    response.discover_personalization_enabled = true;
    response.last_fetch_timestamp = base::Time::Now();
    response_translator_.InjectResponse(std::move(response));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Simulate a Chrome restart.
  base::HistogramTester histograms;
  CreateStream();
  histograms.ExpectUniqueSample("ContentSuggestions.Feed.UserSettingsOnStart",
                                UserSettingsOnStart::kSignedInWaaOffDpOn, 1);
}

TEST_F(FeedStreamTestForAllStreamTypes, ManualRefreshWithoutSurfaceIsAborted) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  surface.Detach();
  WaitForIdleTaskQueue();
  surface.Clear();

  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  // Refresh fails, and surface is not updated.
  EXPECT_EQ(std::optional<bool>(false), callback.GetResult());
  EXPECT_EQ("", surface.DescribeUpdates());
}

TEST_F(FeedStreamTestForAllStreamTypes, ManualRefreshInterestFeedSuccess) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface2(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface2.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(std::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);

  // Check that the root event ID has been updated.
  EXPECT_EQ(MakeTypicalRefreshModelState()->stream_data.root_event_id(),
            surface.update->logging_parameters().root_event_id());
  EXPECT_NE(MakeTypicalInitialModelState()->stream_data.root_event_id(),
            surface.update->logging_parameters().root_event_id());

  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(StreamType(StreamKind::kForYou), store_.get()));
  // Verify another feed is not affected.
  EXPECT_EQ("", surface2.DescribeUpdates());
}

TEST_F(FeedApiTest, ManualRefreshResetsRequestThrottlerQuota) {
  Config config;
  // Small number to make test faster.
  config.max_action_upload_requests_per_day = 3;
  SetFeedConfigForTesting(config);

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  // Exhaust upload action request throttler quota.
  for (;;) {
    CallbackReceiver<UploadActionsTask::Result> callback;
    stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), true,
                          callback.Bind());
    if (callback.RunAndGetResult().upload_attempt_count == 0ul) {
      break;
    }
  }

  // Do a manual refresh, it should upload the queued action immediately, and
  // allow further upload actions.
  int action_requests_before =
      network_.GetApiRequestCount<UploadActionsDiscoverApi>();
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();
  EXPECT_GT(network_.GetApiRequestCount<UploadActionsDiscoverApi>(),
            action_requests_before);

  CallbackReceiver<UploadActionsTask::Result> callback;
  stream_->UploadAction(MakeFeedAction(42ul), CreateLoggingParameters(), true,
                        callback.Bind());
  EXPECT_GT(callback.RunAndGetResult().upload_attempt_count, 0ul);
}

TEST_F(FeedStreamTestForAllStreamTypes, ManualRefreshWebFeedSuccess) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface2.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(std::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(StreamType(StreamKind::kFollowing), store_.get()));
  // Verify another feed is not affected.
  EXPECT_EQ("", surface2.DescribeUpdates());
}

TEST_F(FeedApiTest, ManualRefreshFailsBecauseNetworkRequestFails) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  std::string original_store_dump =
      ModelStateFor(surface.GetStreamType(), store_.get());
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      original_store_dump);

  // Since we didn't inject a network response, the network update will fail.
  // The store should not be updated.
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(std::optional<bool>(false), callback.GetResult());
  EXPECT_EQ("cant-refresh", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kProtoTranslationFailed,
            metrics_reporter_->load_stream_status);
  EXPECT_STRINGS_EQUAL(ModelStateFor(surface.GetStreamType(), store_.get()),
                       original_store_dump);
}

TEST_F(FeedApiTest, ManualRefreshSuccessAfterUnload) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  UnloadModel(surface.GetStreamType());
  WaitForIdleTaskQueue();

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(std::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(StreamType(StreamKind::kForYou), store_.get()));
}

TEST_F(FeedApiTest, ManualRefreshSuccessAfterPreviousLoadFailure) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_EQ("loading -> cant-refresh", surface.DescribeUpdates());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  EXPECT_EQ(std::optional<bool>(true), callback.GetResult());
  EXPECT_EQ("no-cards -> [user@foo] 3 slices", surface.DescribeUpdates());
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->load_stream_status);
  // Verify stored state is equivalent to in-memory model.
  EXPECT_STRINGS_EQUAL(
      stream_->GetModel(surface.GetStreamType())->DumpStateForTesting(),
      ModelStateFor(StreamType(StreamKind::kForYou), store_.get()));
}

TEST_F(FeedApiTest, ManualRefreshFailesWhenLoadingInProgress) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  // Don't call WaitForIdleTaskQueue to finish the loading.

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  CallbackReceiver<bool> callback;
  stream_->ManualRefresh(surface.GetSurfaceId(), callback.Bind());
  WaitForIdleTaskQueue();
  // Manual refresh should fail immediately when loading is still in progress.
  EXPECT_EQ(std::optional<bool>(false), callback.GetResult());
  // The initial loading should finish.
  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, ManualRefresh_MetricsOnNoCardViewed) {
  base::HistogramTester histograms;

  // Load the initial page.
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Manual refresh.
  task_environment_.FastForwardBy(base::Seconds(100));
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ManualRefreshInterval", base::Seconds(100), 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardCountAtManualRefresh", 0, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardPercentageAtManualRefresh", 0, 1);
}

TEST_F(FeedApiTest, ManualRefresh_MetricsOnCardsViewed) {
  base::HistogramTester histograms;

  // Load the initial page.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // View a card.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(),
      surface.initial_state->updated_slices(1).slice().slice_id());
  WaitForIdleTaskQueue();

  // Manual refresh.
  task_environment_.FastForwardBy(base::Seconds(100));
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ManualRefreshInterval", base::Seconds(100), 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardCountAtManualRefresh", 1, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardPercentageAtManualRefresh", 50, 1);

  // View a card.
  stream_->ReportSliceViewed(
      surface.GetSurfaceId(),
      surface.update->updated_slices(0).slice().slice_id());
  WaitForIdleTaskQueue();

  // Manual refresh.
  task_environment_.FastForwardBy(base::Seconds(200));
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  histograms.ExpectBucketCount("ContentSuggestions.Feed.ManualRefreshInterval",
                               base::Seconds(100).InMilliseconds(), 1);
  histograms.ExpectBucketCount("ContentSuggestions.Feed.ManualRefreshInterval",
                               base::Seconds(200).InMilliseconds(), 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardCountAtManualRefresh", 1, 2);
  histograms.ExpectBucketCount(
      "ContentSuggestions.Feed.ViewedCardPercentageAtManualRefresh", 50, 1);
  histograms.ExpectBucketCount(
      "ContentSuggestions.Feed.ViewedCardPercentageAtManualRefresh", 33, 1);
}

TEST_F(FeedApiTest, ManualRefresh_MetricsOnCardsViewedAfterRestart) {
  base::HistogramTester histograms;

  // Load the initial page.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();

    // View a card.
    stream_->ReportSliceViewed(
        surface.GetSurfaceId(),
        surface.initial_state->updated_slices(1).slice().slice_id());
    WaitForIdleTaskQueue();

    histograms.ExpectUniqueSample("NewTabPage.ContentSuggestions.Shown", 1, 1,
                                  FROM_HERE);
  }

  // Simulate a Chrome restart.
  CreateStream();

  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  // View the same card.
  stream_->ReportSliceViewed(
      surface2.GetSurfaceId(),
      surface2.initial_state->updated_slices(1).slice().slice_id());
  WaitForIdleTaskQueue();

  histograms.ExpectUniqueSample("NewTabPage.ContentSuggestions.Shown", 1, 2,
                                FROM_HERE);

  // Manual refresh.
  task_environment_.FastForwardBy(base::Seconds(100));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ManualRefresh(surface2.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  histograms.ExpectUniqueTimeSample(
      "ContentSuggestions.Feed.ManualRefreshInterval", base::Seconds(100), 1,
      FROM_HERE);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardCountAtManualRefresh", 1, 1,
      FROM_HERE);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.ViewedCardPercentageAtManualRefresh", 50, 1,
      FROM_HERE);
}

TEST_F(FeedApiTest, ForYouContentOrderUnset) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(
      feedwire::FeedQuery::ContentOrder::
          FeedQuery_ContentOrder_CONTENT_ORDER_UNSPECIFIED,
      network_.query_request_sent->feed_request().feed_query().order_by());
}

TEST_F(FeedApiTest, ContentOrderIsGroupedByDefault) {
  base::HistogramTester histograms;
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  EXPECT_EQ(
      feedwire::FeedQuery::ContentOrder::FeedQuery_ContentOrder_GROUPED,
      network_.query_request_sent->feed_request().feed_query().order_by());
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.RefreshContentOrder",
      ContentOrder::kGrouped, 1);
}

TEST_F(FeedApiTest, SetContentOrderReloadsContent) {
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  base::HistogramTester histograms;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->SetContentOrder(StreamType(StreamKind::kFollowing),
                           ContentOrder::kReverseChron);
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices -> loading -> 2 slices",
            surface.DescribeUpdates());
  EXPECT_EQ(
      feedwire::FeedQuery::ContentOrder::FeedQuery_ContentOrder_RECENT,
      network_.query_request_sent->feed_request().feed_query().order_by());
  EXPECT_EQ(ContentOrder::kReverseChron,
            stream_->GetContentOrder(StreamType(StreamKind::kFollowing)));
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.RefreshContentOrder",
      ContentOrder::kReverseChron, 1);
}

TEST_F(FeedApiTest, SetContentOrderIsSavedeNotRefreshedIfUnchanged) {
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  // "Raw prefs" order value should start as unspecified.
  EXPECT_EQ(ContentOrder::kUnspecified,
            feed::prefs::GetWebFeedContentOrder(profile_prefs_));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestWebFeedSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->SetContentOrder(StreamType(StreamKind::kFollowing),
                           ContentOrder::kGrouped);
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());
  // "Raw prefs" order value should have been updated.
  EXPECT_EQ(ContentOrder::kGrouped,
            feed::prefs::GetWebFeedContentOrder(profile_prefs_));
  EXPECT_EQ(ContentOrder::kGrouped,
            stream_->GetContentOrder(StreamType(StreamKind::kFollowing)));
}

// This is a regression test for crbug.com/1249772.
TEST_F(FeedApiTest, SignInWhileSurfaceIsOpen) {
  account_info_ = {};  // not signed in initially.
  // Load content and simulate a restart, so that there is stored content.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();
    CreateStream();
  }

  // Simulate signing-in while the feed is open.
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  stream_->ReportFeedViewed(surface.GetSurfaceId());
  TestUnreadContentObserver observer;
  stream_->AddUnreadContentObserver(StreamType(StreamKind::kForYou), &observer);
  account_info_ = TestAccountInfo();
  stream_->OnSignedIn();
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> 2 slices -> loading -> [user@foo] 3 slices",
            surface.DescribeUpdates());
  // Even though content is updated, the feed remains in view, so content is not
  // unread.
  EXPECT_EQ(std::vector<bool>({false}), observer.calls);
}

TEST_F(FeedApiTest, SignOutWhileSurfaceIsOpen) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  account_info_ = {};
  stream_->OnSignedOut();
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  WaitForIdleTaskQueue();

  EXPECT_EQ(
      "loading -> [user@foo] 2 slices -> loading -> [NO Logging] 3 slices",
      surface.DescribeUpdates());
}

TEST_F(FeedApiTest, FollowedFromWebPageMenuCount) {
  // Arrange.
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("dogs")});
  TestWebFeedSurface surface(stream_.get());

  // Act.
  stream_->IncrementFollowedFromWebPageMenuCount();
  // Wait for the surface to load and a network request to be sent.
  WaitForIdleTaskQueue();

  // Assert.
  EXPECT_EQ(1, stream_->GetMetadata().followed_from_web_page_menu_count());
  EXPECT_EQ(
      1, stream_->GetRequestMetadata(StreamType(StreamKind::kFollowing), false)
             .followed_from_web_page_menu_count);

  // We should report exactly 1 case of followed from the menu in the feed
  // request.
  ASSERT_EQ(1, network_.query_request_sent->feed_request()
                   .feed_query()
                   .chrome_fulfillment_info()
                   .chrome_feature_usage()
                   .times_followed_from_web_page_menu());
}

TEST_F(FeedApiTest, InfoCardTrackingActions) {
  // Set up the server and client timestamps that affect the computation of
  // the view timestamps in the info card tracking state.
  base::Time server_timestamp = base::Time::Now();
  task_environment_.AdvanceClock(base::Seconds(100));
  base::Time client_timestamp = base::Time::Now();
  base::TimeDelta timestamp_adjustment = server_timestamp - client_timestamp;
  task_environment_.AdvanceClock(base::Seconds(200));

  // Load the initial page.
  RefreshResponseData response;
  response.model_update_request = MakeTypicalInitialModelState();
  response.last_fetch_timestamp = client_timestamp;
  response.server_response_sent_timestamp = server_timestamp;

  base::Time first_view_timestamp1, last_view_timestamp1, first_view_timestamp2,
      last_view_timestamp2;
  {
    response_translator_.InjectResponse(std::move(response));
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();

    base::HistogramTester histograms;

    // Perform actions on one info card and verify the histograms.
    first_view_timestamp2 = base::Time::Now() + timestamp_adjustment;
    last_view_timestamp2 = first_view_timestamp2;
    stream_->ReportInfoCardTrackViewStarted(surface.GetSurfaceId(),
                                            kTestInfoCardType2);
    stream_->ReportInfoCardViewed(surface.GetSurfaceId(), kTestInfoCardType2,
                                  kMinimumViewIntervalSeconds);
    stream_->ReportInfoCardClicked(surface.GetSurfaceId(), kTestInfoCardType2);
    stream_->ReportInfoCardClicked(surface.GetSurfaceId(), kTestInfoCardType2);
    histograms.ExpectUniqueSample("ContentSuggestions.Feed.InfoCard.Started",
                                  kTestInfoCardType2, 1, FROM_HERE);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Viewed",
                                 kTestInfoCardType2, 1, FROM_HERE);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Clicked",
                                 kTestInfoCardType2, 2, FROM_HERE);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Dismissed",
                                 kTestInfoCardType2, 0, FROM_HERE);

    // Perform actions on another info card and verify the histograms.
    first_view_timestamp1 = base::Time::Now() + timestamp_adjustment;
    stream_->ReportInfoCardViewed(surface.GetSurfaceId(), kTestInfoCardType1,
                                  kMinimumViewIntervalSeconds);
    task_environment_.AdvanceClock(base::Seconds(kMinimumViewIntervalSeconds));
    stream_->ReportInfoCardViewed(surface.GetSurfaceId(), kTestInfoCardType1,
                                  kMinimumViewIntervalSeconds);
    task_environment_.AdvanceClock(base::Seconds(kMinimumViewIntervalSeconds));
    last_view_timestamp1 = base::Time::Now() + timestamp_adjustment;
    stream_->ReportInfoCardViewed(surface.GetSurfaceId(), kTestInfoCardType1,
                                  kMinimumViewIntervalSeconds);
    stream_->ReportInfoCardClicked(surface.GetSurfaceId(), kTestInfoCardType1);
    stream_->ReportInfoCardDismissedExplicitly(surface.GetSurfaceId(),
                                               kTestInfoCardType1);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Started",
                                 kTestInfoCardType1, 0, FROM_HERE);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Viewed",
                                 kTestInfoCardType1, 3, FROM_HERE);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Clicked",
                                 kTestInfoCardType1, 1, FROM_HERE);
    histograms.ExpectBucketCount("ContentSuggestions.Feed.InfoCard.Dismissed",
                                 kTestInfoCardType1, 1, FROM_HERE);

    // Refresh the page so that a feed query including the info card tracking
    // states is sent. Call "CreateStream()" before the refresh to simulate
    // Chrome restart. This is used to test that info card tracking states are
    // sent in the initial page load when stream model is not loaded yet.
    response_translator_.InjectResponse(MakeTypicalRefreshModelState());
    surface.Detach();
  }

  // Simulate restart.
  CreateStream();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  // Verify the info card tracking states. There should be 2 states with
  // expected counts and view timestamps populated.
  ASSERT_EQ(2, network_.query_request_sent->feed_request()
                   .feed_query()
                   .chrome_fulfillment_info()
                   .info_card_tracking_state_size());
  feedwire::InfoCardTrackingState state1;
  state1.set_type(kTestInfoCardType1);
  state1.set_view_count(3);
  state1.set_click_count(1);
  state1.set_explicitly_dismissed_count(1);
  state1.set_first_view_timestamp(
      feedstore::ToTimestampMillis(first_view_timestamp1));
  state1.set_last_view_timestamp(
      feedstore::ToTimestampMillis(last_view_timestamp1));
  EXPECT_THAT(state1, EqualsProto(network_.query_request_sent->feed_request()
                                      .feed_query()
                                      .chrome_fulfillment_info()
                                      .info_card_tracking_state(0)));
  feedwire::InfoCardTrackingState state2;
  state2.set_type(kTestInfoCardType2);
  state2.set_view_count(1);
  state2.set_click_count(2);
  state2.set_first_view_timestamp(
      feedstore::ToTimestampMillis(first_view_timestamp2));
  state2.set_last_view_timestamp(
      feedstore::ToTimestampMillis(last_view_timestamp2));
  EXPECT_THAT(state2, EqualsProto(network_.query_request_sent->feed_request()
                                      .feed_query()
                                      .chrome_fulfillment_info()
                                      .info_card_tracking_state(1)));
}

TEST_F(FeedApiTest, InvalidateFeedCache_CauseRefreshOnNextLoad) {
  // Load the ForYou feed with initial content from the network.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Invalidate cached content and reattach the surface, waiting for the model
  // to unload. It should refresh from the network with refreshed content.
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->InvalidateContentCacheFor(StreamKind::kForYou);
  surface.Detach();
  WaitForModelToAutoUnload();
  surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_EQ("loading -> 3 slices", surface.DescribeUpdates());
}

TEST_F(FeedApiTest, InvalidateFeedCache_DoesNotForceRefreshFeedWhileInUse) {
  // Load the ForYou feed with initial content from the network.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  ASSERT_EQ("loading -> [user@foo] 2 slices", surface.DescribeUpdates());

  // Invalidate the ForYou feed and verify nothing changes right away.
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->InvalidateContentCacheFor(StreamKind::kForYou);
  WaitForIdleTaskQueue();
  ASSERT_EQ("", surface.DescribeUpdates());
  EXPECT_FALSE(response_translator_.InjectedResponseConsumed());
}

TEST_F(FeedApiTest, InvalidateFeedCache_UnknownDoesNotForceRefreshAnyFeeds) {
  {
    // Load both feeds and allow them to fetch network contents.
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface for_you_surface(stream_.get());
    TestWebFeedSurface web_feed_surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              for_you_surface.DescribeUpdates());
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              web_feed_surface.DescribeUpdates());
    EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  }

  // Both surfaces have been destroyed, so wait for the model to unload.
  WaitForModelToAutoUnload();

  // Invalidate with an unknown feed and recreate both surfaces. None should
  // refresh from network.
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->InvalidateContentCacheFor(StreamKind::kUnknown);
  {
    TestForYouSurface for_you_surface(stream_.get());
    TestWebFeedSurface web_feed_surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              for_you_surface.DescribeUpdates());
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              web_feed_surface.DescribeUpdates());
  }
  EXPECT_FALSE(response_translator_.InjectedResponseConsumed());
}

TEST_F(FeedApiTest, InvalidateFeedCache_DoesNotRefreshOtherFeed) {
  {
    // Load both feeds and allow them to fetch network contents.
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface for_you_surface(stream_.get());
    TestWebFeedSurface web_feed_surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              for_you_surface.DescribeUpdates());
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              web_feed_surface.DescribeUpdates());
    EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
  }

  // Both surfaces have been destroyed, so wait for the model to unload.
  WaitForModelToAutoUnload();

  // Invalidate only the WebFeed feed and recreate both surfaces. Only the
  // WebFeed should refresh from network.
  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  stream_->InvalidateContentCacheFor(StreamKind::kFollowing);
  {
    TestForYouSurface for_you_surface(stream_.get());
    TestWebFeedSurface web_feed_surface(stream_.get());
    WaitForIdleTaskQueue();
    ASSERT_EQ("loading -> [user@foo] 2 slices",
              for_you_surface.DescribeUpdates());
    ASSERT_EQ("loading -> [user@foo] 3 slices",
              web_feed_surface.DescribeUpdates());
  }
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
}

class FeedCloseRefreshTest : public FeedApiTest {
 public:
  void SetUp() override {
    FeedApiTest::SetUp();
    // Sometimes the clock starts near zero; move it forward just in case.
    task_environment_.AdvanceClock(base::Minutes(10));
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(FeedCloseRefreshTest, Scroll) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Simulate content being viewed. This shouldn't schedule a refresh itself,
  // but it's required in order for scrolling to schedule a refresh.
  stream_->ReportFeedViewed(surface.GetSurfaceId());
  EXPECT_EQ(base::Seconds(0),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Scrolling should cause a refresh to be scheduled.
  stream_->ReportStreamScrolled(surface.GetSurfaceId(), 1);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  refresh_scheduler_.Clear();

  // Scrolling shouldn't schedule a refresh for the next few minutes.
  stream_->ReportStreamScrolled(surface.GetSurfaceId(), 1);
  // Scheduler shouldn't have been called yet.
  EXPECT_EQ(base::Seconds(0),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  refresh_scheduler_.Clear();
  task_environment_.FastForwardBy(base::Minutes(5) + base::Seconds(1));
  stream_->ReportStreamScrolled(surface.GetSurfaceId(), 1);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedCloseRefreshTest, Open) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Opening should cause a refresh to be scheduled.
  stream_->ReportOpenAction(GURL("http://example.com"), surface.GetSurfaceId(),
                            "", OpenActionType::kDefault);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedCloseRefreshTest, OpenInNewTab) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Should cause a refresh to be scheduled.
  stream_->ReportOpenAction(GURL("http://example.com"), surface.GetSurfaceId(),
                            "", OpenActionType::kNewTab);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedCloseRefreshTest, ManualRefreshResetsCoalesceTimestamp) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Simulate content being viewed. This shouldn't schedule a refresh itself,
  // but it's required in order for later interaction to schedule a refresh.
  stream_->ReportFeedViewed(surface.GetSurfaceId());
  EXPECT_EQ(base::Seconds(0),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Should cause a refresh to be scheduled.
  stream_->ReportStreamScrolled(surface.GetSurfaceId(), 1);
  refresh_scheduler_.Clear();
  // Manual refresh resets the anti-jank timestamp, so the next
  // ReportStreamScrolled() should update the schedule.
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  stream_->ReportStreamScrolled(surface.GetSurfaceId(), 1);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedCloseRefreshTest, ExistingScheduleGetsReplaced) {
  // Inject a typical network response, with a server-defined request schedule.
  {
    RequestSchedule schedule;
    schedule.anchor_time = kTestTimeEpoch;
    schedule.refresh_offsets = {base::Minutes(12), base::Minutes(24)};
    RefreshResponseData response_data;
    response_data.model_update_request = MakeTypicalInitialModelState();
    response_data.request_schedule = schedule;
    response_translator_.InjectResponse(std::move(response_data));
  }
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  // Verify the first refresh was scheduled (epoch + 12 - 10)
  EXPECT_EQ(base::Minutes(2),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Simulate content being viewed. This shouldn't schedule a refresh itself,
  // but it's required in order for later interaction to schedule a refresh.
  stream_->ReportFeedViewed(surface.GetSurfaceId());

  // Should cause a refresh to be scheduled.
  stream_->ReportStreamScrolled(surface.GetSurfaceId(), 1);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedCloseRefreshTest, Retry) {
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  // Update the schedule.
  stream_->ReportOpenAction(GURL("http://example.com"), surface.GetSurfaceId(),
                            "", OpenActionType::kDefault);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Simulate an allowed refresh that failed.
  surface.Detach();
  task_environment_.FastForwardBy(base::Minutes(35));
  WaitForIdleTaskQueue();
  refresh_scheduler_.Clear();
  FeedNetwork::RawResponse raw_response;
  raw_response.response_info.status_code = 400;
  network_.InjectApiRawResponse<QueryBackgroundFeedDiscoverApi>(raw_response);
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // The next one should have been scheduled for 60 minutes after the anchor
  // time, and the clock advanced 35 minutes, so the next one should be
  // scheduled 25 minutes from now.
  EXPECT_EQ(std::set<RefreshTaskId>({RefreshTaskId::kRefreshForYouFeed}),
            refresh_scheduler_.completed_tasks);
  EXPECT_EQ(base::Minutes(25),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Same thing again. There should be one more scheduled retry.
  task_environment_.FastForwardBy(base::Minutes(35));
  refresh_scheduler_.Clear();
  network_.InjectApiRawResponse<QueryBackgroundFeedDiscoverApi>(raw_response);
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  // The next one should have been scheduled for 90 minutes after the anchor
  // time, and the clock advanced 70 minutes total, so the next one should be
  // scheduled 20 minutes from now.
  EXPECT_EQ(std::set<RefreshTaskId>({RefreshTaskId::kRefreshForYouFeed}),
            refresh_scheduler_.completed_tasks);
  EXPECT_EQ(base::Minutes(20),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);
}

TEST_F(FeedCloseRefreshTest, RequestType) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Opening should cause a refresh to be scheduled.
  stream_->ReportOpenAction(GURL("http://example.com"), surface.GetSurfaceId(),
                            "", OpenActionType::kDefault);
  EXPECT_EQ(base::Minutes(30),
            refresh_scheduler_
                .scheduled_run_times[RefreshTaskId::kRefreshForYouFeed]);

  // Close the surface and unload the model.
  surface.Detach();
  task_environment_.FastForwardBy(base::Seconds(2));
  WaitForIdleTaskQueue();

  // Do the refresh.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ExecuteRefreshTask(RefreshTaskId::kRefreshForYouFeed);
  WaitForIdleTaskQueue();

  ASSERT_TRUE(refresh_scheduler_.completed_tasks.count(
      RefreshTaskId::kRefreshForYouFeed));
  EXPECT_EQ(LoadStreamStatus::kLoadedFromNetwork,
            metrics_reporter_->background_refresh_status);
  EXPECT_TRUE(network_.query_request_sent);
  // Request reason should be set.
  EXPECT_EQ(feedwire::FeedQuery::APP_CLOSE_REFRESH,
            network_.query_request_sent->feed_request().feed_query().reason());
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
}

TEST_F(FeedApiTest, CheckDuplicatedContents) {
  Config config;
  config.max_most_recent_viewed_content_hashes = 6;
  SetFeedConfigForTesting(config);

  StreamModelUpdateRequestGenerator model_generator;
  TestForYouSurface surface;

  {
    base::HistogramTester histograms;
    std::vector<int> num_ids({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    response_translator_.InjectResponse(
        model_generator.MakeFirstPageWithSpecificContents(num_ids));
    surface.Attach(stream_.get());
    WaitForIdleTaskQueue();
    histograms.ExpectTotalCount(
        "ContentSuggestions.Feed.ContentDuplication2.Position1", 0, FROM_HERE);
    histograms.ExpectTotalCount(
        "ContentSuggestions.Feed.ContentDuplication2.Position2", 0, FROM_HERE);
    histograms.ExpectTotalCount(
        "ContentSuggestions.Feed.ContentDuplication2.Position3", 0, FROM_HERE);
    histograms.ExpectTotalCount(
        "ContentSuggestions.Feed.ContentDuplication2.Top10", 0, FROM_HERE);
    histograms.ExpectTotalCount(
        "ContentSuggestions.Feed.ContentDuplication2.ForAll", 0, FROM_HERE);
  }

  SurfaceId surface_id = surface.GetSurfaceId();
  StreamType stream_type = surface.GetStreamType();

  stream_->ReportSliceViewed(
      surface_id, surface.initial_state->updated_slices(0).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.initial_state->updated_slices(1).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.initial_state->updated_slices(2).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.initial_state->updated_slices(6).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.initial_state->updated_slices(8).slice().slice_id());
  // Viewed contents: 0, 1, 2, 6, 8

  {
    base::HistogramTester histograms;
    std::vector<int> num_ids(
        {7, 11, 6, 13, 14, 2, 16, 1, 5, 19, 12, 10, 8, 19});
    response_translator_.InjectResponse(
        model_generator.MakeFirstPageWithSpecificContents(num_ids));
    stream_->ManualRefresh(surface_id, base::DoNothing());
    WaitForIdleTaskQueue();
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position1", false, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position2", false, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position3", true, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.First10", 30, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.All", 28, 1, FROM_HERE);
  }

  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(1).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(3).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(4).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(5).slice().slice_id());
  // Viewed contents: (0, 1,) 6, 8, 11, 13, 14, 2

  {
    base::HistogramTester histograms;
    std::vector<int> num_ids(
        {8, 1, 9, 2, 30, 31, 5, 10, 12, 13, 32, 33, 14, 6});
    response_translator_.InjectResponse(
        model_generator.MakeFirstPageWithSpecificContents(num_ids));
    stream_->ManualRefresh(surface_id, base::DoNothing());
    WaitForIdleTaskQueue();
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position1", true, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position2", true, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position3", false, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.First10", 40, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.All", 42, 1, FROM_HERE);
  }

  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(0).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(4).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(5).slice().slice_id());
  stream_->ReportSliceViewed(
      surface_id, surface.update->updated_slices(6).slice().slice_id());
  // Viewed contents: (6, 11, 13), 14, 2, 8, 30, 31, 5

  // Simulate a Chrome restart.
  CreateStream();

  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  {
    base::HistogramTester histograms;
    std::vector<int> num_ids(
        {15, 11, 16, 2, 40, 41, 8, 42, 1, 43, 44, 14, 45, 47});
    response_translator_.InjectResponse(
        model_generator.MakeFirstPageWithSpecificContents(num_ids));
    stream_->ManualRefresh(surface2.GetSurfaceId(), base::DoNothing());
    WaitForIdleTaskQueue();
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position1", false, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position2", true, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.Position3", false, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.First10", 30, 1,
        FROM_HERE);
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.ContentDuplication2.All", 28, 1, FROM_HERE);
  }
}

TEST_F(FeedApiTest, GetRequestMetadataForSignedOutUser) {
  TestForYouSurface surface(stream_.get());
  account_info_ = {};

  RequestMetadata metadata =
      stream_->GetRequestMetadata(StreamType(StreamKind::kForYou), false);

  ASSERT_EQ(metadata.sign_in_status,
            feedwire::ChromeSignInStatus::NOT_SIGNED_IN);
}

TEST_F(FeedApiTest, GetRequestMetadataForSignedInUser) {
  TestForYouSurface surface(stream_.get());

  RequestMetadata metadata =
      stream_->GetRequestMetadata(StreamType(StreamKind::kForYou), false);

  ASSERT_EQ(metadata.sign_in_status, feedwire::ChromeSignInStatus::SIGNED_IN);
}

TEST_F(FeedApiTest, GetRequestMetadataForSigninDisallowedUser) {
  TestForYouSurface surface(stream_.get());
  account_info_ = {};
  is_signin_allowed_ = false;

  RequestMetadata metadata =
      stream_->GetRequestMetadata(StreamType(StreamKind::kForYou), false);

  ASSERT_EQ(metadata.sign_in_status,
            feedwire::ChromeSignInStatus::SIGNIN_DISALLOWED_BY_CONFIG);
}

TEST_F(FeedApiTest, GetRequestMetadataForSigninDisallowedUserWhenSignedIn) {
  TestForYouSurface surface(stream_.get());
  is_signin_allowed_ = false;

  RequestMetadata metadata =
      stream_->GetRequestMetadata(StreamType(StreamKind::kForYou), false);

  ASSERT_EQ(metadata.sign_in_status, feedwire::ChromeSignInStatus::SIGNED_IN);
}

TEST_F(FeedApiTest, GetRequestMetadataWithDefaultSearchEngine) {
  TestForYouSurface surface(stream_.get());
  RequestMetadata metadata =
      stream_->GetRequestMetadata(StreamType(StreamKind::kForYou), false);

  // Defaults to Google for tests.
  ASSERT_EQ(metadata.default_search_engine,
            feedwire::DefaultSearchEngine::ENGINE_GOOGLE);
}

TEST_F(FeedApiTest, ClearAllOnSigninAllowedPrefChange) {
  CallbackReceiver<> on_clear_all;
  on_clear_all_ = on_clear_all.BindRepeating();

  // Fetch a feed, so that there's stored data.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Set signin allowed pref to false. It should perform a ClearAll.
  profile_prefs_.SetBoolean(::prefs::kSigninAllowed, false);
  WaitForIdleTaskQueue();
  EXPECT_TRUE(on_clear_all.called());

  // Re-create the feed, and verify ClearAll isn't called again.
  on_clear_all.Clear();
  WaitForIdleTaskQueue();
  EXPECT_FALSE(on_clear_all.called());
}

TEST_F(FeedApiTest, RefreshFeedOnStartWithFlag) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kRefreshFeedOnRestart);

  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_TRUE(network_.query_request_sent);
  EXPECT_TRUE(response_translator_.InjectedResponseConsumed());
}

TEST_F(FeedApiTest, DoNotRefreshFeedOnStartWithoutFlag) {
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  store_->OverwriteStream(StreamType(StreamKind::kForYou),
                          MakeTypicalInitialModelState(), base::DoNothing());

  response_translator_.InjectResponse(MakeTypicalRefreshModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_FALSE(network_.query_request_sent);
  EXPECT_FALSE(response_translator_.InjectedResponseConsumed());
}

class SignedOutViewDemotionTest : public FeedApiTest {
 public:
  void SetUp() override {
    FeedApiTest::SetUp();
    features_.InitAndEnableFeature(kFeedSignedOutViewDemotion);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(SignedOutViewDemotionTest, ViewsAreSent) {
  account_info_ = {};
  // Simulate loading the feed, viewing two documents, and closing Chrome.
  {
    response_translator_.InjectResponse(MakeTypicalInitialModelState());
    TestForYouSurface surface(stream_.get());
    WaitForIdleTaskQueue();

    stream_->RecordContentViewed(surface.GetSurfaceId(), 123);
    stream_->RecordContentViewed(surface.GetSurfaceId(), 456);
    WaitForIdleTaskQueue();
  }

  base::HistogramTester histograms;
  // Simulate loading the feed again later after restart, triggering a refresh.
  task_environment_.FastForwardBy(GetFeedConfig().stale_content_threshold +
                                  base::Minutes(1));
  CreateStream(true);
  // Avoid a chained web-feed request to focus only on the for-you feed.
  stream_->SetChainedWebFeedRefreshEnabledForTesting(false);
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_THAT(network_.query_request_sent->feed_request()
                  .client_user_profiles()
                  .view_demotion_profile(),
              EqualsTextProto(
                  R"({
  view_demotion_profile {
    tables {
      name: "url_all_ondevice"
      num_rows: 2
      columns {
        type: 4
        name: "dimension_key"
        uint64_values: 123
        uint64_values: 456
      }
      columns {
        type: 2
        name: "FEED_CARD_VIEW"
        int64_values: 1
        int64_values: 1
      }
    }
  }
})"));

  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.DocumentViewSendCount100", 2, 1);
}

TEST_F(SignedOutViewDemotionTest, ViewsAreNotStoredWhenSignedIn) {
  base::HistogramTester histograms;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->RecordContentViewed(surface.GetSurfaceId(), 123);
  WaitForIdleTaskQueue();

  CallbackReceiver<std::vector<feedstore::DocView>> read_callback;
  store_->ReadDocViews(read_callback.Bind());
  EXPECT_THAT(read_callback.RunAndGetResult(), testing::IsEmpty());
  histograms.ExpectTotalCount(
      "ContentSuggestions.Feed.DocumentViewSendCount100", 0);
}

TEST_F(SignedOutViewDemotionTest, ViewsAreNotStoredWhenFeatureIsOff) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kFeedSignedOutViewDemotion);

  account_info_ = {};
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->RecordContentViewed(surface.GetSurfaceId(), 123);
  WaitForIdleTaskQueue();

  CallbackReceiver<std::vector<feedstore::DocView>> read_callback;
  store_->ReadDocViews(read_callback.Bind());
  EXPECT_THAT(read_callback.RunAndGetResult(), testing::IsEmpty());
  histograms.ExpectTotalCount(
      "ContentSuggestions.Feed.DocumentViewSendCount100", 0);
}

TEST_F(SignedOutViewDemotionTest, OldViewsAreDeleted) {
  account_info_ = {};
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  network_.query_request_sent.reset();
  stream_->RecordContentViewed(surface.GetSurfaceId(), 123);

  task_environment_.FastForwardBy(base::Hours(72) + base::Minutes(1));

  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  stream_->ManualRefresh(surface.GetSurfaceId(), base::DoNothing());
  WaitForIdleTaskQueue();

  EXPECT_THAT(network_.query_request_sent->feed_request()
                  .client_user_profiles()
                  .view_demotion_profile(),
              EqualsProto(feedwire::ViewDemotionProfile()));
  CallbackReceiver<std::vector<feedstore::DocView>> read_callback;
  store_->ReadDocViews(read_callback.Bind());
  EXPECT_THAT(read_callback.RunAndGetResult(), testing::IsEmpty());
}

// Keep instantiations at the bottom.
INSTANTIATE_TEST_SUITE_P(FeedApiTest,
                         FeedStreamTestForAllStreamTypes,
                         ::testing::Values(StreamType(StreamKind::kForYou),
                                           StreamType(StreamKind::kFollowing)),
                         ::testing::PrintToStringParamName());
INSTANTIATE_TEST_SUITE_P(FeedApiTest,
                         FeedNetworkEndpointTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace
}  // namespace test
}  // namespace feed
