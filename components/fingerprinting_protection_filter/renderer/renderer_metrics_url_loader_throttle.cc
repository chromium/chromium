// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_metrics_url_loader_throttle.h"

#include <optional>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace fingerprinting_protection_filter {

RendererMetricsURLLoaderThrottle::RendererMetricsURLLoaderThrottle(
    RendererThrottleCreationResult result,
    std::optional<url::Origin> request_initiator,
    const GURL& initial_request_url)
    : request_initiator_(std::move(request_initiator)),
      was_initial_request_same_site_(
          request_initiator_.has_value()
              ? std::make_optional(net::SchemefulSite::IsSameSite(
                    request_initiator_.value(),
                    url::Origin::Create(initial_request_url)))
              : std::nullopt) {
  UMA_HISTOGRAM_ENUMERATION(
      "FingerprintingProtection.RendererThrottleCreationResult", result);
}

RendererMetricsURLLoaderThrottle::~RendererMetricsURLLoaderThrottle() = default;

void RendererMetricsURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  if (!request_initiator_.has_value()) {
    return;
  }

  bool is_redirect_same_site = net::SchemefulSite::IsSameSite(
      request_initiator_.value(), url::Origin::Create(redirect_info->new_url));

  RendererThrottleRedirects bucket;
  if (was_initial_request_same_site_.value()) {
    bucket = is_redirect_same_site
                 ? RendererThrottleRedirects::kSameSiteToSameSiteRedirect
                 : RendererThrottleRedirects::kSameSiteToCrossSiteRedirect;
  } else {
    bucket = is_redirect_same_site
                 ? RendererThrottleRedirects::kCrossSiteToSameSiteRedirect
                 : RendererThrottleRedirects::kCrossSiteToCrossSiteRedirect;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "FingerprintingProtection.RendererThrottleRedirects", bucket);
}

void RendererMetricsURLLoaderThrottle::DetachFromCurrentSequence() {}

}  // namespace fingerprinting_protection_filter
