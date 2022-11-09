// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_UNSUBSCRIBE_FROM_WEB_FEED_TASK_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_UNSUBSCRIBE_FROM_WEB_FEED_TASK_H_

#include "base/memory/raw_ref.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/offline_pages/task/task.h"

namespace feed {

class FeedStream;

// Unsubscribes from a web feed.
class UnsubscribeFromWebFeedTask : public offline_pages::Task {
 public:
  struct Result {
    WebFeedSubscriptionRequestStatus request_status =
        WebFeedSubscriptionRequestStatus::kUnknown;
    std::string unsubscribed_feed_name;
  };

  explicit UnsubscribeFromWebFeedTask(
      FeedStream* stream,
      const OperationToken& operation_token,
      const std::string& web_feed_id,
      feedwire::webfeed::WebFeedChangeReason change_reason,
      base::OnceCallback<void(Result)> callback);
  ~UnsubscribeFromWebFeedTask() override;
  UnsubscribeFromWebFeedTask(const UnsubscribeFromWebFeedTask&) = delete;
  UnsubscribeFromWebFeedTask& operator=(const UnsubscribeFromWebFeedTask&) =
      delete;

 private:
  void Run() override;
  void RequestComplete(
      FeedNetwork::ApiResult<feedwire::webfeed::UnfollowWebFeedResponse>
          result);
  void Done(WebFeedSubscriptionRequestStatus status);

  const raw_ref<FeedStream> stream_;
  OperationToken operation_token_;
  Result result_;
  std::string web_feed_name_;
  feedwire::webfeed::WebFeedChangeReason change_reason_;
  base::OnceCallback<void(Result)> callback_;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_UNSUBSCRIBE_FROM_WEB_FEED_TASK_H_
