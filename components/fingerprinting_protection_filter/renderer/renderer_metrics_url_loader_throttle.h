// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_METRICS_URL_LOADER_THROTTLE_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_METRICS_URL_LOADER_THROTTLE_H_

#include <optional>

#include "components/fingerprinting_protection_filter/common/throttle_creation_result.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

class GURL;

namespace fingerprinting_protection_filter {

class RendererMetricsURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit RendererMetricsURLLoaderThrottle(
      RendererThrottleCreationResult result,
      std::optional<url::Origin> request_initiator,
      const GURL& initial_request_url);
  ~RendererMetricsURLLoaderThrottle() override;

  RendererMetricsURLLoaderThrottle(const RendererMetricsURLLoaderThrottle&) =
      delete;
  RendererMetricsURLLoaderThrottle& operator=(
      const RendererMetricsURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;
  void DetachFromCurrentSequence() override;

 private:
  const std::optional<url::Origin> request_initiator_;
  const std::optional<bool> was_initial_request_same_site_;
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_METRICS_URL_LOADER_THROTTLE_H_
