// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_network.h"

#include "components/feed/core/v2/metrics_reporter.h"

namespace feed {

FeedNetwork::QueryRequestResult::QueryRequestResult() = default;
FeedNetwork::QueryRequestResult::~QueryRequestResult() = default;
FeedNetwork::QueryRequestResult::QueryRequestResult(QueryRequestResult&&) =
    default;
FeedNetwork::QueryRequestResult& FeedNetwork::QueryRequestResult::operator=(
    QueryRequestResult&&) = default;

FeedNetwork::~FeedNetwork() = default;

// static
void FeedNetwork::ParseAndForwardApiResponseStarted(
    NetworkRequestType request_type,
    const RawResponse& raw_response) {
  MetricsReporter::NetworkRequestComplete(request_type,
                                          raw_response.response_info);
}

}  // namespace feed
