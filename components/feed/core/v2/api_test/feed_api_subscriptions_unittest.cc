// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"

#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/feed/feed_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {
namespace {
using testing::PrintToString;
constexpr int64_t kFollowerCount = 123;

WebFeedPageInformation MakeWebFeedPageInformation(const std::string& url) {
  WebFeedPageInformation info;
  info.SetUrl(GURL(url));
  return info;
}

feedwire::webfeed::WebFeedMatcher MakeDomainMatcher(const std::string& domain) {
  feedwire::webfeed::WebFeedMatcher result;
  feedwire::webfeed::WebFeedMatcher::Criteria* criteria = result.add_criteria();
  criteria->set_criteria_type(
      feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX);
  criteria->set_text(domain);
  return result;
}

feedwire::webfeed::WebFeed MakeWireWebFeed(const std::string& name) {
  feedwire::webfeed::WebFeed result;
  result.set_name("id_" + name);
  result.set_title("Title " + name);
  result.set_subtitle("Subtitle " + name);
  result.set_detail_text("details...");
  result.set_visit_uri("https://" + name + ".com");
  result.set_follower_count(kFollowerCount);
  *result.add_web_feed_matchers() = MakeDomainMatcher(name + ".com");
  return result;
}

feedwire::webfeed::FollowWebFeedResponse SuccessfulFollowResponse(
    const std::string& follow_name) {
  feedwire::webfeed::FollowWebFeedResponse response;
  *response.mutable_web_feed() = MakeWireWebFeed(follow_name);
  return response;
}

feedwire::webfeed::UnfollowWebFeedResponse SuccessfulUnfollowResponse() {
  return {};
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

class FeedApiSubscriptionsTest : public FeedApiTest {
 public:
  void SetUp() override {
    subscription_feature_list_.InitAndEnableFeature(kWebFeed);
    FeedApiTest::SetUp();
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

  void InjectRecommendedWebFeedsResponse(
      std::vector<feedwire::webfeed::WebFeed> web_feeds) {
    feedwire::webfeed::ListRecommendedWebFeedsResponse response;
    for (const auto& feed : web_feeds) {
      *response.add_recommended_web_feeds() = feed;
    }
    network_.InjectResponse(response);
  }

  void InjectListWebFeedsResponse(
      std::vector<feedwire::webfeed::WebFeed> web_feeds) {
    feedwire::webfeed::ListWebFeedsResponse response;
    for (const auto& feed : web_feeds) {
      *response.add_web_feeds() = feed;
    }
    network_.InjectResponse(response);
  }

  WebFeedSubscriptionCoordinator& subscriptions() {
    return stream_->subscriptions();
  }

 private:
  base::test::ScopedFeatureList subscription_feature_list_;
};

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedSuccess) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed }",
      PrintToString(callback.RunAndGetResult().web_feed_metadata));
  EXPECT_TRUE(feedstore::IsKnownStale(stream_->GetMetadata(), kWebFeedStream));
}

TEST_F(FeedApiSubscriptionsTest, FollowRecommendedWebFeedById) {
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("catfood")});
  CreateStream();
  network_.InjectResponse(SuccessfulFollowResponse("catfood"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  subscriptions().FollowWebFeed("id_catfood", callback.Bind());
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_catfood is_recommended title=Title catfood "
      "publisher_url=https://catfood.com/ status=kSubscribed }",
      PrintToString(callback.RunAndGetResult().web_feed_metadata));
}

// Make two Follow attempts for the same page. Both appear successful, but only
// one network request is made.
TEST_F(FeedApiSubscriptionsTest, FollowWebFeedTwiceAtOnce) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback2;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback.Bind());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback2.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback2.RunAndGetResult().request_status);
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
                                callback.Bind());
  subscriptions().FollowWebFeed(
      MakeWebFeedPageInformation("http://cool-cats.com"), callback2.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback2.RunAndGetResult().request_status);
  EXPECT_EQ(2, network_.GetFollowRequestCount());
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
                                callback.Bind());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://dogs.com"),
                                callback2.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback2.RunAndGetResult().request_status);
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
                                callback.Bind());

  EXPECT_EQ(0, network_.GetFollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedOffline,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedNetworkError) {
  network_.InjectFollowResponse(MakeFailedResponse());
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  EXPECT_FALSE(feedstore::IsKnownStale(stream_->GetMetadata(), kWebFeedStream));

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
  EXPECT_FALSE(feedstore::IsKnownStale(stream_->GetMetadata(), kWebFeedStream));
}

