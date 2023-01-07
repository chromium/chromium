// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/web_feed_index.h"

#include "components/feed/core/proto/v2/wire/web_feed_matcher.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {
using Criteria = feedwire::webfeed::WebFeedMatcher::Criteria;

WebFeedPageInformation MakePageInfo(const GURL& url,
                                    std::vector<GURL> rss_urls = {}) {
  WebFeedPageInformation page_info;
  page_info.SetUrl(url);
  page_info.SetRssUrls(rss_urls);
  return page_info;
}

feedwire::webfeed::WebFeedMatcher::Criteria TextCriteria(
    feedwire::webfeed::WebFeedMatcher::Criteria::CriteriaType criteria_type,
    const std::string& text) {
  feedwire::webfeed::WebFeedMatcher::Criteria criteria;
  criteria.set_criteria_type(criteria_type);
  criteria.set_text(text);
  return criteria;
}

feedwire::webfeed::WebFeedMatcher::Criteria RegexCriteria(
    feedwire::webfeed::WebFeedMatcher::Criteria::CriteriaType criteria_type,
    const std::string& regex) {
  feedwire::webfeed::WebFeedMatcher::Criteria criteria;
  criteria.set_criteria_type(criteria_type);
  criteria.set_partial_match_regex(regex);
  return criteria;
}

feedwire::webfeed::WebFeedMatcher MakeDomainMatcher(const std::string& domain) {
  feedwire::webfeed::WebFeedMatcher result;
  *result.add_criteria() = TextCriteria(Criteria::PAGE_URL_HOST_SUFFIX, domain);
  return result;
}

TEST(WebFeedIndex, FindWebFeedForUrlBeforePopulate) {
  WebFeedIndex index;
  EXPECT_EQ("",
            index.FindWebFeed(MakePageInfo(GURL("http://foo"))).web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedResolvesDomainsCorrectly) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    *feed->add_matchers() = MakeDomainMatcher("foo.com");
  }
  index.Populate(startup_data.subscribed_web_feeds);

  // Matching URLs.
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com"))).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("http://foo.com"))).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("http://foo.com:1234"))).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com/bar"))).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com"))).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://baz.foo.com"))).web_feed_id);
  EXPECT_EQ("id", index.FindWebFeed(MakePageInfo(GURL("https://baz.foo.com.")))
                      .web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeed(MakePageInfo(GURL("https://a.b.c.d.e.foo.com")))
                .web_feed_id);

  // Non-matching URLs.
  EXPECT_EQ(
      "",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com.br"))).web_feed_id);
  EXPECT_EQ(
      "",
      index.FindWebFeed(MakePageInfo(GURL("https://xfoo.com"))).web_feed_id);
  EXPECT_EQ("", index.FindWebFeed(MakePageInfo(GURL("https://xyz.foo.com.z")))
                    .web_feed_id);
  EXPECT_EQ("", index
                    .FindWebFeed(MakePageInfo(GURL(
                        "https://1000:1000:1000:0000:0000:0000:0000:0000")))
                    .web_feed_id);
}

