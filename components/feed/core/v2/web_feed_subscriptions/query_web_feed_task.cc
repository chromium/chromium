// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/query_web_feed_task.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "components/feed/core/proto/v2/wire/consistency_token.pb.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/web_feed_subscription_coordinator.h"
#include "components/feed/core/v2/web_feed_subscriptions/web_feed_types.h"
#include "components/feed/core/v2/web_feed_subscriptions/wire_to_store.h"
#include "url/gurl.h"

namespace feed {

QueryWebFeedTask::QueryWebFeedTask(
    FeedStream* stream,
    const OperationToken& operation_token,
    Request request,
    base::OnceCallback<void(WebFeedSubscriptions::QueryWebFeedResult)> callback)
    : stream_(*stream),
      operation_token_(operation_token),
      request_(std::move(request)),
      callback_(std::move(callback)) {
  url_ = request_.web_feed_url;
  web_feed_id_ = request_.web_feed_id;
}

QueryWebFeedTask::~QueryWebFeedTask() = default;

void QueryWebFeedTask::Run() {
  if (!operation_token_) {
    Done(WebFeedQueryRequestStatus::kAbortWebFeedQueryPendingClearAll);
    return;
  }
  if (web_feed_id_.empty() && !url_.is_valid()) {
    Done(WebFeedQueryRequestStatus::kFailedInvalidUrl);
    return;
  }
  if (stream_->IsOffline()) {
    Done(WebFeedQueryRequestStatus::kFailedOffline);
    return;
  }

  feedwire::webfeed::QueryWebFeedRequest request;
  SetConsistencyToken(request, stream_->GetMetadata().consistency_token());
  if (!web_feed_id_.empty()) {
    request.set_name(web_feed_id_);
  } else {
    request.mutable_web_feed_uris()->set_web_page_uri(url_.spec());
  }
  stream_->GetNetwork().SendApiRequest<QueryWebFeedDiscoverApi>(
      request, stream_->GetAccountInfo(), stream_->GetSignedInRequestMetadata(),
      base::BindOnce(&QueryWebFeedTask::RequestComplete,
                     base::Unretained(this)));
}

void QueryWebFeedTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::QueryWebFeedResponse> result) {
  // This will always be valid, because ClearAllTask cannot have run after this
  // task starts.
  DCHECK(operation_token_);

  if (result.response_body) {
    stream_->SetMetadata(feedstore::MaybeUpdateConsistencyToken(
        stream_->GetMetadata(), result.response_body->consistency_token()));
    queried_web_feed_info_ =
        ConvertToStore(*result.response_body->mutable_web_feed());
    Done(WebFeedQueryRequestStatus::kSuccess);
    return;
  }
  Done(WebFeedQueryRequestStatus::kFailedUnknownError);
}

void QueryWebFeedTask::Done(WebFeedQueryRequestStatus status) {
  WebFeedSubscriptions::QueryWebFeedResult result;
  result.request_status = status;
  result.web_feed_id = queried_web_feed_info_.web_feed_id();
  result.title = queried_web_feed_info_.title();
  result.url = queried_web_feed_info_.visit_uri();
  std::move(callback_).Run(std::move(result));
  TaskComplete();
}

}  // namespace feed
