// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/core/v2/test/proto_printer.h"
#include "components/feed/core/v2/test/stream_builder.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {
namespace {
using feedwire::webfeed::WebFeedChangeReason;
using testing::PrintToString;

AccountInfo TestAccountInfo() {
  return {"examplegaia", "example@foo.com"};
}

FeedNetwork::RawResponse MakeFailedResponse() {
  FeedNetwork::RawResponse network_response;
  network_response.response_info.status_code = 400;
  return network_response;
}

void WriteRecommendedFeeds(
    FeedStore& store,
    std::vector<feedstore::WebFeedInfo> recommended_feeds) {
  feedstore::RecommendedWebFeedIndex index;
  for (const auto& info : recommended_feeds) {
    auto* entry = index.add_entries();
    entry->set_web_feed_id(info.web_feed_id());
    *entry->mutable_matchers() = info.matchers();
  }

  store.WriteRecommendedFeeds(index, recommended_feeds, base::DoNothing());
}

void WriteSubscribedFeeds(
    FeedStore& store,
    std::vector<feedstore::WebFeedInfo> recommended_feeds) {
  feedstore::SubscribedWebFeeds record;
  for (const feedstore::WebFeedInfo& info : recommended_feeds) {
    *record.add_feeds() = info;
  }
  record.set_update_time_millis(
      feedstore::ToTimestampMillis(base::Time::Now()));

  store.WriteSubscribedFeeds(record, base::DoNothing());
}

class FeedApiSubscriptionsTest : public FeedApiTest {
 public:
  void SetUp() override {
    FeedApiTest::SetUp();
    subscriptions().SetHooksForTesting(&web_feed_subscription_hooks);
    web_feed_subscription_hooks.before_clear_all = base::DoNothing();
    web_feed_subscription_hooks.after_clear_all = base::DoNothing();
  }

  // The test fixture disables the delayed fetch after startup. This function
  // makes the default config active instead, which allows delayed fetch at
  // startup.
  void SetUpWithDefaultConfig() {
    SetFeedConfigForTesting(Config());
    CreateStream();
  }

  // Get all subscriptions, and check that stored subscriptions match.
  std::vector<WebFeedMetadata> CheckAllSubscriptions() {
    // Get subscriptions stored in memory.
    CallbackReceiver<std::vector<WebFeedMetadata>> all_subscriptions;
    subscriptions().GetAllSubscriptions(all_subscriptions.Bind());
    std::vector<WebFeedMetadata> result = all_subscriptions.RunAndGetResult();
    // Clear stream data and load subscriptions from FeedStore
    CreateStream();
    CallbackReceiver<std::vector<WebFeedMetadata>> all_subscriptions2;
    subscriptions().GetAllSubscriptions(all_subscriptions2.Bind());
    std::vector<WebFeedMetadata> result2 = all_subscriptions2.RunAndGetResult();
    // Check that they match.
    ([&]() { ASSERT_EQ(PrintToString(result), PrintToString(result2)); })();
    return result;
  }

  // The stored pending operations in WebFeedMetadataModel are eventually
  // consistent with those in the database. This verifies that the in-memory
  // copy in WebFeedMetadataModel are equivalent to the stored copy.
  void CheckPendingOperationsAreStored() {
    std::vector<feedstore::PendingWebFeedOperation> stored =
        GetAllPendingOperations();
    std::vector<feedstore::PendingWebFeedOperation> in_memory =
        subscriptions().GetPendingOperationStateForTesting();
    auto sort_fn = [](feedstore::PendingWebFeedOperation& a,
                      feedstore::PendingWebFeedOperation& b) {
      return a.id() < b.id();
    };
    std::sort(stored.begin(), stored.end(), sort_fn);
    std::sort(in_memory.begin(), in_memory.end(), sort_fn);
    EXPECT_THAT(stored,
                ::testing::Pointwise(::base::test::EqualsProto(), in_memory));
  }

  std::vector<feedstore::PendingWebFeedOperation> GetAllPendingOperations() {
    // Get subscriptions stored in memory.
    CallbackReceiver<FeedStore::WebFeedStartupData> startup_data;
    store_->ReadWebFeedStartupData(startup_data.Bind());
    return startup_data.RunAndGetResult().pending_operations;
  }

  // Get all recommended web feeds.
  std::vector<WebFeedMetadata> GetRecommendedFeeds() {
    std::vector<WebFeedIndex::Entry> index_entries =
        subscriptions().index().GetRecommendedEntriesForTesting();

    std::vector<WebFeedMetadata> result;
    for (const WebFeedIndex::Entry& entry : index_entries) {
      CallbackReceiver<WebFeedMetadata> metadata;
      subscriptions().FindWebFeedInfoForWebFeedId(entry.web_feed_id,
                                                  metadata.Bind());
      result.push_back(metadata.RunAndGetResult());
    }
    return result;
  }

  // Get all recommended web feeds, and check that stored ones match.
  std::vector<WebFeedMetadata> CheckRecommendedFeeds() {
    std::vector<WebFeedMetadata> recommended_feeds = GetRecommendedFeeds();
    CreateStream();
    std::vector<WebFeedMetadata> recommended_after_reload =
        GetRecommendedFeeds();

    // Make sure entries didn't change.
    ([&]() {
      ASSERT_EQ(PrintToString(recommended_after_reload),
                PrintToString(recommended_feeds));
    })();
    return recommended_feeds;
  }

  WebFeedMetadata FindWebFeedInfoForWebFeedIdSync(
      const std::string& web_feed_id) {
    CallbackReceiver<WebFeedMetadata> callback;
    subscriptions().FindWebFeedInfoForWebFeedId(web_feed_id, callback.Bind());
    return callback.RunAndGetResult();
  }

  void InjectRecommendedWebFeedsResponse(
      std::vector<feedwire::webfeed::WebFeed> web_feeds) {
    feedwire::webfeed::ListRecommendedWebFeedsResponse response;
    for (const auto& feed : web_feeds) {
      *response.add_recommended_web_feeds() = feed;
    }
    network_.InjectResponse(response);
  }

  void SetupWithSubscriptions(
      std::vector<feedwire::webfeed::WebFeed> subscribed_feeds) {
    CallbackReceiver<WebFeedSubscriptions::RefreshResult> refresh_result;
    network_.InjectListWebFeedsResponse(subscribed_feeds);
    subscriptions().RefreshSubscriptions(refresh_result.Bind());
    refresh_result.RunUntilCalled();
  }

