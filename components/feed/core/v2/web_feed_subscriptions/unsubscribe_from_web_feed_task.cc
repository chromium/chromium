// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/unsubscribe_from_web_feed_task.h"

#include <algorithm>

#include "base/bind.h"
#include "base/stl_util.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"

namespace feed {

UnsubscribeFromWebFeedTask::UnsubscribeFromWebFeedTask(
    FeedStream* stream,
    WebFeedId web_feed_id,
    base::OnceCallback<void(Result)> callback)
    : stream_(stream),
      web_feed_id_(web_feed_id),
      callback_(std::move(callback)) {}

UnsubscribeFromWebFeedTask::~UnsubscribeFromWebFeedTask() = default;

void UnsubscribeFromWebFeedTask::Run() {
  WebFeedSubscriptionCoordinator::SubscriptionInfo info =
      stream_->subscriptions().FindSubscriptionInfoById(web_feed_id_);
  if (info.status != WebFeedSubscriptionStatus::kSubscribed) {
    Done(WebFeedSubscriptionRequestStatus::kSuccess);
    return;
  }

  if (stream_->IsOffline()) {
    Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
    return;
  }

  feedwire::webfeed::UnfollowUriRequest request;
  if (web_feed_id_.is_subscription_id()) {
    request.set_subscription_id(web_feed_id_.GetValue());
  } else if (web_feed_id_.is_web_feed_id()) {
    request.set_web_feed_id(web_feed_id_.GetValue());
  }
  stream_->GetNetwork()->SendApiRequest<UnfollowWebFeedDiscoverApi>(
      request, base::BindOnce(&UnsubscribeFromWebFeedTask::RequestComplete,
                              base::Unretained(this)));
}

void UnsubscribeFromWebFeedTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::UnfollowUriResponse> result) {
  if (!result.response_body) {
    Done(WebFeedSubscriptionRequestStatus::kFailedUnknownError);
    return;
  }

  result_.unsubscribed_feed_id = web_feed_id_;
  Done(WebFeedSubscriptionRequestStatus::kSuccess);
}

void UnsubscribeFromWebFeedTask::Done(WebFeedSubscriptionRequestStatus status) {
  result_.request_status = status;
  std::move(callback_).Run(std::move(result_));
  TaskComplete();
}

}  // namespace feed