// Follow and then unfollow a web feed successfully.
TEST_F(FeedApiSubscriptionsTest, UnfollowAFollowedWebFeed) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();
  // Un-mark stream as stale, to verify unsubscribe also marks stream as stale.
  stream_->SetStreamStale(kWebFeedStream, false);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(1, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            unfollow_callback.GetResult()->request_status);
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
  EXPECT_TRUE(feedstore::IsKnownStale(stream_->GetMetadata(), kWebFeedStream));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowAFollowedWebFeedTwiceAtOnce) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback1;
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback2;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      unfollow_callback1.Bind());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
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
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(MakeFailedResponse());
  // Un-mark stream as stale, to verify unsubscribe also marks stream as stale.
  stream_->SetStreamStale(kWebFeedStream, false);
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(1, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            unfollow_callback.GetResult()->request_status);
  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
  EXPECT_FALSE(feedstore::IsKnownStale(stream_->GetMetadata(), kWebFeedStream));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowWhileOffline) {
  network_.InjectResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  is_offline_ = true;

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(MakeFailedResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
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
  subscriptions().UnfollowWebFeed("notfollowed", unfollow_callback.Bind());

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
  subscriptions().FollowWebFeed(page_info, follow_callback.Bind());

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
  subscriptions().FollowWebFeed(page_info, follow_callback.Bind());
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
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  network_.SendResponsesOnCommand(true);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
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
                                base::DoNothing());
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://dogs.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  network_.SendResponsesOnCommand(true);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  subscriptions().UnfollowWebFeed("id_dogs", unfollow_callback.Bind());
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://mice.com"),
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
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kWebFeed);

  SetUpWithDefaultConfig();

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::TimeDelta::FromSeconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(0, network_.GetListRecommendedWebFeedsRequestCount());
}

TEST_F(
    FeedApiSubscriptionsTest,
    RecommendedAndFollowedWebFeedsAreNotFetchedAfterStartupWhenFeedIsDisabled) {
  profile_prefs_.SetBoolean(feed::prefs::kEnableSnippets, false);
  SetUpWithDefaultConfig();

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::TimeDelta::FromSeconds(1));
  WaitForIdleTaskQueue();
  EXPECT_EQ(0, network_.GetListRecommendedWebFeedsRequestCount());
  EXPECT_EQ(0, network_.GetListFollowedWebFeedsRequestCount());
}

TEST_F(FeedApiSubscriptionsTest, RecommendedWebFeedsAreFetchedAfterStartup) {
  SetUpWithDefaultConfig();
  InjectRecommendedWebFeedsResponse({MakeWireWebFeed("cats")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::TimeDelta::FromSeconds(1));
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
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
    ASSERT_EQ(
        "{ WebFeedMetadata{ id=id_cats is_recommended title=Title cats "
        "publisher_url=https://cats.com/ status=kNotSubscribed } }",
        PrintToString(GetRecommendedFeeds()));
  }

  // Sign out, and verify recommended web feeds are cleared.
  signed_in_gaia_ = "";
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
                                  base::TimeDelta::FromSeconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());

  // Sign out, this clears recommended Web Feeds.
  signed_in_gaia_ = "";
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();

  // Sign in, and verify web feeds are fetched and stored.
  signed_in_gaia_ = "examplegaia";
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
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
  }

  // 2. Recreate FeedStream, and verify recommended web feeds are not fetched
  // again.
  {
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListRecommendedWebFeedsRequestCount());
  }

  // 3. Wait until the data is stale, and then verify the recommended web feeds
  // are fetched again.
  {
    task_environment_.FastForwardBy(
        GetFeedConfig().recommended_feeds_staleness_threshold);
    InjectRecommendedWebFeedsResponse({MakeWireWebFeed("catsv2")});
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(2, network_.GetListRecommendedWebFeedsRequestCount());
    EXPECT_EQ(
        "{ WebFeedMetadata{ id=id_catsv2 is_recommended title=Title catsv2 "
        "publisher_url=https://catsv2.com/ status=kNotSubscribed } }",
        PrintToString(CheckRecommendedFeeds()));
  }
}

