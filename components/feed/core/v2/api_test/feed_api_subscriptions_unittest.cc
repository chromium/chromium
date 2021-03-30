// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"

#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {
namespace {
constexpr int64_t kFollowerCount = 123;

WebFeedPageInformation MakeWebFeedPageInformation(const std::string& url) {
  WebFeedPageInformation info;
  info.url = GURL(url);
  return info;
}

feedwire::webfeed::WebFeed MakeWireWebFeed(const std::string& name) {
  feedwire::webfeed::WebFeed result;
  result.set_name("id_" + name);
  result.set_title("Title " + name);
  result.set_subtitle("Subtitle " + name);
  result.set_detail_text("details...");
  result.set_visit_uri("https://" + name + ".com");
  result.set_follower_count(kFollowerCount);
  result.add_uri_matchers()->set_domain_match(name + ".com");
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
    *entry->mutable_matchers() = info.uri_matchers();
  }

  store.WriteRecommendedFeeds(index, recommended_feeds, base::DoNothing());
}

class FeedApiSubscriptionsTest : public FeedApiTest {
 public:
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
    ([&]() {
      ASSERT_EQ(testing::PrintToString(result),
                testing::PrintToString(result2));
    })();
    return result;
  }

  WebFeedSubscriptionCoordinator& subscriptions() {
    return stream_->subscriptions();
  }
};

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedSuccess) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_cats title=Title cats "
      "publisher_url=https://cats.com/ status=kSubscribed }",
      testing::PrintToString(callback.RunAndGetResult().web_feed_metadata));
}

TEST_F(FeedApiSubscriptionsTest, FollowRecommendedWebFeedById) {
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("catfood")});
  CreateStream();
  network_.InjectFollowResponse(SuccessfulFollowResponse("catfood"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;
  subscriptions().FollowWebFeed("id_catfood", callback.Bind());
  EXPECT_EQ(
      "WebFeedMetadata{ id=id_catfood is_recommended title=Title catfood "
      "publisher_url=https://catfood.com/ status=kSubscribed }",
      testing::PrintToString(callback.RunAndGetResult().web_feed_metadata));
}

// Make two Follow attempts for the same page. Both appear successful, but only
// one network request is made.
TEST_F(FeedApiSubscriptionsTest, FollowWebFeedTwiceAtOnce) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
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
      testing::PrintToString(CheckAllSubscriptions()));
}

// Follow two different pages which resolve to the same web feed server-side.
TEST_F(FeedApiSubscriptionsTest, FollowWebFeedTwiceFromDifferentUrls) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
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
      testing::PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, FollowTwoWebFeedsAtOnce) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  network_.InjectFollowResponse(SuccessfulFollowResponse("dogs"));
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
      testing::PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, CantFollowWebFeedWhileOffline) {
  is_offline_ = true;
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback.Bind());

  EXPECT_EQ(0, network_.GetFollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedOffline,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", testing::PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, FollowWebFeedNetworkError) {
  network_.InjectFollowResponse(MakeFailedResponse());
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> callback;

  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                callback.Bind());

  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kFailedUnknownError,
            callback.RunAndGetResult().request_status);
  EXPECT_EQ("{}", testing::PrintToString(CheckAllSubscriptions()));
}

// Follow and then unfollow a web feed successfully.
TEST_F(FeedApiSubscriptionsTest, UnfollowAFollowedWebFeed) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      unfollow_callback.Bind());

  unfollow_callback.RunUntilCalled();
  EXPECT_EQ(1, network_.GetUnfollowRequestCount());
  EXPECT_EQ(WebFeedSubscriptionRequestStatus::kSuccess,
            unfollow_callback.GetResult()->request_status);
  EXPECT_EQ("{}", testing::PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowAFollowedWebFeedTwiceAtOnce) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback1;
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback2;
  network_.InjectUnfollowResponse(SuccessfulUnfollowResponse());
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
  EXPECT_EQ("{}", testing::PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowNetworkFailure) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(MakeFailedResponse());
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
      testing::PrintToString(CheckAllSubscriptions()));
}

TEST_F(FeedApiSubscriptionsTest, UnfollowWhileOffline) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
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
  network_.InjectUnfollowResponse(SuccessfulUnfollowResponse());
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
            testing::PrintToString(metadata.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest, FindWebFeedInfoForWebFeedIdNotSubscribed) {
  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());

  EXPECT_EQ("WebFeedMetadata{ id=id_cats status=kNotSubscribed }",
            testing::PrintToString(metadata.RunAndGetResult()));
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
      testing::PrintToString(metadata.RunAndGetResult()));
}

