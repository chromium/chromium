// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_OMNIBOX_URL_LOADER_THROTTLE_H_
#define COMPONENTS_VARIATIONS_NET_OMNIBOX_URL_LOADER_THROTTLE_H_

#include <memory>
#include <vector>

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace variations {

// This class is created per request. If the request is for a Google domain
// served over HTTPS, it adds a header describing the state of the omnibox
// on-device search suggestions provider, if that provider is in a non-default
// state. It also removes this header on redirect away from Google domains.
class COMPONENT_EXPORT(OMNIBOX_HTTP_HEADERS) OmniboxURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<OmniboxURLLoaderThrottle> {
 public:
  OmniboxURLLoaderThrottle();
  ~OmniboxURLLoaderThrottle() override;

  OmniboxURLLoaderThrottle(OmniboxURLLoaderThrottle&&) = delete;
  OmniboxURLLoaderThrottle(const OmniboxURLLoaderThrottle&) = delete;

  OmniboxURLLoaderThrottle& operator==(OmniboxURLLoaderThrottle&&) = delete;
  OmniboxURLLoaderThrottle& operator==(const OmniboxURLLoaderThrottle&) =
      delete;

  // Adds a utility class that calls into the above function on web content
  // navigations.
  static void AppendThrottleIfNeeded(
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
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_OMNIBOX_URL_LOADER_THROTTLE_H_
