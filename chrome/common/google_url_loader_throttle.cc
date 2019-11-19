// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include "chrome/common/net/safe_search_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_urls.h"
#endif

GoogleURLLoaderThrottle::GoogleURLLoaderThrottle(
    bool is_off_the_record,
    chrome::mojom::DynamicParams dynamic_params)
    : is_off_the_record_(is_off_the_record),
      dynamic_params_(std::move(dynamic_params)) {}

GoogleURLLoaderThrottle::~GoogleURLLoaderThrottle() {}

void GoogleURLLoaderThrottle::DetachFromCurrentSequence() {}

void GoogleURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  variations::AppendVariationsHeaderWithCustomValue(
      request->url,
      is_off_the_record_ ? variations::InIncognito::kYes
                         : variations::InIncognito::kNo,
      dynamic_params_.variation_ids_header, request);

  if (dynamic_params_.force_safe_search) {
    GURL new_url;
    safe_search_util::ForceGoogleSafeSearch(request->url, &new_url);
    if (!new_url.is_empty())
      request->url = new_url;
  }

  static_assert(safe_search_util::YOUTUBE_RESTRICT_OFF == 0,
                "OFF must be first");
  if (dynamic_params_.youtube_restrict >
          safe_search_util::YOUTUBE_RESTRICT_OFF &&
      dynamic_params_.youtube_restrict <
          safe_search_util::YOUTUBE_RESTRICT_COUNT) {
    safe_search_util::ForceYouTubeRestrict(
        request->url, &request->headers,
        static_cast<safe_search_util::YouTubeRestrictMode>(
            dynamic_params_.youtube_restrict));
  }

  if (!dynamic_params_.allowed_domains_for_apps.empty() &&
      request->url.DomainIs("google.com")) {
    request->headers.SetHeader(safe_search_util::kGoogleAppsAllowedDomains,
                               dynamic_params_.allowed_domains_for_apps);
  }
}

void GoogleURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* /* defer */,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers) {
  variations::RemoveVariationsHeaderIfNeeded(*redirect_info, response_head,
                                             to_be_removed_headers);

  // URLLoaderThrottles can only change the redirect URL when the network
  // service is enabled. The non-network service path handles this in
  // ChromeNetworkDelegate.
  if (dynamic_params_.force_safe_search) {
    safe_search_util::ForceGoogleSafeSearch(redirect_info->new_url,
                                            &redirect_info->new_url);
  }

  if (dynamic_params_.youtube_restrict >
          safe_search_util::YOUTUBE_RESTRICT_OFF &&
      dynamic_params_.youtube_restrict <
          safe_search_util::YOUTUBE_RESTRICT_COUNT) {
    safe_search_util::ForceYouTubeRestrict(
        redirect_info->new_url, modified_headers,
        static_cast<safe_search_util::YouTubeRestrictMode>(
            dynamic_params_.youtube_restrict));
  }

  if (!dynamic_params_.allowed_domains_for_apps.empty() &&
      redirect_info->new_url.DomainIs("google.com")) {
    modified_headers->SetHeader(safe_search_util::kGoogleAppsAllowedDomains,
                                dynamic_params_.allowed_domains_for_apps);
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void GoogleURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  // Built-in additional protection for the chrome web store origin.
  GURL webstore_url(extension_urls::GetWebstoreLaunchURL());
  if (response_url.SchemeIsHTTPOrHTTPS() &&
      response_url.DomainIs(webstore_url.host_piece())) {
    if (response_head && response_head->headers &&
        !response_head->headers->HasHeaderValue("x-frame-options", "deny") &&
        !response_head->headers->HasHeaderValue("x-frame-options",
                                                "sameorigin")) {
      response_head->headers->RemoveHeader("x-frame-options");
      response_head->headers->AddHeader("x-frame-options: sameorigin");
    }
  }
}
#endif