  WebFeedSubscriptionCoordinator& subscriptions() {
    return stream_->subscriptions();
  }

 protected:
  WebFeedSubscriptionCoordinator::HooksForTesting web_feed_subscription_hooks;
};

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedSuccess) {
  {
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }
  base::HistogramTester histograms;
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("http://cats.com");
  page_info.SetRssUrls({GURL("http://rss1/"), GURL("http://rss2/")});
  subscriptions().FollowWebFeed(page_info, WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  auto sent_request = network_.GetApiRequestSent<FollowWebFeedDiscoverApi>();
  ASSERT_THAT(sent_request->page_rss_uris(),
              testing::ElementsAre("http://rss1/", "http://rss2/"));
  EXPECT_EQ(sent_request->canonical_uri(), "");
  EXPECT_EQ("token", sent_request->consistency_token().token());
  EXPECT_EQ(sent_request->change_reason(), WebFeedChangeReason::WEB_PAGE_MENU);
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed }",
      PrintToString(callback.RunAndGetResult().web_feed_metadata));
  EXPECT_EQ(1, callback.RunAndGetResult().subscription_count);
  EXPECT_EQ("follow-ct", stream_->GetMetadata().consistency_token());
  EXPECT_TRUE(feedstore::IsKnownStale(stream_->GetMetadata(),
                                      StreamType(StreamKind::kFollowing)));
  ASSERT_THAT(
      network_.GetApiRequestSent<FollowWebFeedDiscoverApi>()->page_rss_uris(),
      testing::ElementsAre("http://rss1/", "http://rss2/"));
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowUriResult",
      WebFeedSubscriptionRequestStatus::kSuccess, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.AfterFollow", 1, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.NewFollow.IsRecommended", 0, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.NewFollow.ChangeReason",
      WebFeedChangeReason::WEB_PAGE_MENU, 1);
}

TEST_F(FeedApiSubscriptionsTest, QueryWebFeedSuccess) {
  {
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }

  base::HistogramTester histograms;
  network_.InjectResponse(SuccessfulQueryResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::QueryWebFeedResult> callback;

  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("http://cats.com");
  subscriptions().QueryWebFeed(page_info.url(), callback.Bind());
  EXPECT_EQ(WebFeedQueryRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  auto sent_request = network_.GetApiRequestSent<QueryWebFeedDiscoverApi>();
  EXPECT_STREQ("http://cats.com/",
               sent_request->web_feed_uris().web_page_uri().c_str());
  EXPECT_EQ("token", sent_request->consistency_token().token());
  EXPECT_EQ("id_cats", callback.RunAndGetResult().web_feed_id);
  EXPECT_EQ("query-ct", stream_->GetMetadata().consistency_token());

  histograms.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.QueryResult",
                                WebFeedSubscriptionRequestStatus::kSuccess, 1);
}

TEST_F(FeedApiSubscriptionsTest, QueryWebFeedError) {
  base::HistogramTester histograms;
  network_.InjectQueryResponse(MakeFailedResponse());
  CallbackReceiver<WebFeedSubscriptions::QueryWebFeedResult> callback;
  subscriptions().QueryWebFeed(GURL("http://cats.com"), callback.Bind());

  EXPECT_EQ(WebFeedQueryRequestStatus::kFailedUnknownError,
            callback.RunAndGetResult().request_status);

  histograms.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.QueryResult",
                                WebFeedQueryRequestStatus::kFailedUnknownError,
                                1);
}

TEST_F(FeedApiSubscriptionsTest, QueryWebFeedInvalidUrlError) {
  base::HistogramTester histograms;
  CallbackReceiver<WebFeedSubscriptions::QueryWebFeedResult> callback;
  subscriptions().QueryWebFeed(GURL(), callback.Bind());

  EXPECT_EQ(WebFeedQueryRequestStatus::kFailedInvalidUrl,
            callback.RunAndGetResult().request_status);

  histograms.ExpectUniqueSample("ContentSuggestions.Feed.WebFeed.QueryResult",
                                WebFeedQueryRequestStatus::kFailedInvalidUrl,
                                1);
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedAbortOnClearAll) {
  // The goal of this test is to test the task order:
  // ClearAllTask, SubscribeToWebFeedTask.

  // Set up a function to fetch the status of the "cats" webfeed. First, use
  // GetAllSubscriptions to force the internal model to load. This ensures that
  // FindWebFeedInfoForWebFeedId() will call its callback without a PostTask.
  subscriptions().GetAllSubscriptions(base::DoNothing());
  WaitForIdleTaskQueue();

  auto find_cats_subscription_status = [&]() {
    CallbackReceiver<WebFeedMetadata> result;
    subscriptions().FindWebFeedInfoForWebFeedId("cats", result.Bind());
    EXPECT_TRUE(result.GetResult());
    if (!result.GetResult())
      return WebFeedSubscriptionStatus::kUnknown;
    return result.GetResult()->subscription_status;
  };

  stream_->OnCacheDataCleared();
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;

  // Try to follow cats.com.
  subscriptions().FollowWebFeed("cats", /*is_durable_request=*/false,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  EXPECT_EQ(WebFeedSubscriptionStatus::kSubscribeInProgress,
            find_cats_subscription_status());

  // Run until ClearAllTask completes, this should update the subscription
  // status.
  base::RunLoop run_loop;
  web_feed_subscription_hooks.after_clear_all =
      base::BindLambdaForTesting([&]() {
        // The Follow task has not yet completed, and the subscription is no
        // longer in progress due to ClearAll.
        EXPECT_FALSE(follow_callback.GetResult());
        EXPECT_EQ(WebFeedSubscriptionStatus::kNotSubscribed,
                  find_cats_subscription_status());
        run_loop.Quit();
      });
  run_loop.Run();

  // Finally, let the subscription task complete.
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::
                kAbortWebFeedSubscriptionPendingClearAll,
            follow_callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionStatus::kNotSubscribed,
            find_cats_subscription_status());
}

TEST_F(FeedApiSubscriptionsTest, UnfollowWebFeedAbortOnClearAll) {
  // Follow 'cats'.
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  // Test task order: ClearAllTask, UnsubscribeToWebFeedTask.
  stream_->OnCacheDataCleared();
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::
                kAbortWebFeedSubscriptionPendingClearAll,
            unfollow_callback.RunAndGetResult().request_status);
}

