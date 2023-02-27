// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_QUERY_WEB_FEED_TASK_H_
#define COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_QUERY_WEB_FEED_TASK_H_

#include "base/memory/raw_ref.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/web_feeds.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/operation_token.h"
#include "components/feed/core/v2/public/web_feed_subscriptions.h"
#include "components/offline_pages/task/task.h"
#include "url/gurl.h"

namespace feed {

class FeedStream;

// Queries for a web feed id given a URL.
class QueryWebFeedTask : public offline_pages::Task {
 public:
  struct Request {
    GURL web_feed_url;
    std::string web_feed_id;
  };
  QueryWebFeedTask(
      FeedStream* stream,
      const OperationToken& operation_token,
      Request request,
      base::OnceCallback<void(WebFeedSubscriptions::QueryWebFeedResult)>
          callback);
  ~QueryWebFeedTask() override;
  QueryWebFeedTask(const QueryWebFeedTask&) = delete;
  QueryWebFeedTask& operator=(const QueryWebFeedTask&) = delete;

  void Run() override;

 private:
  void RequestComplete(
      FeedNetwork::ApiResult<feedwire::webfeed::QueryWebFeedResponse> result);
  void Done(WebFeedQueryRequestStatus status);

  const raw_ref<FeedStream> stream_;
  OperationToken operation_token_;
  Request request_;
  feedstore::WebFeedInfo queried_web_feed_info_;
  base::OnceCallback<void(WebFeedSubscriptions::QueryWebFeedResult)> callback_;

  WebFeedSubscriptions::QueryWebFeedResult result_;
  GURL url_;
  std::string web_feed_id_;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_WEB_FEED_SUBSCRIPTIONS_QUERY_WEB_FEED_TASK_H_