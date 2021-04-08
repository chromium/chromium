// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/web_feed_subscriptions/fetch_recommended_web_feeds_task.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/web_feed_subscriptions/wire_to_store.h"

namespace feed {

FetchRecommendedWebFeedsTask::Result::Result() = default;
FetchRecommendedWebFeedsTask::Result::~Result() = default;
FetchRecommendedWebFeedsTask::Result::Result(const Result&) = default;
FetchRecommendedWebFeedsTask::Result::Result(Result&&) = default;
FetchRecommendedWebFeedsTask::Result&
FetchRecommendedWebFeedsTask::Result::operator=(const Result&) = default;
FetchRecommendedWebFeedsTask::Result&
FetchRecommendedWebFeedsTask::Result::operator=(Result&&) = default;

FetchRecommendedWebFeedsTask::FetchRecommendedWebFeedsTask(
    FeedStream* stream,
    base::OnceCallback<void(Result)> callback)
    : stream_(stream), callback_(std::move(callback)) {}
FetchRecommendedWebFeedsTask::~FetchRecommendedWebFeedsTask() = default;

void FetchRecommendedWebFeedsTask::Run() {
  if (stream_->ClearAllInProgress()) {
    Done(WebFeedRefreshStatus::kAbortFetchWebFeedPendingClearAll);
    return;
  }
  if (!stream_->GetRequestThrottler()->RequestQuota(
          ListRecommendedWebFeedDiscoverApi::kRequestType)) {
    Done(WebFeedRefreshStatus::kNetworkRequestThrottled);
    return;
  }
  stream_->GetNetwork()->SendApiRequest<ListRecommendedWebFeedDiscoverApi>(
      {}, stream_->GetSyncSignedInGaia(),
      base::BindOnce(&FetchRecommendedWebFeedsTask::RequestComplete,
                     base::Unretained(this)));
}

void FetchRecommendedWebFeedsTask::RequestComplete(
    FeedNetwork::ApiResult<feedwire::webfeed::ListRecommendedWebFeedsResponse>
        response) {
  if (!response.response_body) {
    Done(WebFeedRefreshStatus::kNetworkFailure);
    return;
  }

  result_.recommended_web_feeds.reserve(
      response.response_body->recommended_web_feeds_size());
  for (auto& web_feed :
       *response.response_body->mutable_recommended_web_feeds()) {
    result_.recommended_web_feeds.push_back(
        ConvertToStore(std::move(web_feed)));
  }
  Done(WebFeedRefreshStatus::kSuccess);
}

void FetchRecommendedWebFeedsTask::Done(WebFeedRefreshStatus status) {
  result_.status = status;
  std::move(callback_).Run(std::move(result_));
  TaskComplete();
}

}  // namespace feed