TEST_F(FeedApiSubscriptionsTest, SubscribedWebFeedsAreLoadedFromStore) {
  // Store a subscribed web feed, and ensure it is loaded.
  WriteSubscribedFeeds(*store_, {MakeWebFeedInfo("catfood")});

  CallbackReceiver<std::vector<WebFeedMetadata>> subscriptions_callback;
  subscriptions().GetAllSubscriptions(subscriptions_callback.Bind());

  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_catfood title=Title catfood "
      "publisher_url=https://catfood.com/ status=kSubscribed } }",
      PrintToString(subscriptions_callback.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest, ClearAllAbortsModelLoad) {
  // In this test, we want to trigger model loading, hit ClearAllFinished, and
  // then verify the model loading completes. This unfortunately requires a
  // test-only hook.

  // Store a subscribed feed.
  WriteSubscribedFeeds(*store_, {MakeWebFeedInfo("catfood")});

  // Trigger ClearAll. Just before processing ClearAllFinished, trigger a model
  // load.
  CallbackReceiver<std::vector<WebFeedMetadata>> subscriptions_callback;
  web_feed_subscription_hooks.before_clear_all =
      base::BindLambdaForTesting([&]() {
        subscriptions().GetAllSubscriptions(subscriptions_callback.Bind());
        EXPECT_FALSE(subscriptions_callback.called());
        EXPECT_TRUE(subscriptions().is_loading_model_for_testing());
      });
  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  // Model should be loaded, and GetAllSubscriptions should complete.
  EXPECT_TRUE(subscriptions_callback.called());
  EXPECT_FALSE(subscriptions().is_loading_model_for_testing());
  // This call uses the model, and confirms it is non-null.
  EXPECT_EQ(WebFeedSubscriptionStatus::kUnknown,
            subscriptions().FindSubscriptionInfoById("catfood").status);
  // Unlike the SubscribedWebFeedsAreLoadedFromStore test, there are no
  // subscribed feeds loaded.
  EXPECT_EQ("{}", PrintToString(*subscriptions_callback.GetResult()));
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedSendsCanonicalUrl) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("http://cats.com");
  page_info.SetCanonicalUrl(GURL("http://felis-catus.com"));
  subscriptions().FollowWebFeed(page_info, WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  callback.RunUntilCalled();

  auto sent_request = network_.GetApiRequestSent<FollowWebFeedDiscoverApi>();
  EXPECT_EQ("http://felis-catus.com/", sent_request->canonical_uri());
}

TEST_F(FeedApiSubscriptionsTest, FollowRecommendedWebFeedById) {
  base::HistogramTester histograms;
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("catfood")});
  CreateStream();
  network_.InjectResponse(SuccessfulFollowResponse("catfood"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  subscriptions().FollowWebFeed("id_catfood", /*is_durable_request=*/false,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_catfood is_recommended title=Title catfood "
      "publisher_url=https://catfood.com/ status=kSubscribed }",
      PrintToString(callback.RunAndGetResult().web_feed_metadata));
  EXPECT_EQ(1, callback.RunAndGetResult().subscription_count);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowByIdResult",
      WebFeedSubscriptionRequestStatus::kSuccess, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.AfterFollow", 1, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.NewFollow.IsRecommended", 1, 1);
}

// Make two Follow attempts for the same page. Both appear successful, but only
// one network request is made.
TEST_F(FeedApiSubscriptionsTest, FollowWebFeedTwiceAtOnce) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback2;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback2.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback2.RunAndGetResult().request_status);
  EXPECT_EQ(1, callback.RunAndGetResult().subscription_count);
  EXPECT_EQ(1, network_.GetFollowRequestCount());
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
}

// Follow two different pages which resolve to the same web feed server-side.
TEST_F(FeedApiSubscriptionsTest, FollowWebFeedTwiceFromDifferentUrls) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback2;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  subscriptions().FollowWebFeed(
      MakeWebFeedPageInformation("http://cool-cats.com"),
      WebFeedChangeReason::WEB_PAGE_MENU, callback2.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback2.RunAndGetResult().request_status);
  EXPECT_EQ(2, network_.GetFollowRequestCount());
  EXPECT_EQ(1, callback.RunAndGetResult().subscription_count);
  EXPECT_EQ(1, callback2.RunAndGetResult().subscription_count);
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, FollowTwoWebFeedsAtOnce) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  network_.InjectResponse(SuccessfulFollowResponse("dogs"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback2;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://dogs.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback2.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback2.RunAndGetResult().request_status);
  EXPECT_EQ(1, callback.RunAndGetResult().subscription_count);
  EXPECT_EQ(2, callback2.RunAndGetResult().subscription_count);
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed }, "
      "WebFeedMetadata{ id=id_dogs title=Title dogs "
      "publisher_url=https://dogs.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, CantFollowWebFeedWhileOffline) {
  is_offline_ = true;
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());

  EXPECT_EQ(0, network_.GetFollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedOffline,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, CantFollowWebFeedByIdWhileOffline) {
  base::HistogramTester histograms;
  is_offline_ = true;
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed("feed_id", /*is_durable_request=*/false,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());

  EXPECT_EQ(0, network_.GetFollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedOffline,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowByIdResult",
      WebFeedSubscriptionRequestStatus::kFailedOffline, 1);
  histograms.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.FollowCount.AfterFollow", 0);
  histograms.ExpectTotalCount(
      "ContentSuggestions.Feed.WebFeed.NewFollow.IsRecommended", 0);
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedNetworkError) {
  base::HistogramTester histograms;
  network_.InjectFollowResponse(MakeFailedResponse());
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  EXPECT_FALSE(feedstore::IsKnownStale(stream_->GetMetadata(),
                                       StreamType(StreamKind::kFollowing)));

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
  EXPECT_FALSE(feedstore::IsKnownStale(stream_->GetMetadata(),
                                       StreamType(StreamKind::kFollowing)));
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowUriResult",
      WebFeedSubscriptionRequestStatus::kFailedUnknownError, 1);
}

