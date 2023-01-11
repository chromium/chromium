// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/unsubscribe_from_web_feed_task.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_types.h"

namespace feed {

UnsubscribeFromWebFeedTask::UnsubscribeFromWebFeedTask(
    FeedStream* stream,
    const OperationToken& operation_token,
    const std::string& web_feed_id,
    feedwire::webfeed::WebFeedChangeReason change_reason,
    base::OnceCallback<void(Result)> callback)
    : stream_(*stream),
      operation_token_(operation_token),
      web_feed_name_(web_feed_id),
      change_reason_(change_reason),
      callback_(std::move(callback)) {}

UnsubscribeFromWebFeedTask::~UnsubscribeFromWebFeedTask() = default;

void UnsubscribeFromWebFeedTask::Run() {
  if (!operation_token_) {
    Done(WebFeedSubscriptionRequestStatus::
             kAbortWebFeedSubscriptionPendingClearAll);
    return;
  }

  WebFeedSubscriptionInfo info =
      stream_->subscriptions().FindSubscriptionInfoById(web_feed_name_);
  if (info.status != WebFeedSubscriptionStatus::kSubscribed) {
    Done(WebFeedSubscriptionRequestStatus::kSuccess);
    return;
  }

  if (stream_->IsOffline()) {
    Done(WebFeedSubscriptionRequestStatus::kFailedOffline);
    return;
  }

  feedwire::webfeed::UnfollowWebFeedRequest request;
  SetConsistencyToken(request, stream_->GetMetadata().consistency_token());
  request.set_name(web_feed_name_);
  request.set_change_reason(change_reason_);
  stream_->GetNetwork().SendApiRequest<UnfollowWebFeedDiscoverApi>(
      request, stream_->GetAccountInfo(), stream_->GetSignedInRequestMetadata(),
      base::BindOnce(&UnsubscribeFromWebFeedTask::RequestComplete,
                     base::Unretained(this)));
}

void UnsubscribeFromWebFeedTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::UnfollowWebFeedResponse> result) {
  // This will always be valid, because ClearAllTask cannot have run after this
  // task starts.
  DCHECK(operation_token_);
  if (!result.response_body) {
    Done(WebFeedSubscriptionRequestStatus::kFailedUnknownError);
    return;
  }

  stream_->SetMetadata(feedstore::MaybeUpdateConsistencyToken(
      stream_->GetMetadata(), result.response_body->consistency_token()));

  result_.unsubscribed_feed_name = web_feed_name_;
  Done(WebFeedSubscriptionRequestStatus::kSuccess);
}

void UnsubscribeFromWebFeedTask::Done(WebFeedSubscriptionRequestStatus status) {
  result_.request_status = status;
  std::move(callback_).Run(std::move(result_));
  TaskComplete();
}

}  // namespace feed
