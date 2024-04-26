// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_URL_LOADER_THROTTLE_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_URL_LOADER_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/variations/variations.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace variations {

enum class Owner;
class VariationsClient;

// For non-incognito sessions, this class is created per request. If the
// requests is for a google domains, it adds variations where appropriate (see
// VariationsHeaderHelper::AppendHeaderIfNeeded) and removes them on redirect
// if necessary.
class VariationsURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // Constructor for throttles created outside the render thread. Allows us to
  // distinguish between Owner::kUnknownFromRenderer and Owner::kUnknown for
  // ResourceRequests without TrustedParams. See IsFirstPartyContext() in
  // variations_http_headers.cc for more details.
  //
  // TODO(crbug.com/40135370): Consider removing this once we've confirmed that
  // non-render-thread-initiated requests have TrustedParams when needed.
  explicit VariationsURLLoaderThrottle(
      variations::mojom::VariationsHeadersPtr variations_headers);
  // Constructor for throttles created in the render thread, i.e. via
  // VariationsRenderThreadObserver.
  VariationsURLLoaderThrottle(
      variations::mojom::VariationsHeadersPtr variations_headers,
      const url::Origin& top_frame_origin);
  ~VariationsURLLoaderThrottle() override;

  VariationsURLLoaderThrottle(VariationsURLLoaderThrottle&&) = delete;
  VariationsURLLoaderThrottle(const VariationsURLLoaderThrottle&) = delete;

  VariationsURLLoaderThrottle& operator==(VariationsURLLoaderThrottle&&) =
      delete;
  VariationsURLLoaderThrottle& operator==(const VariationsURLLoaderThrottle&) =
      delete;

  // If |variations_client| isn't null and the user isn't incognito then a
  // throttle will be appended.
  static void AppendThrottleIfNeeded(
      const variations::VariationsClient* variations_client,
      std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles);

 private:
  // blink::URLLoaderThrottle:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;

  // Stores multiple appropriate variations headers. See GetClientDataHeaders()
  // in variations_ids_provider.h for more details.
  variations::mojom::VariationsHeadersPtr variations_headers_;

  // Denotes whether the top frame of the request-initiating frame is a Google-
  // owned web property, e.g. YouTube.
  Owner owner_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_URL_LOADER_THROTTLE_H_