// Follow and then unfollow a web feed successfully.
TEST_F(FeedApiSubscriptionsTest, UnfollowAFollowedWebFeed) {
  base::HistogramTester histograms;
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();
  // Un-mark stream as stale, to verify unsubscribe also marks stream as stale.
  stream_->SetStreamStale(StreamType(StreamKind::kFollowing), false);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(1, network_.GetUnfollowRequestCount());
  EXPECT_EQ("follow-ct",
            network_.GetApiRequestSent<UnfollowWebFeedDiscoverApi>()
                ->consistency_token()
                .token());
  EXPECT_EQ(
      network_.GetApiRequestSent<UnfollowWebFeedDiscoverApi>()->change_reason(),
      WebFeedChangeReason::WEB_PAGE_MENU);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            unfollow_callback.RunAndGetResult().request_status);
  EXPECT_EQ(0, unfollow_callback.RunAndGetResult().subscription_count);
  EXPECT_EQ("unfollow-ct", stream_->GetMetadata().consistency_token());
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
  EXPECT_TRUE(feedstore::IsKnownStale(stream_->GetMetadata(),
                                      StreamType(StreamKind::kFollowing)));
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.UnfollowResult",
      WebFeedSubscriptionRequestStatus::kSuccess, 1);
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.FollowCount.AfterUnfollow", 0, 1);
}

TEST_F(FeedApiSubscriptionsTest, UnfollowAFollowedWebFeedTwiceAtOnce) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback1;
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback2;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback1.Bind());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback2.Bind());

  unfollow_callback1.RunUntilCalled();
  unfollow_callback2.RunUntilCalled();
  EXPECT_EQ(1, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            unfollow_callback1.GetResult()->request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            unfollow_callback2.GetResult()->request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowNetworkFailure) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(MakeFailedResponse());
  // Un-mark stream as stale, to verify unsubscribe also marks stream as stale.
  stream_->SetStreamStale(StreamType(StreamKind::kFollowing), false);
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(1, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            unfollow_callback.GetResult()->request_status);
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
  EXPECT_FALSE(feedstore::IsKnownStale(stream_->GetMetadata(),
                                       StreamType(StreamKind::kFollowing)));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowWhileOffline) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  is_offline_ = true;

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(MakeFailedResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(0, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedOffline,
            unfollow_callback.GetResult()->request_status);
}

// Try to unfollow a web feed that is already unfollowed. This is successful
// without any network requests.
TEST_F(FeedApiSubscriptionsTest, UnfollowAnUnfollowedWebFeed) {
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed("notfollowed", /*is_durable_request=*/false,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(0, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            unfollow_callback.GetResult()->request_status);
}

TEST_F(FeedApiSubscriptionsTest, FindWebFeedInfoForPageNotSubscribed) {
  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(
      MakeWebFeedPageInformation("https://catfood.com"), metadata.Bind());

  EXPECT_EQ("WebFeedMetadata{ status=kNotSubscribed }",
            PrintToString(metadata.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest, FindWebFeedInfoForWebFeedIdNotSubscribed) {
  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());

  EXPECT_EQ("WebFeedMetadata{ id=id_cats status=kNotSubscribed }",
            PrintToString(metadata.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForWebFeedIdRecommendedButNotSubscribed) {
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("cats")});
  CreateStream();
  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_cats is_recommended title=Title cats "
      "publisher_url=https://cats.com/ status=kNotSubscribed }",
      PrintToString(metadata.RunAndGetResult()));
}

// Call FindWebFeedInfoForPage for a web feed which is unknown, but after a
// successful subscription. This covers using FindWebFeedInfoForPage when a
// model is loaded.
TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageNotSubscribedAfterSubscribe) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(
      MakeWebFeedPageInformation("https://www.catfood.com"), metadata.Bind());

  EXPECT_EQ("WebFeedMetadata{ status=kNotSubscribed }",
            PrintToString(metadata.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageRecommendedAndNotSubscribed) {
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("catfood")});
  CreateStream();

  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(
      MakeWebFeedPageInformation("https://yummy.catfood.com"), metadata.Bind());
  WebFeedMetadata result = metadata.RunAndGetResult();

  EXPECT_EQ(
      "WebFeedMetadata{ id=id_catfood is_recommended title=Title catfood "
      "publisher_url=https://catfood.com/ status=kNotSubscribed }",
      PrintToString(metadata.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageRecommendedSubscribeInProgress) {
  network_.SendResponsesOnCommand(true);
  network_.InjectResponse(SuccessfulFollowResponse("catfood"));
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("catfood")});
  CreateStream();
  WebFeedPageInformation page_info;
  page_info.SetUrl(GURL("https://catfood.com"));

  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(page_info, WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());

  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(page_info, metadata.Bind());

  EXPECT_EQ(
      "WebFeedMetadata{ id=id_catfood is_recommended title=Title catfood "
      "publisher_url=https://catfood.com/ status=kSubscribeInProgress }",
      PrintToString(metadata.RunAndGetResult()));
}

// Check FindWebFeedInfo*() for a web feed which is currently in the process of
// being subscribed.
TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageNonRecommendedSubscribeInProgress) {
  network_.SendResponsesOnCommand(true);
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("https://cats.com");

  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(page_info, WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  // Check status during subscribe process.
  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForPage(
        MakeWebFeedPageInformation("https://cats.com#fragments-are-ignored"),
        metadata.Bind());

    EXPECT_EQ("WebFeedMetadata{ status=kSubscribeInProgress }",
              PrintToString(metadata.RunAndGetResult()));
  }

  // Complete subscription and check status.
  network_.SendResponse();
  follow_callback.RunUntilCalled();

  // Check status with WebFeedPageInformation.
  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForPage(page_info, metadata.Bind());
    EXPECT_EQ(
        "WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kSubscribed }",
        PrintToString(metadata.RunAndGetResult()));
  }
  // Check status with WebFeedId.
  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());
    EXPECT_EQ(
        "WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kSubscribed }",
        PrintToString(metadata.RunAndGetResult()));
  }
}

