// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace net {
class HttpRequestHeaders;
}

namespace network::mojom {
class NetworkContextParams;
}

namespace request_header_integrity {

class RequestHeaderIntegrityURLLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  RequestHeaderIntegrityURLLoaderThrottle();
  RequestHeaderIntegrityURLLoaderThrottle(
      const RequestHeaderIntegrityURLLoaderThrottle&) = delete;
  RequestHeaderIntegrityURLLoaderThrottle& operator=(
      const RequestHeaderIntegrityURLLoaderThrottle&) = delete;

  ~RequestHeaderIntegrityURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

  static bool IsFeatureEnabled();
  static void UpdateCorsExemptHeaders(
      network::mojom::NetworkContextParams* params);

  // Modifies the request integrity headers in `cors_exempt_headers` in the same
  // way as `WillStartRequest()` and `WillRedirectRequest()`.
  // - Adds the request integrity headers if a request is for a target domain.
  // - Removes the headers if the request is redirected from a target domain to
  //   a non-target domain.
  static void ModifyRequestIntegrityHeaders(
      const GURL& url,
      bool url_is_redirect,
      net::HttpRequestHeaders& cors_exempt_headers);
};

}  // namespace request_header_integrity

#endif  // CHROME_COMMON_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_
