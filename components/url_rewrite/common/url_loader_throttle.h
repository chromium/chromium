// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_REWRITE_COMMON_URL_LOADER_THROTTLE_H_
#define COMPONENTS_URL_REWRITE_COMMON_URL_LOADER_THROTTLE_H_

#include <string_view>

#include "components/url_rewrite/common/url_request_rewrite_rules.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace network {
struct ResourceRequest;
}

namespace url_rewrite {

// Implements a URLLoaderThrottle that applies network request rewrites provided
// through the |UrlRequestRewriteRules| rules.
class URLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // A callback that checks if provided header is CORS exempt. The
  // implementation must be case-insensitive.
  using IsHeaderCorsExemptCallback =
      base::RepeatingCallback<bool(std::string_view)>;

  URLLoaderThrottle(scoped_refptr<UrlRequestRewriteRules> rules,
                    IsHeaderCorsExemptCallback is_header_cors_exempt_callback);
  ~URLLoaderThrottle() override;

  URLLoaderThrottle(const URLLoaderThrottle&) = delete;
  URLLoaderThrottle& operator=(const URLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

 private:
  // Applies transformations specified by |rule| to |request|, conditional on
  // the matching criteria of |rule|.
  void ApplyRule(network::ResourceRequest* request,
                 const mojom::UrlRequestRulePtr& rule);

  // Applies |rewrite| transformations to |request|.
  void ApplyRewrite(network::ResourceRequest* request,
                    const mojom::UrlRequestActionPtr& rewrite);

  // Adds HTTP headers from |add_headers| to |request|.
  void ApplyAddHeaders(
      network::ResourceRequest* request,
      const mojom::UrlRequestRewriteAddHeadersPtr& add_headers);

  scoped_refptr<UrlRequestRewriteRules> rules_;
  IsHeaderCorsExemptCallback is_header_cors_exempt_callback_;
};

}  // namespace url_rewrite

#endif  // COMPONENTS_URL_REWRITE_COMMON_URL_LOADER_THROTTLE_H_
