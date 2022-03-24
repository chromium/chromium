// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_filter_throttle.h"

#include <memory>

#include "chrome/browser/url_param_filter/cross_otr_observer.h"
#include "chrome/browser/url_param_filter/url_param_filterer.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"

namespace url_param_filter {

void UrlParamFilterThrottle::MaybeCreateThrottle(
    content::WebContents* web_contents,
    const network::ResourceRequest& request,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttle_list) {
  if (!web_contents) {
    return;
  }
  // Only main frame navigations are in scope. We do not modify other
  // navigations.
  if (!request.is_main_frame) {
    return;
  }
  CrossOtrObserver* observer = CrossOtrObserver::FromWebContents(web_contents);
  if (observer && observer->IsCrossOtrState()) {
    throttle_list->push_back(
        std::make_unique<UrlParamFilterThrottle>(request.request_initiator));
  }
}

UrlParamFilterThrottle::UrlParamFilterThrottle(
    const absl::optional<url::Origin>& request_initiator_origin) {
  last_hop_initiator_ = request_initiator_origin.has_value()
                            ? request_initiator_origin->GetURL()
                            : GURL();
}
UrlParamFilterThrottle::~UrlParamFilterThrottle() = default;

void UrlParamFilterThrottle::DetachFromCurrentSequence() {}

void UrlParamFilterThrottle::WillStartRequest(network::ResourceRequest* request,
                                              bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  request->url = FilterUrl(last_hop_initiator_, request->url);
  last_hop_initiator_ = request->url;
}

void UrlParamFilterThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  redirect_info->new_url =
      FilterUrl(last_hop_initiator_, redirect_info->new_url);
  // Future redirects should use the redirect's domain as the navigation source.
  last_hop_initiator_ = redirect_info->new_url;
}

bool UrlParamFilterThrottle::makes_unsafe_redirect() {
  // Scheme changes are not possible with this throttle. Only URL params are
  // modified.
  return false;
}
}  // namespace url_param_filter