TEST(WebFeedIndex, PopulateOverwritesContent) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  auto* feed = startup_data.subscribed_web_feeds.add_feeds();
  feed->set_web_feed_id("id");
  *feed->add_matchers() = MakeDomainMatcher("foo.com");
  index.Populate(startup_data.subscribed_web_feeds);
  *feed->mutable_matchers(0) = MakeDomainMatcher("boo.com");
  feed->set_web_feed_id("aid");
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ(
      "aid",
      index.FindWebFeed(MakePageInfo(GURL("https://boo.com"))).web_feed_id);
  EXPECT_EQ(
      "", index.FindWebFeed(MakePageInfo(GURL("https://foo.com"))).web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedForUrlFindsRecommendedUrl) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  auto* feed = startup_data.recommended_feed_index.add_entries();
  feed->set_web_feed_id("id");
  *feed->add_matchers() = MakeDomainMatcher("foo.com");
  index.Populate(startup_data.recommended_feed_index);

  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com"))).web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedWithPageInfoFindsSubscribedFeedsPreferentially) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("sub-id");
    *feed->add_matchers() = MakeDomainMatcher("foo.com");
  }
  {
    auto* feed = startup_data.recommended_feed_index.add_entries();
    feed->set_web_feed_id("recommended-id");
    *feed->add_matchers() = MakeDomainMatcher("foo.com");
  }
  index.Populate(startup_data.recommended_feed_index);
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ(
      "sub-id",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com"))).web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedCriteriaPageUrlHostMatch) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    *feed->add_matchers()->add_criteria() =
        RegexCriteria(Criteria::PAGE_URL_HOST_MATCH, "^[fb]oobar.com");
    *feed->add_matchers()->add_criteria() =
        TextCriteria(Criteria::PAGE_URL_HOST_MATCH, "baz.com");
  }
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ("id",
            index.FindWebFeed(MakePageInfo(GURL("https://foobar.com/path")))
                .web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeed(MakePageInfo(GURL("https://boobar.com/path")))
                .web_feed_id);
  EXPECT_EQ("id", index.FindWebFeed(MakePageInfo(GURL("https://baz.com/path")))
                      .web_feed_id);

  EXPECT_EQ("",
            index.FindWebFeed(MakePageInfo(GURL("https://sub.foobar.com/path")))
                .web_feed_id);
  EXPECT_EQ("",
            index.FindWebFeed(MakePageInfo(GURL("https://sub.baz.com/path")))
                .web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedCriteriaPathMatch) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    *feed->add_matchers()->add_criteria() =
        RegexCriteria(Criteria::PAGE_URL_PATH_MATCH, "[fb]oobar");
    *feed->add_matchers()->add_criteria() =
        TextCriteria(Criteria::PAGE_URL_PATH_MATCH, "/baz");
  }
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://test.com/fun-foobar-fun")))
          .web_feed_id);
  EXPECT_EQ("id",
            index.FindWebFeed(MakePageInfo(GURL("https://test.com/boobar")))
                .web_feed_id);
  EXPECT_EQ("id", index.FindWebFeed(MakePageInfo(GURL("https://test.com/baz")))
                      .web_feed_id);

  EXPECT_EQ("", index.FindWebFeed(MakePageInfo(GURL("https://test.com/path")))
                    .web_feed_id);

  EXPECT_EQ(
      "",
      index.FindWebFeed(MakePageInfo(GURL("https://test.com"))).web_feed_id);

  EXPECT_EQ("", index.FindWebFeed(MakePageInfo(GURL("https://foobar.com/path")))
                    .web_feed_id);

  EXPECT_EQ("", index.FindWebFeed(MakePageInfo(GURL("https://test.com/baz-")))
                    .web_feed_id);
}

TEST(WebFeedIndex, FindWebFeedCriteriaRssUrlMatch) {
  // Visited page URL doesn't matter for this test, because we're using RSS URL
  // match criteria.
  const GURL PAGE_URL = GURL("https://somepage");
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    *feed->add_matchers()->add_criteria() =
        RegexCriteria(Criteria::RSS_URL_MATCH, "[fb]oobar");
    *feed->add_matchers()->add_criteria() =
        TextCriteria(Criteria::RSS_URL_MATCH, "https://plaintext/rss.xml");
  }
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ("id",
            index
                .FindWebFeed(MakePageInfo(
                    PAGE_URL, {GURL("https://test.com/fun-foobar-fun.xml")}))
                .web_feed_id);
  EXPECT_EQ("id", index
                      .FindWebFeed(MakePageInfo(
                          PAGE_URL, {GURL("https://test.com/boobar.xml")}))
                      .web_feed_id);
  EXPECT_EQ("id", index
                      .FindWebFeed(MakePageInfo(
                          PAGE_URL, {GURL("https://plaintext/rss.xml")}))
                      .web_feed_id);
  EXPECT_EQ("id", index
                      .FindWebFeed(MakePageInfo(
                          PAGE_URL, {GURL("https://notmatch"),
                                     GURL("https://plaintext/rss.xml")}))
                      .web_feed_id);

  EXPECT_EQ(
      "",
      index.FindWebFeed(MakePageInfo(PAGE_URL, {GURL("https://test.com/path")}))
          .web_feed_id);
  EXPECT_EQ(
      "", index.FindWebFeed(MakePageInfo(PAGE_URL, {GURL("https://test.com")}))
              .web_feed_id);
  EXPECT_EQ("", index
                    .FindWebFeed(MakePageInfo(
                        PAGE_URL, {GURL("https://plaintext/rss.xml2")}))
                    .web_feed_id);
}