// Check that FindWebFeedInfoForWebFeedId() returns information about a web feed
// which is currently in the process of being unsubscribed.
TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForWebFeedIdUnsubscribeInProgress) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  network_.SendResponsesOnCommand(true);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      /*is_durable_request=*/false, WebFeedChangeReason::WEB_PAGE_MENU,
      unfollow_callback.Bind());

  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());

    EXPECT_EQ(
        "WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kUnsubscribeInProgress }",
        PrintToString(metadata.RunAndGetResult()));
  }

  // Allow the unsubscribe request to complete, and check that the new status is
  // returned. Because the web feed was recently subscribed, additional details
  // about the web feed are returned.
  network_.SendResponse();
  unfollow_callback.RunUntilCalled();
  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());
    EXPECT_EQ(
        "WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kNotSubscribed }",
        PrintToString(metadata.RunAndGetResult()));
  }
}

TEST_F(FeedApiSubscriptionsTest, GetAllSubscriptionsWithNoSubscriptions) {
  CallbackReceiver<std::vector<WebFeedMetadata>> all_subscriptions;
  subscriptions().GetAllSubscriptions(all_subscriptions.Bind());

  EXPECT_EQ("{}", PrintToString(all_subscriptions.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest, GetAllSubscriptionsWithSomeSubscriptions) {
  // Set up two subscriptions, begin unsubscribing from one, and subscribing to
  // a third. Only subscribed web feeds are returned by GetAllSubscriptions.
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  network_.InjectResponse(SuccessfulFollowResponse("dogs"));
  network_.InjectResponse(SuccessfulFollowResponse("mice"));
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://dogs.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  network_.SendResponsesOnCommand(true);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  subscriptions().UnfollowWebFeed("id_dogs", /*is_durable_request=*/false,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  unfollow_callback.Bind());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://mice.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());

  CallbackReceiver<std::vector<WebFeedMetadata>> all_subscriptions;
  subscriptions().GetAllSubscriptions(all_subscriptions.Bind());

  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed }, "
      "WebFeedMetadata{ id=id_dogs title=Title dogs "
      "publisher_url=https://dogs.com/ status=kUnsubscribeInProgress } }",
      PrintToString(all_subscriptions.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest,
       RecommendedWebFeedsAreNotFetchedAfterStartupWhenFeatureIsDisabled) {
  // Set to a non-launched country to disable web feed feature.
  SetCountry("FR");

  SetUpWithDefaultConfig();

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(0, network_.GetListRecommendedWebFeedsRequestCount());

  // Restore the country.
  SetCountry("US");
}

TEST_F(
    FeedApiSubscriptionsTest,
    RecommendedAndFollowedWebFeedsAreNotFetchedAfterStartupWhenFeedIsDisabled) {
  profile_prefs_.SetBoolean(feed::prefs::kEnableSnippets, false);
  SetUpWithDefaultConfig();

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  EXPECT_EQ(0, network_.GetListRecommendedWebFeedsRequestCount());
  EXPECT_EQ(0, network_.GetListFollowedWebFeedsRequestCount());
}

TEST_F(FeedApiSubscriptionsTest, RecommendedWebFeedsAreFetchedAfterStartup) {
  SetUpWithDefaultConfig();
  InjectRecommendedWebFeedsResponse({MakeWireWebFeed("cats")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());

  // Ensure the new recommended feed information is immediately available.
  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(
      MakeWebFeedPageInformation("https://cats.com"), metadata.Bind());
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_cats is_recommended title=Title cats "
      "publisher_url=https://cats.com/ status=kNotSubscribed }",
      PrintToString(metadata.RunAndGetResult()));
  // Check that recommended feeds are exactly as expected, and persisted.
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats is_recommended title=Title cats "
      "publisher_url=https://cats.com/ status=kNotSubscribed } }",
      PrintToString(CheckRecommendedFeeds()));
}

TEST_F(FeedApiSubscriptionsTest, RecommendedWebFeedsAreClearedOnSignOut) {
  // 1. Populate web feeds at startup for a signed-in users.
  {
    SetUpWithDefaultConfig();
    InjectRecommendedWebFeedsResponse({MakeWireWebFeed("cats")});

    // Wait until the delayed task runs, and verify the network request was
    // sent.
    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
    ASSERT_EQ(
        "{ WebFeedMetadata{ id=id_cats is_recommended title=Title cats "
        "publisher_url=https://cats.com/ status=kNotSubscribed } }",
        PrintToString(GetRecommendedFeeds()));
  }

  // Sign out, and verify recommended web feeds are cleared.
  account_info_ = {};
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
  EXPECT_EQ("{}", PrintToString(CheckRecommendedFeeds()));
}

TEST_F(FeedApiSubscriptionsTest,
       RecommendedWebFeedsAreFetchedAfterSignInButNotSignOut) {
  SetUpWithDefaultConfig();
  InjectRecommendedWebFeedsResponse({MakeWireWebFeed("cats")});
  InjectRecommendedWebFeedsResponse({MakeWireWebFeed("dogs")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());

  // Sign out, this clears recommended Web Feeds.
  account_info_ = {};
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();

  // Sign in, and verify web feeds are fetched and stored.
  account_info_ = TestAccountInfo();
  stream_->OnSignedIn();
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.GetListRecommendedWebFeedsRequestCount());
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_dogs is_recommended title=Title dogs "
      "publisher_url=https://dogs.com/ status=kNotSubscribed } }",
      PrintToString(CheckRecommendedFeeds()));
}

TEST_F(FeedApiSubscriptionsTest,
       RecommendedWebFeedsAreFetchedAfterStartupOnlyWhenStale) {
  // 1. First, fetch recommended web feeds at startup, same as
  // RecommendedWebFeedsAreFetchedAfterStartup.
  {
    SetUpWithDefaultConfig();
    InjectRecommendedWebFeedsResponse({MakeWireWebFeed("cats")});

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
  }

  // 2. Recreate FeedStream, and verify recommended web feeds are not fetched
  // again.
  {
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
  }

  // 3. Wait until the data is stale, and then verify the recommended web feeds
  // are fetched again.
  {
    task_environment_.FastForwardBy(
        GetFeedConfig().recommended_feeds_staleness_threshold);
    InjectRecommendedWebFeedsResponse({MakeWireWebFeed("catsv2")});
    base::HistogramTester histograms;
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(2, network_.GetListRecommendedWebFeedsRequestCount());
    EXPECT_EQ(
        "{ WebFeedMetadata{ id=id_catsv2 is_recommended title=Title catsv2 "
        "publisher_url=https://catsv2.com/ status=kNotSubscribed } }",
        PrintToString(CheckRecommendedFeeds()));
    histograms.ExpectUniqueSample(
        "ContentSuggestions.Feed.WebFeed.RefreshRecommendedFeeds",
        WebFeedRefreshStatus::kSuccess, 1);
  }
}

