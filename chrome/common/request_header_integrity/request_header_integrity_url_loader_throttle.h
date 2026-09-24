// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace request_header_integrity {

class ChromeCompaneroLoader;

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
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  static bool IsFeatureEnabled();
  static void UpdateCorsExemptHeaders(
      network::mojom::NetworkContextParams* params);

  // Adds the integrity headers.
  static void AddRequestIntegrityHeaders(
      net::HttpRequestHeaders* headers,
      ChromeCompaneroLoader& companero_loader);

  // Called both for initial requests and upon redirects during prefetching.
  // - Adds the integrity header names to `removed_headers` in the case where
  //   the request is redirection from a target domain to a non-target domain
  //   and already has the integrity headers.
  // - Adds the integrity headers to `modified_cors_exempt_headers` if the
  //   request is for a target domain.
  static void ModifyRequestIntegrityHeadersForPrefetch(
      const GURL& url,
      std::vector<std::string>& removed_headers,
      net::HttpRequestHeaders& modified_cors_exempt_headers);

 protected:
  // Test seam. Production code uses the default constructor, which binds
  // ChromeCompaneroLoader::GetInstance(); tests derive from this class to
  // inject their own loader. `companero_loader` must outlive `this`.
  explicit RequestHeaderIntegrityURLLoaderThrottle(
      ChromeCompaneroLoader& companero_loader);

 private:
  const raw_ref<ChromeCompaneroLoader> companero_loader_;
};

}  // namespace request_header_integrity

#endif  // CHROME_COMMON_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_
