// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_WEB_FEED_SUBSCRIPTIONS_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_WEB_FEED_SUBSCRIPTIONS_H_

#include <ostream>
#include <string>

#include "base/functional/callback.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

// API to access Web Feed subscriptions.
class WebFeedSubscriptions {
 public:
  struct FollowWebFeedResult {
    WebFeedSubscriptionRequestStatus request_status =
        WebFeedSubscriptionRequestStatus::kUnknown;
    // If followed, the metadata for the followed feed.
    WebFeedMetadata web_feed_metadata;
    // Number of subscriptions the user has after the Follow operation.
    int subscription_count = 0;
    // The change reason from the request.
    feedwire::webfeed::WebFeedChangeReason change_reason;
  };
  struct QueryWebFeedResult {
    QueryWebFeedResult();
    QueryWebFeedResult(const QueryWebFeedResult& query_web_feed_result);
    ~QueryWebFeedResult();
    WebFeedQueryRequestStatus request_status =
        WebFeedQueryRequestStatus::kUnknown;
    // The id of the queried web feed.
    std::string web_feed_id;
    // The title of the queried web feed.
    std::string title;
    // The url of the queried web feed.
    std::string url;
  };
  // Follow a web feed given information about a web page. Calls `callback` when
  // complete. The callback parameter reports whether the url is now considered
  // followed. This always creates a non-durable request.
  virtual void FollowWebFeed(
      const WebFeedPageInformation& page_info,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback) = 0;

  // Follow a web feed given a web feed ID.
  // If `is_durable_request` is true, the request to follow will be persisted
  // and retried later if necessary. `callback` provides the result of the
  // initial Follow request, but not any later retries.
  virtual void FollowWebFeed(
      const std::string& web_feed_id,
      bool is_durable_request,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(FollowWebFeedResult)> callback) = 0;

  struct UnfollowWebFeedResult {
    WebFeedSubscriptionRequestStatus request_status =
        WebFeedSubscriptionRequestStatus::kUnknown;
    // Number of subscriptions the user has after the Unfollow operation.
    int subscription_count = 0;
  };

  // Follow a web feed given a URL. Calls `callback` when complete. The callback
  // parameter reports whether the url is now considered followed.
  // If `is_durable_request` is true, the request to follow will be persisted
  // and retried later if necessary. `callback` provides the result of the
  // initial Follow request, but not any later retries.
  virtual void UnfollowWebFeed(
      const std::string& web_feed_id,
      bool is_durable_request,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(UnfollowWebFeedResult)> callback) = 0;

  // Web Feed lookup for pages. These functions fetch `WebFeedMetadata` for any
  // web feed which is recommended by the server, currently subscribed, or was
  // recently subscribed. `callback` is given a nullptr if no web feed data is
  // found.

  // Look up web feed information for a web page.
  virtual void FindWebFeedInfoForPage(
      const WebFeedPageInformation& page_info,
      base::OnceCallback<void(WebFeedMetadata)> callback) = 0;

  // Look up web feed information for a web page given the `web_feed_id`.
  virtual void FindWebFeedInfoForWebFeedId(
      const std::string& web_feed_id,
      base::OnceCallback<void(WebFeedMetadata)> callback) = 0;

  // Returns all current subscriptions.
  virtual void GetAllSubscriptions(
      base::OnceCallback<void(std::vector<WebFeedMetadata>)> callback) = 0;

  // Result of `RefreshSubscriptions()`.
  struct RefreshResult {
    // Whether or not the refresh succeeded. Failures may happen due to network
    // errors.
    bool success = false;
  };
  // Refresh list of subscribed web feeds from the server.
  virtual void RefreshSubscriptions(
      base::OnceCallback<void(RefreshResult)> callback) = 0;

  // Force a refresh of the server-recommended web feeds.
  virtual void RefreshRecommendedFeeds(
      base::OnceCallback<void(RefreshResult)> callback) = 0;

  // Whether the user has subscribed to at least one web feed. May require
  // fetching data from the server if cached data is not fresh. If fetching
  // fails, returns the last-known state.
  virtual void IsWebFeedSubscriber(base::OnceCallback<void(bool)> callback) = 0;

  // How many web feeds for which the user is subscribed. May require
  // fetching data from the server if cached data is not fresh. If fetching
  // fails, returns the last-known state.
  virtual void SubscribedWebFeedCount(
      base::OnceCallback<void(int)> callback) = 0;

  // return the WebFeed Id based on the Uri provided.
  virtual void QueryWebFeed(
      const GURL& url,
      base::OnceCallback<void(QueryWebFeedResult)> callback) = 0;

  // return the WebFeed Id based on the id provided.
  virtual void QueryWebFeedId(
      const std::string& web_feed_id,
      base::OnceCallback<void(QueryWebFeedResult)> callback) = 0;

  // Output debugging information for snippets-internals.
  virtual void DumpStateForDebugging(std::ostream& ss) {}
};

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_WEB_FEED_SUBSCRIPTIONS_H_
