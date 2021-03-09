// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_index.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

TEST(WebFeedIndex, FindWebFeedForUrlBeforePopulate) {
  WebFeedIndex index;
  EXPECT_EQ(WebFeedId(), index.FindWebFeedForUrl(GURL("http://foo")));
}

TEST(WebFeedIndex, FindWebFeedForUrlResolvesDomainsCorrectly) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    feed->add_uri_matchers()->set_domain_match("foo.com");
  }
  index.Populate(startup_data);

  // Matching URLs.
  const auto kId = WebFeedId::FromWebFeedId("id");
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("https://foo.com")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("http://foo.com")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("http://foo.com:1234")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("https://foo.com/bar")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("https://foo.com")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("https://baz.foo.com")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("https://baz.foo.com.")));
  EXPECT_EQ(kId, index.FindWebFeedForUrl(GURL("https://a.b.c.d.e.foo.com")));

  // Non-matching URLs.
  EXPECT_EQ(WebFeedId(), index.FindWebFeedForUrl(GURL("https://foo.com.br")));
  EXPECT_EQ(WebFeedId(),
            index.FindWebFeedForUrl(GURL("https://xyz.foo.com.z")));
  EXPECT_EQ(WebFeedId(),
            index.FindWebFeedForUrl(
                GURL("https://1000:1000:1000:0000:0000:0000:0000:0000")));
}

TEST(WebFeedIndex, PopulateOverwritesContent) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  auto* feed = startup_data.subscribed_web_feeds.add_feeds();
  feed->set_web_feed_id("id");
  feed->add_uri_matchers()->set_domain_match("foo.com");
  index.Populate(startup_data);
  feed->mutable_uri_matchers(0)->set_domain_match("boo.com");
  feed->set_web_feed_id("aid");
  index.Populate(startup_data);

  EXPECT_EQ(WebFeedId::FromWebFeedId("aid"),
            index.FindWebFeedForUrl(GURL("https://boo.com")));
  EXPECT_FALSE(index.FindWebFeedForUrl(GURL("https://foo.com")));
}

TEST(WebFeedIndex, FindWebFeedForUrlFindsRecommendedUrl) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  auto* feed = startup_data.recommended_feed_index.add_entries();
  feed->set_web_feed_id("id");
  feed->add_matchers()->set_domain_match("foo.com");
  index.Populate(startup_data);

  EXPECT_EQ(WebFeedId::FromWebFeedId("id"),
            index.FindWebFeedForUrl(GURL("https://foo.com")));
}

TEST(WebFeedIndex, FindWebFeedForUrlFindMoreSpecificFirst) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.recommended_feed_index.add_entries();
    feed->set_web_feed_id("foo");
    feed->add_matchers()->set_domain_match("foo.com");
  }
  {
    auto* feed = startup_data.recommended_feed_index.add_entries();
    feed->set_web_feed_id("barfoo");
    feed->add_matchers()->set_domain_match("bar.foo.com");
  }

  index.Populate(startup_data);

  EXPECT_EQ(WebFeedId::FromWebFeedId("barfoo"),
            index.FindWebFeedForUrl(GURL("https://bar.foo.com")));
  EXPECT_EQ(WebFeedId::FromWebFeedId("barfoo"),
            index.FindWebFeedForUrl(GURL("https://a.bar.foo.com")));
  EXPECT_EQ(WebFeedId::FromWebFeedId("foo"),
            index.FindWebFeedForUrl(GURL("https://foo.com")));
  EXPECT_EQ(WebFeedId::FromWebFeedId("foo"),
            index.FindWebFeedForUrl(GURL("https://baz.foo.com")));
}

TEST(WebFeedIndex, FindWebFeedForUrlFindsSubscribedFeedsPreferentially) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("sub-id");
    feed->add_uri_matchers()->set_domain_match("foo.com");
  }
  {
    auto* feed = startup_data.recommended_feed_index.add_entries();
    feed->set_web_feed_id("recommended-id");
    feed->add_matchers()->set_domain_match("foo.com");
  }
  index.Populate(startup_data);

  EXPECT_EQ(WebFeedId::FromWebFeedId("sub-id"),
            index.FindWebFeedForUrl(GURL("https://foo.com")));
}

TEST(WebFeedIndex, FindWebFeedForUrlFindsFeedWithNoWebFeedId) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_subscription_id("sub-id");
    feed->add_uri_matchers()->set_domain_match("foo.com");
  }
  index.Populate(startup_data);

  EXPECT_EQ(WebFeedId::FromFollowId("sub-id"),
            index.FindWebFeedForUrl(GURL("https://foo.com")));
}

}  // namespace
}  // namespace feed
