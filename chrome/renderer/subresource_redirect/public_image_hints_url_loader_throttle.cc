// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/public_image_hints_url_loader_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/renderer/previews/resource_loading_hints_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/renderer/render_frame.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

PublicImageHintsURLLoaderThrottle::PublicImageHintsURLLoaderThrottle(
    int render_frame_id,
    bool allowed_to_redirect)
    : SubresourceRedirectURLLoaderThrottle(render_frame_id) {
  redirect_result_ =
      allowed_to_redirect
          ? SubresourceRedirectHintsAgent::RedirectResult::kRedirectable
          : SubresourceRedirectHintsAgent::RedirectResult::
                kIneligibleOtherImage;
}

PublicImageHintsURLLoaderThrottle::~PublicImageHintsURLLoaderThrottle() =
    default;

SubresourceRedirectHintsAgent*
PublicImageHintsURLLoaderThrottle::GetSubresourceRedirectHintsAgent() {
  return subresource_redirect::SubresourceRedirectHintsAgent::Get(
      GetRenderFrame());
}

bool PublicImageHintsURLLoaderThrottle::ShouldRedirectImage(const GURL& url) {
  if (redirect_result_ !=
      SubresourceRedirectHintsAgent::RedirectResult::kRedirectable) {
    return false;
  }

  auto* subresource_redirect_hints_agent = GetSubresourceRedirectHintsAgent();
  if (!subresource_redirect_hints_agent)
    return false;

  redirect_result_ = subresource_redirect_hints_agent->ShouldRedirectImage(url);
  if (redirect_result_ !=
      SubresourceRedirectHintsAgent::RedirectResult::kRedirectable) {
    return false;
  }
  return true;
}

void PublicImageHintsURLLoaderThrottle::OnRedirectedLoadCompleteWithError() {
  redirect_result_ =
      SubresourceRedirectHintsAgent::RedirectResult::kIneligibleOtherImage;
}

void PublicImageHintsURLLoaderThrottle::RecordMetricsOnLoadFinished(
    const GURL& url,
    int64_t content_length) {
  auto* subresource_redirect_hints_agent = GetSubresourceRedirectHintsAgent();
  if (subresource_redirect_hints_agent) {
    subresource_redirect_hints_agent->RecordMetricsOnLoadFinished(
        url, content_length, redirect_result_);
  }
}

}  // namespace subresource_redirect
