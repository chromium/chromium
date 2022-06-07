// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/content/cross_otr_observer.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "ui/base/page_transition_types.h"

namespace url_param_filter {
namespace {
constexpr char kInternalRedirectHeaderStatusLine[] =
    "HTTP/1.1 307 Internal Redirect";

bool IsInternalRedirect(const net::HttpResponseHeaders* headers) {
  return base::EqualsCaseInsensitiveASCII(headers->GetStatusLine(),
                                          kInternalRedirectHeaderStatusLine);
}
}  // anonymous namespace

void CrossOtrObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents,
    bool privacy_sensitivity_is_cross_otr,
    bool started_from_context_menu,
    ui::PageTransition transition) {
  if (privacy_sensitivity_is_cross_otr && started_from_context_menu &&
      !ui::PageTransitionCoreTypeIs(transition,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    // Inherited from WebContentsUserData and checks for an already-attached
    // instance internally.
    CrossOtrObserver::CreateForWebContents(web_contents);
  }
}

CrossOtrObserver::CrossOtrObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CrossOtrObserver>(*web_contents),
      weak_factory_(this) {}

CrossOtrObserver::~CrossOtrObserver() = default;

bool CrossOtrObserver::IsCrossOtrState() const {
  return protecting_navigations_;
}

void CrossOtrObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If we've already observed the end of a navigation, and the navigation is in
  // the primary main frame, and it is not the result of a client redirect,
  // we've finished the cross-OTR case. Note that observing user activation
  // would also serve to stop the protecting_navigations_ case. Note that
  // refreshes after page load also trigger this, and thus are not at risk of
  // being considered part of the cross-OTR case.
  if (observed_response_ && navigation_handle->IsInPrimaryMainFrame() &&
      !(navigation_handle->GetPageTransition() &
        ui::PAGE_TRANSITION_CLIENT_REDIRECT)) {
    protecting_navigations_ = false;
  }
}

void CrossOtrObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    // We only are concerned with top-level, non-same doc navigations.
    return;
  }

  // We only want the first navigation, including client redirects occurring
  // without having observed user activation, to be counted; after that, no
  // response codes should be tracked. The observer is left in place to track
  // refreshes on the first page.
  if (protecting_navigations_) {
    observed_response_ = true;
    const net::HttpResponseHeaders* headers =
        navigation_handle->GetResponseHeaders();

    // Metrics will not be collected for non intervened navigation chains and
    // navigations occurring prior to params filtering.
    if (headers && did_filter_params_) {
      WriteResponseMetric(
          net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
    }
    return;
  }
  if (navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    refresh_count_++;
    return;
  }
  if (navigation_handle->HasCommitted() && !protecting_navigations_) {
    Detach();
    // DO NOT add code past this point. `this` is destroyed.
  }
}

void CrossOtrObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    // We only are concerned with top-level, non-same doc navigations.
    return;
  }

  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  // After the first full navigation has committed, including any client
  // redirects that occur without user activation, we no longer want to track
  // redirects.
  // Metrics will not be collected for non intervened navigation chains and
  // navigations occurring prior to params filtering.
  if (protecting_navigations_ && headers && did_filter_params_ &&
      !IsInternalRedirect(headers)) {
    WriteResponseMetric(
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

void CrossOtrObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Anytime the user activates a frame in the web contents, we cease to
  // consider the case cross-OTR.
  protecting_navigations_ = false;
}

void CrossOtrObserver::Detach() {
  // Metrics will not be collected for non intervened navigation chains and
  // navigations occurring prior to params filtering.
  if (did_filter_params_) {
    WriteRefreshMetric(refresh_count_);
  }
  web_contents()->RemoveUserData(CrossOtrObserver::UserDataKey());
  // DO NOT add code past this point. `this` is destroyed.
}

base::WeakPtr<CrossOtrObserver> CrossOtrObserver::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void CrossOtrObserver::SetDidFilterParams(
    bool value,
    ClassificationExperimentStatus experiment_status) {
  did_filter_params_ = value;
  // If we have already seen experimental params, treat all metrics as coming
  // after an experimental param classification. In other words, we consider all
  // response codes/refresh counts after an experimental param has been stripped
  // as being influenced by that experimental parameter removal.
  if (experiment_status_ != ClassificationExperimentStatus::EXPERIMENTAL) {
    experiment_status_ = experiment_status;
  }
}

void CrossOtrObserver::WriteRefreshMetric(int refresh_count) {
  // If we used experimental classifications, write the experimental metric in
  // addition to the standard one for additional segmentation (default vs
  // experimental).
  if (experiment_status_ == ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramCounts100(
        "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental",
        refresh_count);
  }
  base::UmaHistogramCounts100("Navigation.CrossOtr.ContextMenu.RefreshCount",
                              refresh_count);
}
void CrossOtrObserver::WriteResponseMetric(int response_code) {
  // If we used experimental classifications, write the experimental metric in
  // addition to the standard one for additional segmentation (default vs
  // experimental).
  if (experiment_status_ == ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramSparse(
        "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental",
        response_code);
  }
  base::UmaHistogramSparse("Navigation.CrossOtr.ContextMenu.ResponseCode",
                           response_code);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CrossOtrObserver);

}  // namespace url_param_filter