TEST_F(FeedApiSubscriptionsTest,
       SubscribedWebFeedsAreNotFetchedAfterStartupWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(kWebFeed);

  SetUpWithDefaultConfig();

  // Wait until the delayed task would normally run, verify no request is made.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::TimeDelta::FromSeconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(0, network_.GetListFollowedWebFeedsRequestCount());
}

TEST_F(FeedApiSubscriptionsTest, SubscribedWebFeedsAreFetchedAfterStartup) {
  SetUpWithDefaultConfig();
  InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::TimeDelta::FromSeconds(1));
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
  EXPECT_TRUE(subscriptions().IsWebFeedSubscriber());
}

TEST_F(FeedApiSubscriptionsTest, SubscribedWebFeedsAreClearedOnSignOut) {
  // Populate web feeds at startup for a signed-in users.
  {
    SetUpWithDefaultConfig();
    InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

    // Wait until the delayed task runs, and verify the network request was
    // sent.
    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
    ASSERT_EQ(
        "{ WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kSubscribed } }",
        PrintToString(CheckAllSubscriptions()));
  }

  // Sign out, and verify recommended web feeds are cleared.
  signed_in_gaia_ = "";
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));
  EXPECT_FALSE(subscriptions().IsWebFeedSubscriber());
}

TEST_F(FeedApiSubscriptionsTest,
       SubscribedWebFeedsAreFetchedAfterSignInButNotSignOut) {
  SetUpWithDefaultConfig();
  InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  InjectListWebFeedsResponse({MakeWireWebFeed("dogs")});

  // Wait until the delayed task runs, and verify the network request was sent.
  task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                  base::TimeDelta::FromSeconds(1));
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());

  // Sign out, and verify no web feeds are fetched.
  signed_in_gaia_ = "";
  stream_->OnSignedOut();
  WaitForIdleTaskQueue();
  ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  EXPECT_EQ("{}", PrintToString(CheckAllSubscriptions()));

  // Sign in, and verify web feeds are fetched and stored.
  signed_in_gaia_ = "examplegaia";
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
    InjectListWebFeedsResponse({MakeWireWebFeed("cats")});

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  }

  // 2. Recreate FeedStream, and verify subscribed web feeds are not fetched
  // again.
  {
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(1, network_.GetListFollowedWebFeedsRequestCount());
  }

  // 3. Wait until the data is stale, and then verify the subscribed web feeds
  // are fetched again.
  {
    task_environment_.FastForwardBy(
        GetFeedConfig().subscribed_feeds_staleness_threshold);
    InjectListWebFeedsResponse({MakeWireWebFeed("catsv2")});
    CreateStream();

    task_environment_.FastForwardBy(GetFeedConfig().fetch_web_feed_info_delay +
                                    base::TimeDelta::FromSeconds(1));
    WaitForIdleTaskQueue();
    ASSERT_EQ(2, network_.GetListFollowedWebFeedsRequestCount());
    EXPECT_EQ(
        "{ WebFeedMetadata{ id=id_catsv2 title=Title catsv2 "
        "publisher_url=https://catsv2.com/ status=kSubscribed } }",
        PrintToString(CheckAllSubscriptions()));
  }
}

TEST_F(FeedApiSubscriptionsTest, RefreshSubscriptionsSuccess) {
  CallbackReceiver<WebFeedSubscriptions::RefreshResult> result;
  InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
  subscriptions().RefreshSubscriptions(result.Bind());

  WaitForIdleTaskQueue();

  EXPECT_TRUE(result.RunAndGetResult().success);

  EXPECT_EQ(
      "{ WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed } }",
      PrintToString(CheckAllSubscriptions()));
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
  InjectListWebFeedsResponse({MakeWireWebFeed("cats")});
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

}  // namespace
}  // namespace test
}  // namespace feed
