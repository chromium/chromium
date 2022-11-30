// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/activity_filtering_url_loader_throttle.h"

#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"

namespace chromecast {
namespace {

const char kCancelReason[] = "ActivityFilteringURLLoaderThrottle";

}  // namespace

ActivityFilteringURLLoaderThrottle::ActivityFilteringURLLoaderThrottle(
    ActivityUrlFilter* filter)
    : url_filter_(filter) {}

ActivityFilteringURLLoaderThrottle::~ActivityFilteringURLLoaderThrottle() =
    default;

void ActivityFilteringURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* /* defer */) {
  FilterURL(request->url);
}

void ActivityFilteringURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* /* defer */,
    std::vector<std::string>* /* to_be_removed_request_headers */,
    net::HttpRequestHeaders* /* modified_request_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_request_headers */) {
  FilterURL(redirect_info->new_url);
}

void ActivityFilteringURLLoaderThrottle::DetachFromCurrentSequence() {}

void ActivityFilteringURLLoaderThrottle::FilterURL(const GURL& url) {
  // Pass through allowed URLs, block otherwise.
  if (!url_filter_->UrlMatchesWhitelist(url))
    delegate_->CancelWithError(net::ERR_ACCESS_DENIED, kCancelReason);
}

}  // namespace chromecast
