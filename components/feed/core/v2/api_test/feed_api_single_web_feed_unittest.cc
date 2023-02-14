// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/proto/v2/wire/feed_entry_point_source.pb.h"
#include "components/feed/core/proto/v2/wire/feed_query.pb.h"

#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/stream_type.h"
#include "components/feed/core/v2/public/types.h"
#include "feed_api_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is for testing the Single Web Feed content.

namespace feed::test {
namespace {

TEST_F(FeedApiTest, SingleWebFeedShouldIgnoreQuota) {
  LoadStreamStatus status = LoadStreamStatus::kNoStatus;
  for (int i = 0; i < 50; i++) {
    status =
        stream_
            ->ShouldMakeFeedQueryRequest(StreamType(StreamKind::kSingleWebFeed),
                                         LoadType::kInitialLoad)
            .load_stream_status;
  }

  ASSERT_EQ(LoadStreamStatus::kNoStatus, status);
}

TEST_F(FeedApiTest, SingleWebFeed_AttachMultiple) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  StreamType stream_type_A(StreamKind::kSingleWebFeed, "A");
  StreamType stream_type_B(StreamKind::kSingleWebFeed, "B");

  TestSingleWebFeedSurface single_web_feed_surface_a(stream_.get(), "A");
  TestSingleWebFeedSurface single_web_feed_surface_b(stream_.get(), "B");

  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices",
            single_web_feed_surface_a.DescribeUpdates());
  ASSERT_EQ("loading -> [user@foo] 2 slices",
            single_web_feed_surface_b.DescribeUpdates());

  ASSERT_EQ(stream_->GetModel(stream_type_A)->DumpStateForTesting(),
            ModelStateFor(stream_type_A, store_.get()));
  ASSERT_EQ(stream_->GetModel(stream_type_B)->DumpStateForTesting(),
            ModelStateFor(stream_type_B, store_.get()));

  single_web_feed_surface_b.Detach();

  WaitForModelToAutoUnload();
  WaitForIdleTaskQueue();

  EXPECT_TRUE(stream_->GetModel(stream_type_A));
  EXPECT_FALSE(stream_->GetModel(stream_type_B));
}

TEST_F(FeedApiTest, SingleWebFeed_LoggingDisabledForMenu) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState(
      0, kTestTimeEpoch, true, true, false,
      feedstore::StreamKey(StreamType(StreamKind::kSingleWebFeed, "A",
                                      SingleWebFeedEntryPoint::kMenu))));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestSingleWebFeedSurface single_web_feed_surface_menu(
      stream_.get(), "A", SingleWebFeedEntryPoint::kMenu);

  TestSingleWebFeedSurface single_web_feed_surface_other(
      stream_.get(), "B", SingleWebFeedEntryPoint::kOther);

  TestSingleWebFeedSurface single_web_feed_surface_attribution(
      stream_.get(), "A", SingleWebFeedEntryPoint::kAttribution);

  WaitForIdleTaskQueue();

  EXPECT_EQ("loading -> [NO logging user@foo] 2 slices",
            single_web_feed_surface_menu.DescribeUpdates());
  EXPECT_EQ("loading -> [user@foo] 2 slices",
            single_web_feed_surface_other.DescribeUpdates());
  EXPECT_EQ("loading -> [user@foo] 2 slices",
            single_web_feed_surface_attribution.DescribeUpdates());
}

