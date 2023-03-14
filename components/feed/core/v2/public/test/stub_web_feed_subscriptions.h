// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_WEB_FEED_SUBSCRIPTIONS_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_WEB_FEED_SUBSCRIPTIONS_H_

#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"

namespace feed {

class StubWebFeedSubscriptions : public WebFeedSubscriptions {
 public:
  void FollowWebFeed(
      const WebFeedPageInformation& page_info,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback) override {}
  void FollowWebFeed(
      const std::string& web_feed_id,
      bool is_durable_request,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback) override {}
  void UnfollowWebFeed(
      const std::string& web_feed_id,
      bool is_durable_request,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(UnfollowWebFeedResult)> callback) override {}
  void FindWebFeedInfoForPage(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(WebFeedMetadata)> callback) override {}
  void FindWebFeedInfoForWebFeedId(
      const std::string& web_feed_id,
      base::OnceCallback<void(WebFeedMetadata)> callback) override {}
  void GetAllSubscriptions(
      base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback)
      override {}
  void RefreshSubscriptions(
      base::OnceCallback<void(RefreshResult)> callback) override {}
  void RefreshRecommendedFeeds(
      base::OnceCallback<void(RefreshResult)> callback) override {}
  void IsWebFeedSubscriber(base::OnceCallback<void(bool)> callback) override {}
  void SubscribedWebFeedCount(base::OnceCallback<void(int)> callback) override {
  }
  void QueryWebFeed(
      const GURL& gurl,
      base::OnceCallback<void(QueryWebFeedResult)> callback) override {}
  void QueryWebFeedId(
      const std::string& web_feed_id,
      base::OnceCallback<void(QueryWebFeedResult)> callback) override {}
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_TEST_STUB_WEB_FEED_SUBSCRIPTIONS_H_