TEST_F(FeedApiSubscriptionsTest,
       SubscribedWebFeedsAreNotFetchedAfterStartupWhenFeatureIsDisabled) {
  // Set to a non-launched country to disable web feed feature.
  SetCountry("FR");

  SetUpWithDefaultConfig();

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(0, network_.GetListFollowedWebFeedsRequestCount());

  // Restore the country.
  SetCountry("US");
}

TEST_F(FeedApiSubscriptionsTest, SubscribedWebFeedsAreFetchedAfterStartup) {
  SetUpWithDefaultConfig();
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());

  // Ensure the new subscribed feed information is immediately available.
  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(
      MakeWebFeedPageInformation("https://cats.com"), metadata.Bind());
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed }",
      PrintToString(metadata.RunAndGetResult()));
  // Check that subscribed feeds are exactly as expected, and persisted.
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, SubscribedWebFeedsAreClearedOnSignOut) {
  // Populate web feeds at startup for a signed-in users.
  {
    SetUpWithDefaultConfig();
    network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

    // Wait until the delayed task runs, and verify the network request was
    // sent.
    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
    ASSERT_EQ(
        "{ WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kSubscribed } }",
        PrintToString(CheckAllSubscriptions()));
  }

  // Sign out, and verify recommended web feeds are cleared.
  account_info_ = {};
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest,
       SubscribedWebFeedsAreFetchedAfterSignInButNotSignOut) {
  SetUpWithDefaultConfig();
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("dogs")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());

  // Sign out, and verify no web feeds are fetched.
  account_info_ = {};
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));

  // Sign in, and verify web feeds are fetched and stored.
  account_info_ = TestAccountInfo();
  stream_->OnSignedIn();
  WaitForIdleTaskQueue();

  ASSERT_EQ(2, network_.GetListFollowedWebFeedsRequestCount());
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_dogs title=Title dogs "
      "publisher_url=https://dogs.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest,
       SubscribedWebFeedsAreFetchedAfterStartupOnlyWhenStale) {
  // 1. First, fetch subscribed web feeds at startup, same as
  // SubscribedWebFeedsAreFetchedAfterStartup.
  {
    SetUpWithDefaultConfig();
    network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  }

  // 2. Recreate FeedStream, and verify subscribed web feeds are not fetched
  // again.
  {
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  }

  // 3. Wait until the data is stale, and then verify the subscribed web feeds
  // are fetched again.
  {
    task_environment_.FastForwardBy(
        GetFeedConfig().subscribed_feeds_staleness_threshold);
    network_.InjectListWebFeedsResponse({MakeWireWebFeed("catsv2")});
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::Seconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(2, network_.GetListFollowedWebFeedsRequestCount());
    EXPECT_EQ(
        "{ WebFeedMetadata{ id=id_catsv2 title=Title catsv2 "
        "publisher_url=https://catsv2.com/ status=kSubscribed } }",
        PrintToString(CheckAllSubscriptions()));
  }
}

TEST_F(FeedApiSubscriptionsTest, RefreshSubscriptionsSuccess) {
  {
    auto metadata = stream_->GetMetadata();
    metadata.set_consistency_token("token");
    stream_->SetMetadata(metadata);
  }
  base::HistogramTester histograms;
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> result;
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  subscriptions().RefreshSubscriptions(result.Bind());

  WaitForIdleTaskQueue();

  EXPECT_TRUE(result.RunAndGetResult().success);
  EXPECT_EQ("token", network_.GetApiRequestSent<ListWebFeedsDiscoverApi>()
                         ->consistency_token()
                         .token());
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
  histograms.ExpectUniqueSample(
      "ContentSuggestions.Feed.WebFeed.RefreshSubscribedFeeds.Force",
      WebFeedRefreshStatus::kSuccess, 1);
}

TEST_F(FeedApiSubscriptionsTest, RefreshSubscriptionsFail) {
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> result;

  network_.InjectApiRawResponse<ListWebFeedsDiscoverApi>(MakeFailedResponse());
  subscriptions().RefreshSubscriptions(result.Bind());

  WaitForIdleTaskQueue();

  EXPECT_FALSE(result.RunAndGetResult().success);
}

// Two calls to RefreshSubscriptions at the same time, so only one refresh
// occurs.
TEST_F(FeedApiSubscriptionsTest, RefreshSubscriptionsDuringRefresh) {
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> result1;
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> result2;
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  subscriptions().RefreshSubscriptions(result1.Bind());
  subscriptions().RefreshSubscriptions(result2.Bind());

  WaitForIdleTaskQueue();

  EXPECT_TRUE(result1.RunAndGetResult().success);
  EXPECT_TRUE(result2.RunAndGetResult().success);

  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, FetchRecommendedWebFeedsAbortOnClearAll) {
  // This makes sure that the model has been loaded first.
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed("cats", false,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());

  // Test task ordering: ClearAllTask, FetchRecommendedWebFeedsTask.
  stream_->OnCacheDataCleared();
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> callback;
  InjectRecommendedWebFeedsResponse({MakeWireWebFeed("cats")});
  subscriptions().RefreshRecommendedFeeds(callback.Bind());

  EXPECT_FALSE(callback.RunAndGetResult().success);
}

TEST_F(FeedApiSubscriptionsTest, FetchSubscribedWebFeedsRetryOnClearAll) {
  // This makes sure that the model has been loaded first.
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed("cats", false,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                follow_callback.Bind());

  // Test task ordering: ClearAllTask, FetchSubscribedWebFeedsTask, then retry.
  stream_->OnCacheDataCleared();
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> callback;
  network_.InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  subscriptions().RefreshSubscriptions(callback.Bind());

  EXPECT_TRUE(callback.RunAndGetResult().success);
}

