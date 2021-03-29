// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_UNSUBSCRIBE_FROM_WEB_FEED_TASK_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_UNSUBSCRIBE_FROM_WEB_FEED_TASK_H_

#include "base/memory/checked_ptr.h"
#include "components/feed/core/proto/v2/wire/web_feed.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_id.h"
#include "components/offline_pages/task/task.h"

namespace feed {

class FeedStream;

// Unsubscribes from a web feed.
class UnsubscribeFromWebFeedTask : public offline_pages::Task {
 public:
  struct Result {
    WebFeedSubscriptionRequestStatus request_status =
        WebFeedSubscriptionRequestStatus::kUnknown;
    WebFeedId unsubscribed_feed_id;
  };

  explicit UnsubscribeFromWebFeedTask(
      FeedStream* stream,
      WebFeedId web_feed_id,
      base::OnceCallback<void(Result)> callback);
  ~UnsubscribeFromWebFeedTask() override;
  UnsubscribeFromWebFeedTask(const UnsubscribeFromWebFeedTask&) = delete;
  UnsubscribeFromWebFeedTask& operator=(const UnsubscribeFromWebFeedTask&) =
      delete;

 private:
  void Run() override;
  void RequestComplete(
      FeedNetwork::ApiResult<feedwire::webfeed::UnfollowUriResponse> result);
  void Done(WebFeedSubscriptionRequestStatus status);

  CheckedPtr<FeedStream> stream_;
  Result result_;
  WebFeedId web_feed_id_;
  base::OnceCallback<void(Result)> callback_;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_UNSUBSCRIBE_FROM_WEB_FEED_TASK_H_
