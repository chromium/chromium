// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/cross_otr_observer.h"
#include <memory>
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace url_param_filter {

const char kCrossOtrResponseCodeMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";

void CrossOtrObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    const NavigateParams& params) {
  if (params.privacy_sensitivity ==
          NavigateParams::PrivacySensitivity::CROSS_OTR &&
      params.started_from_context_menu &&
      !ui::PageTransitionCoreTypeIs(params.transition,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
      !web_contents->GetUserData(CrossOtrObserver::kUserDataKey)) {
    web_contents->SetUserData(CrossOtrObserver::kUserDataKey,
                              std::make_unique<CrossOtrObserver>(web_contents));
  }
}

CrossOtrObserver::CrossOtrObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void CrossOtrObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers) {
    base::UmaHistogramSparse(
        kCrossOtrResponseCodeMetricName,
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
  }
  web_contents()->RemoveUserData(CrossOtrObserver::kUserDataKey);
  // DO NOT add code past this point. `this` is destroyed.
}

void CrossOtrObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers) {
    base::UmaHistogramSparse(
        kCrossOtrResponseCodeMetricName,
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
  }
}

}  // namespace url_param_filter