TEST(WebFeedIndex, MultipleConditionsRequiredForMatch) {
  WebFeedIndex index;
  FeedStore::WebFeedStartupData startup_data;
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id");
    feedwire::webfeed::WebFeedMatcher* matcher = feed->add_matchers();
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX);
      criteria->set_text("foo.com");
    }
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_PATH_MATCH);
      criteria->set_text("/fun");
    }
  }
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id2");
    feedwire::webfeed::WebFeedMatcher* matcher = feed->add_matchers();
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX);
      criteria->set_text("bar.com");
    }
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_PATH_MATCH);
      criteria->set_text("/woo");
    }
  }
  {
    auto* feed = startup_data.subscribed_web_feeds.add_feeds();
    feed->set_web_feed_id("id3");
    feedwire::webfeed::WebFeedMatcher* matcher = feed->add_matchers();
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::PAGE_URL_HOST_SUFFIX);
      criteria->set_text("feed.com");
    }
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::RSS_URL_MATCH);
      criteria->set_text("https://rss1.com/rss.xml");
    }
    {
      feedwire::webfeed::WebFeedMatcher::Criteria* criteria =
          matcher->add_criteria();
      criteria->set_criteria_type(
          feedwire::webfeed::WebFeedMatcher::Criteria::RSS_URL_MATCH);
      criteria->set_text("https://rss2.com/rss.xml");
    }
  }
  index.Populate(startup_data.subscribed_web_feeds);

  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com/fun"))).web_feed_id);
  EXPECT_EQ(
      "id",
      index.FindWebFeed(MakePageInfo(GURL("http://sub.foo.com/fun?query")))
          .web_feed_id);
  EXPECT_EQ(
      "id2",
      index.FindWebFeed(MakePageInfo(GURL("https://bar.com/woo"))).web_feed_id);
  EXPECT_EQ(
      "id2",
      index.FindWebFeed(MakePageInfo(GURL("http://sub.bar.com/woo?query")))
          .web_feed_id);
  EXPECT_EQ("id3",
            index
                .FindWebFeed(MakePageInfo(GURL("http://feed.com/"),
                                          {GURL("https://rss2.com/rss.xml"),
                                           GURL("https://rss1.com/rss.xml")}))
                .web_feed_id);

  EXPECT_EQ("", index.FindWebFeed(MakePageInfo(GURL("https://fooo.com/fun")))
                    .web_feed_id);
  EXPECT_EQ("", index.FindWebFeed(MakePageInfo(GURL("https://foo.com/fuun")))
                    .web_feed_id);
  EXPECT_EQ(
      "",
      index.FindWebFeed(MakePageInfo(GURL("https://bar.com/fun"))).web_feed_id);
  EXPECT_EQ(
      "",
      index.FindWebFeed(MakePageInfo(GURL("https://foo.com/woo"))).web_feed_id);
  EXPECT_EQ("",
            index
                .FindWebFeed(MakePageInfo(GURL("http://feed.com/"),
                                          {GURL("https://rss1.com/rss.xml")}))
                .web_feed_id);
  EXPECT_EQ("",
            index
                .FindWebFeed(MakePageInfo(GURL("http://feed.com/"),
                                          {GURL("https://rss2.com/rss.xml")}))
                .web_feed_id);
}

}  // namespace
}  // namespace feed
