// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/content/captive_portal_url_loader_throttle.h"

#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "services/network/public/cpp/resource_request.h"

namespace captive_portal {

CaptivePortalURLLoaderThrottle::CaptivePortalURLLoaderThrottle(
    content::WebContents* web_contents) {
  is_captive_portal_window_ =
      web_contents && CaptivePortalTabHelper::FromWebContents(web_contents) &&
      CaptivePortalTabHelper::FromWebContents(web_contents)
          ->is_captive_portal_window();
}

void CaptivePortalURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (!is_captive_portal_window_)
    return;

  if (!request->trusted_params)
    request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->disable_secure_dns = true;
}

}  // namespace captive_portal
