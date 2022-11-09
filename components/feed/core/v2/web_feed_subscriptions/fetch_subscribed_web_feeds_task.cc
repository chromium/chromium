// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/fetch_subscribed_web_feeds_task.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feedstore_util.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/web_feed_subscriptions/wire_to_store.h"

namespace feed {

FetchSubscribedWebFeedsTask::Result::Result() = default;
FetchSubscribedWebFeedsTask::Result::~Result() = default;
FetchSubscribedWebFeedsTask::Result::Result(const Result&) = default;
FetchSubscribedWebFeedsTask::Result::Result(Result&&) = default;
FetchSubscribedWebFeedsTask::Result&
FetchSubscribedWebFeedsTask::Result::operator=(const Result&) = default;
FetchSubscribedWebFeedsTask::Result&
FetchSubscribedWebFeedsTask::Result::operator=(Result&&) = default;

FetchSubscribedWebFeedsTask::FetchSubscribedWebFeedsTask(
    FeedStream* stream,
    const OperationToken& operation_token,
    base::OnceCallback<void(Result)> callback)
    : stream_(*stream),
      operation_token_(operation_token),
      callback_(std::move(callback)) {}
FetchSubscribedWebFeedsTask::~FetchSubscribedWebFeedsTask() = default;

void FetchSubscribedWebFeedsTask::Run() {
  if (!operation_token_) {
    Done(WebFeedRefreshStatus::kAbortFetchWebFeedPendingClearAll);
    return;
  }
  if (!stream_->GetRequestThrottler().RequestQuota(
          ListWebFeedsDiscoverApi::kRequestType)) {
    Done(WebFeedRefreshStatus::kNetworkRequestThrottled);
    return;
  }
  feedwire::webfeed::ListWebFeedsRequest request;
  SetConsistencyToken(request, stream_->GetMetadata().consistency_token());
  stream_->GetNetwork().SendApiRequest<ListWebFeedsDiscoverApi>(
      request, stream_->GetAccountInfo(), stream_->GetSignedInRequestMetadata(),
      base::BindOnce(&FetchSubscribedWebFeedsTask::RequestComplete,
                     base::Unretained(this)));
}

void FetchSubscribedWebFeedsTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::ListWebFeedsResponse> response) {
  // This will always be valid, because ClearAllTask cannot have run after this
  // task starts.
  DCHECK(operation_token_);

  if (!response.response_body) {
    Done(WebFeedRefreshStatus::kNetworkFailure);
    return;
  }

  result_.subscribed_web_feeds.reserve(
      response.response_body->web_feeds_size());
  for (auto& web_feed : *response.response_body->mutable_web_feeds()) {
    result_.subscribed_web_feeds.push_back(ConvertToStore(std::move(web_feed)));
  }
  Done(WebFeedRefreshStatus::kSuccess);
}

void FetchSubscribedWebFeedsTask::Done(WebFeedRefreshStatus status) {
  result_.status = status;
  std::move(callback_).Run(std::move(result_));
  TaskComplete();
}

}  // namespace feed
