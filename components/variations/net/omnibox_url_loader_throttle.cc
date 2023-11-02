// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/omnibox_url_loader_throttle.h"

#include "components/variations/net/omnibox_http_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace variations {

OmniboxURLLoaderThrottle::OmniboxURLLoaderThrottle() = default;
OmniboxURLLoaderThrottle::~OmniboxURLLoaderThrottle() = default;

// static
void OmniboxURLLoaderThrottle::AppendThrottleIfNeeded(
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  if (!base::FeatureList::IsEnabled(kReportOmniboxOnDeviceSuggestionsHeader))
    return;

  // Don't add the URL loader throttle if there's no header to report. This is a
  // minor optimization, and also prevents some noisy DCHECKs from failing in
  // tests: https://cs.chromium.org/search/?q=crbug.com/845683
  const std::string header = GetHeaderValue();
  if (header.empty())
    return;

  throttles->push_back(std::make_unique<OmniboxURLLoaderThrottle>());
}

void OmniboxURLLoaderThrottle::DetachFromCurrentSequence() {}

void OmniboxURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  AppendOmniboxOnDeviceSuggestionsHeaderIfNeeded(request->url, request);
}

void OmniboxURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  // Note: No need to check the kReportOmniboxOnDeviceSuggestionsHeader feature
  // state here, as this class is only instantiated when the feature is enabled.
  if (!ShouldAppendHeader(redirect_info->new_url))
    to_be_removed_headers->push_back(kOmniboxOnDeviceSuggestionsHeader);
}

}  // namespace variations
