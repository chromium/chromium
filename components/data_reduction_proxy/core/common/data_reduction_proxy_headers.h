// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_

#include "base/strings/string_piece.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace data_reduction_proxy {

// Gets the header used for data reduction proxy requests and responses.
const char* chrome_proxy_header();

// Returns the Original-Full-Content-Length(OFCL) value in the Chrome-Proxy
// header. Returns -1 in case of of error or if OFCL does not exist. |headers|
// must be non-null.
int64_t GetDataReductionProxyOFCL(const net::HttpResponseHeaders* headers);

// Returns an estimate of the compression ratio from the Content-Length and
// Chrome-Proxy Original-Full-Content-Length(OFCL) response headers. These may
// not be populated for responses which are streamed from the origin which will
// be treated as a no compression case. Notably, only the response body size is
// used to compute the ratio, and headers are excluded, since this is only an
// estimate for response that is beginning to arrive.
double EstimateCompressionRatioFromHeaders(
    const network::mojom::URLResponseHead* response_head);

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_HEADERS_H_