TEST_F(FeedApiSubscriptionsTest, FieldTrialRegistered_OneFollow) {
  // Follow one web feed, and recreate FeedStream to simulate a Chrome restart.
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());

  WaitForIdleTaskQueue();
  CreateStream();

  // RegisterFollowingFeedFollowCountFieldTrial is called twice, one before and
  // one after CreateStream().
  EXPECT_EQ(std::vector<size_t>({0, 1}),
            register_following_feed_follow_count_field_trial_calls_);
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedDurableSuccess) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed("cats", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", PrintToString(GetAllPendingOperations()));
  EXPECT_EQ("Subscribed to id_cats\n",
            subscriptions().DescribeStateForTesting());
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedDurable_RemainsAfterFailure) {
  network_.InjectFollowResponse(MakeFailedResponse());
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed("id_cats", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());

  // The request fails, but we retain the pending operation.
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("Pending SUBSCRIBE id_cats attempts=1\n",
            subscriptions().DescribeStateForTesting());
  EXPECT_EQ(WebFeedSubscriptionStatus::kSubscribeInProgress,
            FindWebFeedInfoForWebFeedIdSync("id_cats").subscription_status);
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, UnfollowWebFeedDurable_RemainsAfterFailure) {
  SetupWithSubscriptions({MakeWireWebFeed("cats")});

  network_.InjectUnfollowResponse(MakeFailedResponse());
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult> callback;
  subscriptions().UnfollowWebFeed("id_cats", /*is_durable_request=*/true,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  callback.Bind());

  // The request fails, but we retain the pending operation.
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(
      "Pending UNSUBSCRIBE id_cats attempts=1\n"
      "Subscribed to id_cats\n",
      subscriptions().DescribeStateForTesting());
  EXPECT_EQ(WebFeedSubscriptionStatus::kUnsubscribeInProgress,
            FindWebFeedInfoForWebFeedIdSync("id_cats").subscription_status);
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedDurable_AbortsPreviousDurable) {
  SetupWithSubscriptions({MakeWireWebFeed("cats")});

  network_.InjectUnfollowResponse(MakeFailedResponse());
  network_.InjectFollowResponse(MakeFailedResponse());

  subscriptions().UnfollowWebFeed("id_cats", /*is_durable_request=*/true,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
  WaitForIdleTaskQueue();
  subscriptions().FollowWebFeed("id_cats", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  // The Follow request aborted the Unfollow request, so there are no pending
  // requests.
  EXPECT_EQ("Subscribed to id_cats\n",
            subscriptions().DescribeStateForTesting());
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, RetryPendingOperations_Success) {
  SetupWithSubscriptions({MakeWireWebFeed("cats")});

  network_.InjectUnfollowResponse(MakeFailedResponse());
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().UnfollowWebFeed("id_cats", /*is_durable_request=*/true,
                                  WebFeedChangeReason::MANAGEMENT,
                                  base::DoNothing());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  network_.InjectResponse(SuccessfulUnfollowResponse());
  network_.InjectResponse(SuccessfulFollowResponse("dogs"));
  subscriptions().RetryPendingOperationsForTesting();
  WaitForIdleTaskQueue();
  EXPECT_EQ(
      network_.GetApiRequestSent<FollowWebFeedDiscoverApi>()->change_reason(),
      WebFeedChangeReason::WEB_PAGE_MENU);
  EXPECT_EQ(
      network_.GetApiRequestSent<UnfollowWebFeedDiscoverApi>()->change_reason(),
      WebFeedChangeReason::MANAGEMENT);
  EXPECT_EQ("Subscribed to id_dogs\n",
            subscriptions().DescribeStateForTesting());
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, RetryPendingOperations_Failure) {
  SetupWithSubscriptions({MakeWireWebFeed("cats")});

  network_.InjectUnfollowResponse(MakeFailedResponse());
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().UnfollowWebFeed("id_cats", /*is_durable_request=*/true,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  network_.InjectUnfollowResponse(MakeFailedResponse());
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().RetryPendingOperationsForTesting();
  WaitForIdleTaskQueue();

  EXPECT_EQ(R"(Pending UNSUBSCRIBE id_cats attempts=2
Pending SUBSCRIBE id_dogs attempts=2
Subscribed to id_cats
)",
            subscriptions().DescribeStateForTesting());
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, RetryPendingOperations_ExceedsRetryLimit) {
  SetupWithSubscriptions({MakeWireWebFeed("cats")});

  for (int i = 0; i < WebFeedInFlightChange::kMaxDurableOperationAttempts + 1;
       ++i) {
    network_.InjectUnfollowResponse(MakeFailedResponse());
    network_.InjectFollowResponse(MakeFailedResponse());
  }

  subscriptions().UnfollowWebFeed("id_cats", /*is_durable_request=*/true,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  for (int i = 0; i < WebFeedInFlightChange::kMaxDurableOperationAttempts;
       ++i) {
    subscriptions().RetryPendingOperationsForTesting();
    WaitForIdleTaskQueue();
  }

  EXPECT_EQ(R"(Subscribed to id_cats
)",
            subscriptions().DescribeStateForTesting());
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, RetryPendingOperations_AbortsWhenOffline) {
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  is_offline_ = true;
  subscriptions().RetryPendingOperationsForTesting();
  WaitForIdleTaskQueue();

  // Only one attempt has been tried.
  EXPECT_EQ("Pending SUBSCRIBE id_dogs attempts=1\n",
            subscriptions().DescribeStateForTesting());
}

TEST_F(FeedApiSubscriptionsTest, RefreshSubscriptions_TriggersRetryPending) {
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  network_.InjectResponse(SuccessfulFollowResponse("dogs"));

  // Use RefreshSubscriptions(),
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> refresh_callback;
  network_.InjectListWebFeedsResponse(
      {MakeWireWebFeed("dogs"), MakeWireWebFeed("cats")});
  subscriptions().RefreshSubscriptions(refresh_callback.Bind());
  EXPECT_TRUE(refresh_callback.RunAndGetResult().success);

  // Make sure kListWebFeeds is last.
  EXPECT_EQ(std::vector<NetworkRequestType>({
                NetworkRequestType::kFollowWebFeed,
                NetworkRequestType::kFollowWebFeed,
                NetworkRequestType::kListWebFeeds,
            }),
            network_.sent_request_types());
  EXPECT_EQ(
      "Subscribed to id_dogs\n"
      "Subscribed to id_cats\n",
      subscriptions().DescribeStateForTesting());
}

TEST_F(FeedApiSubscriptionsTest, Startup_PendingRequestsAreEventuallyRetried) {
  // Fail a durable request so that there is a pending request stored.
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  // Simulate a restart, using the regular configuration that allows for delayed
  // tasks at startup.
  SetUpWithDefaultConfig();

  // Wait and verify that the follow request is retried.
  network_.InjectResponse(SuccessfulFollowResponse("dogs"));
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::Seconds(1));

  EXPECT_EQ("Subscribed to id_dogs\n",
            subscriptions().DescribeStateForTesting());
}

TEST_F(FeedApiSubscriptionsTest, PendingOperations_RemovedOnClearAll) {
  network_.InjectFollowResponse(MakeFailedResponse());
  subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/true,
                                WebFeedChangeReason::WEB_PAGE_MENU,
                                base::DoNothing());
  WaitForIdleTaskQueue();

  stream_->OnCacheDataCleared();
  WaitForIdleTaskQueue();

  EXPECT_EQ("", subscriptions().DescribeStateForTesting());
  CheckPendingOperationsAreStored();
}