TEST_F(FeedApiTest, SingleWebFeed_EntryPointSetInSurface) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  StreamType stream_type_A(StreamKind::kSingleWebFeed, "A");

  TestSingleWebFeedSurface single_web_feed_surface_recommendation(
      stream_.get(), "A", SingleWebFeedEntryPoint::kRecommendation);

  WaitForIdleTaskQueue();

  EXPECT_EQ(
      SingleWebFeedEntryPoint::kRecommendation,
      single_web_feed_surface_recommendation.GetSingleWebFeedEntryPoint());

  EXPECT_TRUE(network_.query_request_sent);
  ASSERT_EQ(
      feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_RECOMMENDATION,
      network_.query_request_sent->feed_request()
          .feed_query()
          .feed_entry_point_data()
          .feed_entry_point_source_value());

  single_web_feed_surface_recommendation.Detach();
  // Uploaded action should have been erased from the store.
  network_.ClearTestData();

  // Surfaces with different entry point types share stream data, with the
  // exceptions of surfaces that are created with kMenu. So this is waiting as a
  // new entry point will only reported after the previous stream data is
  // cleared on timeout.

  WaitForModelToAutoUnload();

  task_environment_.FastForwardBy(base::Seconds(70));
  WaitForIdleTaskQueue();

  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestSingleWebFeedSurface single_web_feed_surface_attribution(
      stream_.get(), "A", SingleWebFeedEntryPoint::kAttribution);

  WaitForIdleTaskQueue();

  EXPECT_EQ(SingleWebFeedEntryPoint::kAttribution,
            single_web_feed_surface_attribution.GetSingleWebFeedEntryPoint());

  EXPECT_TRUE(network_.query_request_sent);
  ASSERT_EQ(feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_ATTRIBUTION,
            network_.query_request_sent->feed_request()
                .feed_query()
                .feed_entry_point_data()
                .feed_entry_point_source_value());

  // Uploaded action should have been erased from the store.
  network_.ClearTestData();
  WaitForIdleTaskQueue();
  response_translator_.InjectResponse(MakeTypicalInitialModelState());

  TestSingleWebFeedSurface single_web_feed_surface_menu(
      stream_.get(), "A", SingleWebFeedEntryPoint::kMenu);

  WaitForIdleTaskQueue();

  EXPECT_EQ(SingleWebFeedEntryPoint::kMenu,
            single_web_feed_surface_menu.GetSingleWebFeedEntryPoint());

  EXPECT_TRUE(network_.query_request_sent);
  ASSERT_EQ(feedwire::FeedEntryPointSource::CHROME_SINGLE_WEB_FEED_MENU,
            network_.query_request_sent->feed_request()
                .feed_query()
                .feed_entry_point_data()
                .feed_entry_point_source_value());
}

TEST_F(FeedApiTest, SingleWebFeed_DelayedDeletion) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  StreamType stream_type(StreamKind::kSingleWebFeed, "A");

  TestSingleWebFeedSurface single_web_feed_surface(stream_.get(), "A");

  WaitForIdleTaskQueue();

  ASSERT_EQ("loading -> [user@foo] 2 slices",
            single_web_feed_surface.DescribeUpdates());

  ASSERT_EQ(stream_->GetModel(stream_type)->DumpStateForTesting(),
            ModelStateFor(stream_type, store_.get()));

  single_web_feed_surface.Detach();

  WaitForModelToAutoUnload();
  EXPECT_TRUE(stream_->GetStreamPresentForTest(stream_type));
  task_environment_.FastForwardBy(base::Seconds(70));
  WaitForIdleTaskQueue();

  EXPECT_FALSE(stream_->GetModel(stream_type));
  EXPECT_FALSE(stream_->GetStreamPresentForTest(stream_type));

  ASSERT_EQ("{Failed to load model from store}",
            ModelStateFor(stream_type, store_.get()));

  EXPECT_FALSE(stream_->GetStreamPresentForTest(stream_type));
}

TEST_F(FeedApiTest, SingleWebFeed_DataRemovedOnStartup) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  StreamType stream_type(StreamKind::kSingleWebFeed, "A");

  TestSingleWebFeedSurface single_web_feed_feed_surface(stream_.get(), "A");
  WaitForIdleTaskQueue();

  ASSERT_NE("{Failed to load model from store}",
            ModelStateFor(stream_type, store_.get()));
  // Creating a stream should init database.
  CreateStream();
  WaitForIdleTaskQueue();

  ASSERT_EQ("{Failed to load model from store}",
            ModelStateFor(stream_type, store_.get()));
}

TEST_F(FeedApiTest, SingleWebFeed_ReattachedSingleWebStreamFetches) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  StreamType stream_type(StreamKind::kSingleWebFeed, "A");

  EXPECT_EQ(0, prefetch_image_call_count_);

  TestSingleWebFeedSurface single_web_feed_feed_surface(stream_.get(), "A");
  WaitForIdleTaskQueue();

  ASSERT_EQ(stream_->GetModel(stream_type)->DumpStateForTesting(),
            ModelStateFor(stream_type, store_.get()));

  EXPECT_EQ("loading -> [user@foo] 2 slices",
            single_web_feed_feed_surface.DescribeUpdates());
  single_web_feed_feed_surface.Detach();

  WaitForModelToAutoUnload();
  WaitForIdleTaskQueue();

  EXPECT_FALSE(stream_->GetModel(stream_type));

  task_environment_.FastForwardBy(base::Seconds(40));

  single_web_feed_feed_surface.Attach(stream_.get());
  WaitForIdleTaskQueue();
  // verify no new fetches were required to populate reattach.
  EXPECT_EQ("loading -> 2 slices",
            single_web_feed_feed_surface.DescribeUpdates());
}

}  // namespace
}  // namespace feed::test
