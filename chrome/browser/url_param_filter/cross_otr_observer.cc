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

constexpr char kCrossOtrResponseCodeMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";
constexpr char kCrossOtrRefreshCountMetricName[] =
    "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental";

void CrossOtrObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    const NavigateParams& params) {
  if (params.privacy_sensitivity ==
          NavigateParams::PrivacySensitivity::CROSS_OTR &&
      params.started_from_context_menu &&
      !ui::PageTransitionCoreTypeIs(params.transition,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    // Inherited from WebContentsUserData and checks for an already-attached
    // instance internally.
    CrossOtrObserver::CreateForWebContents(web_contents);
  }
}

CrossOtrObserver::CrossOtrObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CrossOtrObserver>(*web_contents) {}

void CrossOtrObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // We only want the first navigation to be counted; after that point, no
  // response codes should be tracked. The observer is left in place to track
  // refreshes on the first page.
  if (!wrote_response_metric_) {
    wrote_response_metric_ = true;
    const net::HttpResponseHeaders* headers =
        navigation_handle->GetResponseHeaders();
    if (headers) {
      base::UmaHistogramSparse(
          kCrossOtrResponseCodeMetricName,
          net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
    }
  } else if (navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    refresh_count_++;
  } else if (navigation_handle->IsInPrimaryMainFrame() &&
             !navigation_handle->IsSameDocument() &&
             navigation_handle->HasCommitted()) {
    Detach();
    // DO NOT add code past this point. `this` is destroyed.
  }
}

void CrossOtrObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  // After the first navigation has committed, we no longer want to track
  // redirects.
  if (!wrote_response_metric_ && headers) {
    base::UmaHistogramSparse(
        kCrossOtrResponseCodeMetricName,
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
  }
}
void CrossOtrObserver::WebContentsDestroyed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The user has closed the tab or otherwise destroyed the web contents. Flush
  // metrics and cease observation.
  Detach();
  // DO NOT add code past this point. `this` is destroyed.
}

void CrossOtrObserver::Detach() {
  base::UmaHistogramCounts100(kCrossOtrRefreshCountMetricName, refresh_count_);
  web_contents()->RemoveUserData(CrossOtrObserver::UserDataKey());
  // DO NOT add code past this point. `this` is destroyed.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrossOtrObserver);

}  // namespace url_param_filter