// Call FindWebFeedInfoForPage for a web feed which is unknown, but after a
// successful subscription. This covers using FindWebFeedInfoForPage when a
// model is loaded.
TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageNotSubscribedAfterSubscribe) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(
      MakeWebFeedPageInformation("https://www.catfood.com"), metadata.Bind());

  EXPECT_EQ("WebFeedMetadata{ status=kNotSubscribed }",
            testing::PrintToString(metadata.RunAndGetResult()));
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
      testing::PrintToString(metadata.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageRecommendedSubscribeInProgress) {
  network_.SendResponsesOnCommand(true);
  network_.InjectFollowResponse(SuccessfulFollowResponse("catfood"));
  WriteRecommendedFeeds(*store_, {MakeWebFeedInfo("catfood")});
  CreateStream();
  WebFeedPageInformation page_info;
  page_info.url = GURL("https://catfood.com");

  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(page_info, follow_callback.Bind());

  CallbackReceiver<WebFeedMetadata> metadata;
  subscriptions().FindWebFeedInfoForPage(page_info, metadata.Bind());

  EXPECT_EQ(
      "WebFeedMetadata{ id=id_catfood is_recommended title=Title catfood "
      "publisher_url=https://catfood.com/ status=kSubscribeInProgress }",
      testing::PrintToString(metadata.RunAndGetResult()));
}

// Check FindWebFeedInfo*() for a web feed which is currently in the process of
// being subscribed.
TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForPageNonRecommendedSubscribeInProgress) {
  network_.SendResponsesOnCommand(true);
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  WebFeedPageInformation page_info =
      MakeWebFeedPageInformation("https://cats.com");

  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(page_info, follow_callback.Bind());
  // Check status during subscribe process.
  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForPage(page_info, metadata.Bind());

    EXPECT_EQ("WebFeedMetadata{ status=kSubscribeInProgress }",
              testing::PrintToString(metadata.RunAndGetResult()));
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
        testing::PrintToString(metadata.RunAndGetResult()));
  }
  // Check status with WebFeedId.
  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());
    EXPECT_EQ(
        "WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kSubscribed }",
        testing::PrintToString(metadata.RunAndGetResult()));
  }
}

// Check that FindWebFeedInfoForWebFeedId() returns information about a web feed
// which is currently in the process of being unsubscribed.
TEST_F(FeedApiSubscriptionsTest,
       FindWebFeedInfoForWebFeedIdUnsubscribeInProgress) {
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  CallbackReceiver<WebFeedSubscriptions::FollowWebFeedResult> follow_callback;
  subscriptions().FollowWebFeed(MakeWebFeedPageInformation("http://cats.com"),
                                follow_callback.Bind());
  follow_callback.RunUntilCalled();

  network_.SendResponsesOnCommand(true);
  CallbackReceiver<WebFeedSubscriptions::UnfollowWebFeedResult>
      unfollow_callback;
  network_.InjectUnfollowResponse(SuccessfulUnfollowResponse());
  subscriptions().UnfollowWebFeed(
      follow_callback.GetResult()->web_feed_metadata.web_feed_id,
      unfollow_callback.Bind());

  {
    CallbackReceiver<WebFeedMetadata> metadata;
    subscriptions().FindWebFeedInfoForWebFeedId("id_cats", metadata.Bind());

    EXPECT_EQ(
        "WebFeedMetadata{ id=id_cats title=Title cats "
        "publisher_url=https://cats.com/ status=kUnsubscribeInProgress }",
        testing::PrintToString(metadata.RunAndGetResult()));
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
        testing::PrintToString(metadata.RunAndGetResult()));
  }
}

TEST_F(FeedApiSubscriptionsTest, GetAllSubscriptionsWithNoSubscriptions) {
  CallbackReceiver<std::vector<WebFeedMetadata>> all_subscriptions;
  subscriptions().GetAllSubscriptions(all_subscriptions.Bind());

  EXPECT_EQ("{}", testing::PrintToString(all_subscriptions.RunAndGetResult()));
}

TEST_F(FeedApiSubscriptionsTest, GetAllSubscriptionsWithSomeSubscriptions) {
  // Set up two subscriptions, begin unsubscribing from one, and subscribing to
  // a third. Only subscribed web feeds are returned by GetAllSubscriptions.
  network_.InjectFollowResponse(SuccessfulFollowResponse("cats"));
  network_.InjectFollowResponse(SuccessfulFollowResponse("dogs"));
  network_.InjectFollowResponse(SuccessfulFollowResponse("mice"));
  network_.InjectUnfollowResponse(SuccessfulUnfollowResponse());
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
      testing::PrintToString(all_subscriptions.RunAndGetResult()));
}

}  // namespace
}  // namespace test
}  // namespace feed
