// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_

#include "content/public/common/url_loader_throttle.h"

namespace data_reduction_proxy {

// Handles Data Reduction Proxy logic that needs to be applied to each request,
// e.g. setting headers to be used with the proxy.
class DataReductionProxyURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  explicit DataReductionProxyURLLoaderThrottle(
      const net::HttpRequestHeaders& post_cache_headers);
  ~DataReductionProxyURLLoaderThrottle() override;

  // content::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

 private:
  net::HttpRequestHeaders post_cache_headers_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_COMMON_DATA_REDUCTION_PROXY_URL_LOADER_THROTTLE_H_
