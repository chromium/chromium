// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/google/core/common/google_util.h"
#include "components/safe_search_api/safe_search_util.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/mojom/x_frame_options.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_urls.h"
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
const char kCCTClientDataHeader[] = "X-CCT-Client-Data";
#endif

}  // namespace

// static
void GoogleURLLoaderThrottle::UpdateCorsExemptHeader(
    network::mojom::NetworkContextParams* params) {
  params->cors_exempt_header_list.push_back(
      safe_search_api::kGoogleAppsAllowedDomains);
  params->cors_exempt_header_list.push_back(
      safe_search_api::kYouTubeRestrictHeaderName);
#if BUILDFLAG(IS_ANDROID)
  params->cors_exempt_header_list.push_back(kCCTClientDataHeader);
#endif
}

GoogleURLLoaderThrottle::GoogleURLLoaderThrottle(
#if BUILDFLAG(IS_ANDROID)
    const std::string& client_data_header,
#endif
    chrome::mojom::DynamicParams dynamic_params)
    :
#if BUILDFLAG(IS_ANDROID)
      client_data_header_(client_data_header),
#endif
      dynamic_params_(std::move(dynamic_params)) {
}

GoogleURLLoaderThrottle::~GoogleURLLoaderThrottle() = default;

void GoogleURLLoaderThrottle::DetachFromCurrentSequence() {}

void GoogleURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (dynamic_params_.force_safe_search) {
    GURL new_url;
    safe_search_api::ForceGoogleSafeSearch(request->url, &new_url);
    if (!new_url.is_empty())
      request->url = new_url;
  }

  static_assert(safe_search_api::YOUTUBE_RESTRICT_OFF == 0,
                "OFF must be first");
  if (dynamic_params_.youtube_restrict >
          safe_search_api::YOUTUBE_RESTRICT_OFF &&
      dynamic_params_.youtube_restrict <
          safe_search_api::YOUTUBE_RESTRICT_COUNT) {
    safe_search_api::ForceYouTubeRestrict(
        request->url, &request->cors_exempt_headers,
        static_cast<safe_search_api::YouTubeRestrictMode>(
            dynamic_params_.youtube_restrict));
  }

  if (!dynamic_params_.allowed_domains_for_apps.empty() &&
      request->url.DomainIs("google.com")) {
    request->cors_exempt_headers.SetHeader(
        safe_search_api::kGoogleAppsAllowedDomains,
        dynamic_params_.allowed_domains_for_apps);
  }

#if BUILDFLAG(IS_ANDROID)
  if (!client_data_header_.empty() &&
      google_util::IsGoogleAssociatedDomainUrl(request->url)) {
    request->cors_exempt_headers.SetHeader(kCCTClientDataHeader,
                                           client_data_header_);
  }
#endif
}

void GoogleURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* /* defer */,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  // URLLoaderThrottles can only change the redirect URL when the network
  // service is enabled. The non-network service path handles this in
  // ChromeNetworkDelegate.
  if (dynamic_params_.force_safe_search) {
    safe_search_api::ForceGoogleSafeSearch(redirect_info->new_url,
                                           &redirect_info->new_url);
  }

  if (dynamic_params_.youtube_restrict >
          safe_search_api::YOUTUBE_RESTRICT_OFF &&
      dynamic_params_.youtube_restrict <
          safe_search_api::YOUTUBE_RESTRICT_COUNT) {
    safe_search_api::ForceYouTubeRestrict(
        redirect_info->new_url, modified_cors_exempt_headers,
        static_cast<safe_search_api::YouTubeRestrictMode>(
            dynamic_params_.youtube_restrict));
  }

  if (!dynamic_params_.allowed_domains_for_apps.empty() &&
      redirect_info->new_url.DomainIs("google.com")) {
    modified_cors_exempt_headers->SetHeader(
        safe_search_api::kGoogleAppsAllowedDomains,
        dynamic_params_.allowed_domains_for_apps);
  }

#if BUILDFLAG(IS_ANDROID)
  if (!client_data_header_.empty() &&
      !google_util::IsGoogleAssociatedDomainUrl(redirect_info->new_url)) {
    to_be_removed_headers->push_back(kCCTClientDataHeader);
  }
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void GoogleURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // Built-in additional protection for the chrome web store origin by ensuring
  // that the X-Frame-Options protection mechanism is set to either DENY or
  // SAMEORIGIN.
  if (response_url.SchemeIsHTTPOrHTTPS() &&
      extension_urls::IsWebstoreDomain(response_url)) {
    // TODO(mkwst): Consider shifting this to a NavigationThrottle rather than
    // relying on implicit ordering between this check and the time at which
    // ParsedHeaders is created.
    CHECK(response_head);
    CHECK(response_head->parsed_headers);
    if (response_head->parsed_headers->xfo !=
        network::mojom::XFrameOptionsValue::kDeny) {
      response_head->headers->SetHeader("X-Frame-Options", "SAMEORIGIN");
      response_head->parsed_headers->xfo =
          network::mojom::XFrameOptionsValue::kSameOrigin;
    }
  }
}
#endif
