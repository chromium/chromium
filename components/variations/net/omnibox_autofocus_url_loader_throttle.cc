// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/omnibox_autofocus_url_loader_throttle.h"

#include "components/variations/net/omnibox_autofocus_http_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace variations {

OmniboxAutofocusURLLoaderThrottle::OmniboxAutofocusURLLoaderThrottle() =
    default;
OmniboxAutofocusURLLoaderThrottle::~OmniboxAutofocusURLLoaderThrottle() =
    default;

// static
void OmniboxAutofocusURLLoaderThrottle::AppendThrottleIfNeeded(
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kReportOmniboxAutofocusHeader)) {
    return;
  }
  throttles->push_back(std::make_unique<OmniboxAutofocusURLLoaderThrottle>());
#endif  // BUILDFLAG(IS_ANDROID)
}

void OmniboxAutofocusURLLoaderThrottle::DetachFromCurrentSequence() {}

void OmniboxAutofocusURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
#if BUILDFLAG(IS_ANDROID)
  AppendOmniboxAutofocusHeaderIfNeeded(request->url, request);
#endif  // BUILDFLAG(IS_ANDROID)
}

void OmniboxAutofocusURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  // Note: No need to check the kReportOmniboxAutofocusHeader
  // feature state here, as this class is only instantiated when the feature is
  // enabled.
 #if BUILDFLAG(IS_ANDROID)
  if (!ShouldAppendHeader(redirect_info->new_url)) {
    to_be_removed_headers->push_back(kOmniboxAutofocusHeaderName);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace variations
