// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_ACTIVITY_FILTERING_URL_LOADER_THROTTLE_H_
#define CHROMECAST_COMMON_ACTIVITY_FILTERING_URL_LOADER_THROTTLE_H_

#include <string>
#include <vector>

#include "chromecast/common/activity_url_filter.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace chromecast {

// This class monitors requests issued by third-party javascript run via
// Activities, and blocks the request based on URL whitelisting.
class ActivityFilteringURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit ActivityFilteringURLLoaderThrottle(ActivityUrlFilter* filter);

  ActivityFilteringURLLoaderThrottle(
      const ActivityFilteringURLLoaderThrottle&) = delete;
  ActivityFilteringURLLoaderThrottle& operator=(
      const ActivityFilteringURLLoaderThrottle&) = delete;

  ~ActivityFilteringURLLoaderThrottle() override;

  // content::URLLoaderThrottle implementation:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

 private:
  // content::URLLoaderThrottle implementation:
  void DetachFromCurrentSequence() override;

  void FilterURL(const GURL& url);

  ActivityUrlFilter* url_filter_;
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_ACTIVITY_FILTERING_URL_LOADER_THROTTLE_H_
