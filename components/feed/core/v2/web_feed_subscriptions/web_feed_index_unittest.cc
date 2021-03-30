// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

TEST(WebFeedIndex, FindWebFeedForUrlBeforePopulate) {
  WebFeedIndex index;
  EXPECT_EQ("", index.FindWebFeedForUrl(GURL("http://foo")).web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedForUrlResolvesDomainsCorrectly) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    feed->add_uri_matchers()->set_domain_match("foo.com");
  }
  index.Populate(startup_data.subscribed_web_feeds);

  // Matching URLs.
  EXPECT_EQ("id", index.FindWebFeedForUrl(GURL("https://foo.com")).web_feed_id);
  EXPECT_EQ("id", index.FindWebFeedForUrl(GURL("http://foo.com")).web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeedForUrl(GURL("http://foo.com:1234")).web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeedForUrl(GURL("https://foo.com/bar")).web_feed_id);
  EXPECT_EQ("id", index.FindWebFeedForUrl(GURL("https://foo.com")).web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeedForUrl(GURL("https://baz.foo.com")).web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeedForUrl(GURL("https://baz.foo.com.")).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeedForUrl(GURL("https://a.b.c.d.e.foo.com")).web_feed_id);

  // Non-matching URLs.
  EXPECT_EQ("",
            index.FindWebFeedForUrl(GURL("https://foo.com.br")).web_feed_id);
  EXPECT_EQ("",
            index.FindWebFeedForUrl(GURL("https://xyz.foo.com.z")).web_feed_id);
  EXPECT_EQ("", index
                    .FindWebFeedForUrl(
                        GURL("https://1000:1000:1000:0000:0000:0000:0000:0000"))
                    .web_feed_id);
}

TEST(WebFeedIndex, PopulateOverwritesContent) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  auto* feed = startup_data.subscribed_web_feeds.add_feeds();
  feed->set_web_feed_id("id");
  feed->add_uri_matchers()->set_domain_match("foo.com");
  index.Populate(startup_data.subscribed_web_feeds);
  feed->mutable_uri_matchers(0)->set_domain_match("boo.com");
  feed->set_web_feed_id("aid");
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ("aid",
            index.FindWebFeedForUrl(GURL("https://boo.com")).web_feed_id);
  EXPECT_EQ("", index.FindWebFeedForUrl(GURL("https://foo.com")).web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedForUrlFindsRecommendedUrl) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  auto* feed = startup_data.recommended_feed_index.add_entries();
  feed->set_web_feed_id("id");
  feed->add_matchers()->set_domain_match("foo.com");
  index.Populate(startup_data.recommended_feed_index);

  EXPECT_EQ("id", index.FindWebFeedForUrl(GURL("https://foo.com")).web_feed_id);
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

  index.Populate(startup_data.recommended_feed_index);

  EXPECT_EQ("barfoo",
            index.FindWebFeedForUrl(GURL("https://bar.foo.com")).web_feed_id);
  EXPECT_EQ("barfoo",
            index.FindWebFeedForUrl(GURL("https://a.bar.foo.com")).web_feed_id);
  EXPECT_EQ("foo",
            index.FindWebFeedForUrl(GURL("https://foo.com")).web_feed_id);
  EXPECT_EQ("foo",
            index.FindWebFeedForUrl(GURL("https://baz.foo.com")).web_feed_id);
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
  index.Populate(startup_data.recommended_feed_index);
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ("sub-id",
            index.FindWebFeedForUrl(GURL("https://foo.com")).web_feed_id);
}

}  // namespace
}  // namespace feed