TEST_F(FeedApiSubscriptionsTest, DataStoreReceivesFollowState) {
  WriteSubscribedFeeds(*store_, {MakeWebFeedInfo("food")});
  CreateStream();
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();
  EXPECT_THAT(surface.DescribeDataStoreUpdates(),
              testing::ElementsAre(
                  "write /app/webfeed-follow-state/id_food: FOLLOWED"));

  // Follow cats.
  {
    network_.InjectResponse(SuccessfulFollowResponse("cats"));
    subscriptions().FollowWebFeed("id_cats", /*is_durable_request=*/false,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
    WaitForIdleTaskQueue();
    EXPECT_THAT(
        surface.DescribeDataStoreUpdates(),
        testing::ElementsAre(
            "write /app/webfeed-follow-state/id_cats: FOLLOW_IN_PROGRESS",
            "write /app/webfeed-follow-state/id_cats: FOLLOWED"));
  }

  // Follow dogs.
  {
    network_.InjectResponse(SuccessfulFollowResponse("dogs"));
    subscriptions().FollowWebFeed("id_dogs", /*is_durable_request=*/false,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
    WaitForIdleTaskQueue();
    EXPECT_THAT(
        surface.DescribeDataStoreUpdates(),
        testing::ElementsAre(
            "write /app/webfeed-follow-state/id_dogs: FOLLOW_IN_PROGRESS",
            "write /app/webfeed-follow-state/id_dogs: FOLLOWED"));
  }

  // Unfollow dogs.
  {
    network_.InjectResponse(SuccessfulUnfollowResponse());
    subscriptions().UnfollowWebFeed("id_dogs", /*is_durable_request=*/false,
                                    WebFeedChangeReason::WEB_PAGE_MENU,
                                    base::DoNothing());
    WaitForIdleTaskQueue();
    EXPECT_THAT(
        surface.DescribeDataStoreUpdates(),
        testing::UnorderedElementsAre(
            "write /app/webfeed-follow-state/id_dogs: UNFOLLOW_IN_PROGRESS",
            "delete /app/webfeed-follow-state/id_dogs"));
  }

  // Unfollow cats unsuccessfully.
  {
    network_.InjectUnfollowResponse(MakeFailedResponse());
    subscriptions().UnfollowWebFeed("id_cats", /*is_durable_request=*/false,
                                    WebFeedChangeReason::WEB_PAGE_MENU,
                                    base::DoNothing());
    WaitForIdleTaskQueue();
    EXPECT_THAT(
        surface.DescribeDataStoreUpdates(),
        testing::ElementsAre(
            "write /app/webfeed-follow-state/id_cats: UNFOLLOW_IN_PROGRESS",
            "write /app/webfeed-follow-state/id_cats: FOLLOWED"));
  }

  // Follow fish unsuccessfully.
  {
    network_.InjectFollowResponse(MakeFailedResponse());
    subscriptions().FollowWebFeed("id_fish", /*is_durable_request=*/false,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
    WaitForIdleTaskQueue();
    EXPECT_THAT(
        surface.DescribeDataStoreUpdates(),
        testing::ElementsAre(
            "write /app/webfeed-follow-state/id_fish: FOLLOW_IN_PROGRESS",
            "delete /app/webfeed-follow-state/id_fish"));
  }

  // Refresh subscriptions from server: following cats and birds.
  {
    SetupWithSubscriptions({MakeWireWebFeed("cats"), MakeWireWebFeed("birds")});
    EXPECT_THAT(surface.DescribeDataStoreUpdates(),
                testing::UnorderedElementsAre(
                    "delete /app/webfeed-follow-state/id_food",
                    "write /app/webfeed-follow-state/id_birds: FOLLOWED"));
  }

  // Follow fish unsuccessfully, with a durable request.
  // Note that because durable requests are retried later, it can make injecting
  // the right network responses after this point difficult, so this should
  // probably be the last follow request.
  {
    network_.InjectFollowResponse(MakeFailedResponse());
    subscriptions().FollowWebFeed("id_fish", /*is_durable_request=*/true,
                                  WebFeedChangeReason::WEB_PAGE_MENU,
                                  base::DoNothing());
    WaitForIdleTaskQueue();
    EXPECT_THAT(
        surface.DescribeDataStoreUpdates(),
        testing::ElementsAre(
            "write /app/webfeed-follow-state/id_fish: FOLLOW_IN_PROGRESS"));
  }

  // Attach another surface, it should receive all data.
  {
    TestWebFeedSurface surface2(stream_.get());
    WaitForIdleTaskQueue();
    EXPECT_THAT(surface.DescribeDataStoreUpdates(), testing::IsEmpty());
    EXPECT_THAT(
        surface2.DescribeDataStoreUpdates(),
        testing::UnorderedElementsAre(
            "write /app/webfeed-follow-state/id_cats: FOLLOWED",
            "write /app/webfeed-follow-state/id_birds: FOLLOWED",
            "write /app/webfeed-follow-state/id_fish: FOLLOW_IN_PROGRESS"));
  }
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedBeforeFeedStreamInitialized) {
  CreateStream(/*wait_for_initialization=*/false);

  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("http://cats.com");
  page_info.SetRssUrls({GURL("http://rss1/"), GURL("http://rss2/")});
  subscriptions().FollowWebFeed(page_info, WebFeedChangeReason::WEB_PAGE_MENU,
                                callback.Bind());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
}

}  // namespace
}  // namespace test
}  // namespace feed
