// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_FETCH_SUBSCRIBED_WEB_FEEDS_TASK_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_FETCH_SUBSCRIBED_WEB_FEEDS_TASK_H_

#include "base/memory/raw_ref.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/types.h"
#include "components/offline_pages/task/task.h"

namespace feed {
class FeedStream;

// Fetches and returns the subscribed web feeds.
class FetchSubscribedWebFeedsTask : public offline_pages::Task {
 public:
  struct Result {
    Result();
    ~Result();
    Result(const Result&);
    Result(Result&&);
    Result& operator=(const Result&);
    Result& operator=(Result&&);
    WebFeedRefreshStatus status = WebFeedRefreshStatus::kNoStatus;
    std::vector<feedstore::WebFeedInfo> subscribed_web_feeds;
  };

  FetchSubscribedWebFeedsTask(FeedStream* stream,
                              const OperationToken& operation_token,
                              base::OnceCallback<void(Result)> callback);
  ~FetchSubscribedWebFeedsTask() override;
  FetchSubscribedWebFeedsTask(const FetchSubscribedWebFeedsTask&) = delete;
  FetchSubscribedWebFeedsTask& operator=(const FetchSubscribedWebFeedsTask&) =
      delete;

 private:
  void Run() override;
  void RequestComplete(
      FeedNetwork::ApiResult<feedwire::webfeed::ListWebFeedsResponse> response);
  void Done(WebFeedRefreshStatus status);

  const raw_ref<FeedStream> stream_;
  OperationToken operation_token_;
  Result result_;
  base::OnceCallback<void(Result)> callback_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_FETCH_SUBSCRIBED_WEB_FEEDS_TASK_H_
