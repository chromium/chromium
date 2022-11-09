// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_SUBSCRIBE_TO_WEB_FEED_TASK_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_SUBSCRIBE_TO_WEB_FEED_TASK_H_

#include "base/memory/raw_ref.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/offline_pages/task/task.h"
#include "url/gurl.h"

namespace feed {

class FeedStream;

// Subscribes to a web feed.
class SubscribeToWebFeedTask : public offline_pages::Task {
 public:
  // To subscribe, only one of these fields should be set.
  struct Request {
    WebFeedPageInformation page_info;
    std::string web_feed_id;
    feedwire::webfeed::WebFeedChangeReason change_reason = feedwire::webfeed::
        WebFeedChangeReason::WEB_FEED_CHANGE_REASON_UNSPECIFIED;
    // Whether the subscription request will be stored and retried if failed.
    bool durable = false;
  };
  struct Result {
    WebFeedSubscriptionRequestStatus request_status =
        WebFeedSubscriptionRequestStatus::kUnknown;
    // If the subscription succeeds, or the web feed was already followed, this
    // is its ID.
    std::string followed_web_feed_id;
    feedstore::WebFeedInfo web_feed_info;
    // The change reason from the request.
    feedwire::webfeed::WebFeedChangeReason change_reason = feedwire::webfeed::
        WebFeedChangeReason::WEB_FEED_CHANGE_REASON_UNSPECIFIED;
  };
  SubscribeToWebFeedTask(FeedStream* stream,
                         const OperationToken& operation_token,
                         Request request,
                         base::OnceCallback<void(Result)> callback);
  ~SubscribeToWebFeedTask() override;
  SubscribeToWebFeedTask(const SubscribeToWebFeedTask&) = delete;
  SubscribeToWebFeedTask& operator=(const SubscribeToWebFeedTask&) = delete;

  void Run() override;

 private:
  void RequestComplete(
      FeedNetwork::ApiResult<feedwire::webfeed::FollowWebFeedResponse> result);
  void ReadFeedDataComplete(FeedStore::WebFeedStartupData startup_data);
  void Done(WebFeedSubscriptionRequestStatus status);

  const raw_ref<FeedStream> stream_;
  OperationToken operation_token_;
  Request request_;
  feedstore::WebFeedInfo subscribed_web_feed_info_;
  base::OnceCallback<void(Result)> callback_;

  Result result_;
  GURL url_;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_SUBSCRIBE_TO_WEB_FEED_TASK_H_
