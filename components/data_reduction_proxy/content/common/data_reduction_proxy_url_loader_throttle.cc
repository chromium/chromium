// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_url_loader_throttle.h"

#include "components/data_reduction_proxy/content/common/header_util.h"

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

DataReductionProxyURLLoaderThrottle::DataReductionProxyURLLoaderThrottle(
    const net::HttpRequestHeaders& post_cache_headers)
    : post_cache_headers_(post_cache_headers) {}

DataReductionProxyURLLoaderThrottle::~DataReductionProxyURLLoaderThrottle() {}

void DataReductionProxyURLLoaderThrottle::DetachFromCurrentSequence() {}

void DataReductionProxyURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  MaybeSetAcceptTransformHeader(
      request->url, static_cast<content::ResourceType>(request->resource_type),
      request->previews_state, &request->custom_proxy_pre_cache_headers);
  request->custom_proxy_post_cache_headers = post_cache_headers_;

  if (request->resource_type == content::RESOURCE_TYPE_MEDIA)
    request->custom_proxy_use_alternate_proxy_list = true;
}

}  // namespace data_reduction_proxy
