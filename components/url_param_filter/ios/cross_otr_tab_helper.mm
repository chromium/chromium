// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/ios/cross_otr_tab_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_filterer.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "ui/base/page_transition_types.h"

namespace url_param_filter {

namespace {

// Returns true if the web_state corresponds to one where a user enters
// incognito by long-pressing on an embedded link and selecting "Open In
// Incognito".
bool IsOpenInIncognito(web::WebState* web_state,
                       web::NavigationContext* navigation_context) {
  // "Open In Incognito" that creates an OTR browser state causes a TYPED
  // transition.
  return navigation_context->HasUserGesture() &&
         ui::PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                                      ui::PAGE_TRANSITION_TYPED);
}

// TODO(https://crbug.com/1342757): Refactor this class and CrossOtrObserver to
// share logic.
void WriteRefreshMetric(ClassificationExperimentStatus experiment_status,
                        int refresh_count) {
  // If we used experimental classifications, write the experimental metric in
  // addition to the standard one for additional segmentation (default vs
  // experimental).
  if (experiment_status == ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramCounts100(
        "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental",
        refresh_count);
  }
  base::UmaHistogramCounts100("Navigation.CrossOtr.ContextMenu.RefreshCount",
                              refresh_count);
}

// TODO(https://crbug.com/1342757): Refactor this class and CrossOtrObserver to
// share logic.
void WriteResponseMetric(ClassificationExperimentStatus experiment_status,
                         int response_code) {
  // If we used experimental classifications, write the experimental metric in
  // addition to the standard one for additional segmentation (default vs
  // experimental).
  if (experiment_status == ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramSparse(
        "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental",
        response_code);
  }
  base::UmaHistogramSparse("Navigation.CrossOtr.ContextMenu.ResponseCode",
                           response_code);
}

}  // namespace

// static
void CrossOtrTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  WebStateUserData<CrossOtrTabHelper>::CreateForWebState(web_state);
}

CrossOtrTabHelper::CrossOtrTabHelper(web::WebState* web_state) {
  DCHECK(web_state);
  web_state->AddObserver(this);
}

CrossOtrTabHelper::~CrossOtrTabHelper() = default;

void CrossOtrTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!observed_response_) {
    protecting_navigations_ = IsOpenInIncognito(web_state, navigation_context);
    return;
  }

  if (!(navigation_context->GetPageTransition() &
        ui::PAGE_TRANSITION_CLIENT_REDIRECT) ||
      navigation_context->HasUserGesture()) {
    // If a navigation that isn't a client redirect occurs, or a user-activated
    // navigation occurs we stop protecting navigations.
    protecting_navigations_ = false;
  }
}

void CrossOtrTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // web::NavigationContext doesn't expose whether or not it's a main frame
  // navigation.
  if (navigation_context->IsSameDocument()) {
    // We only are concerned with non-same doc navigations.
    return;
  }
  // We only want the first navigation, including client redirects occurring
  // without having observed user activation, to be counted; after that, no
  // response codes should be tracked. The observer is left in place to track
  // refreshes on the first page.
  if (protecting_navigations_) {
    observed_response_ = true;
    const net::HttpResponseHeaders* headers =
        navigation_context->GetResponseHeaders();
    // TODO(https://crbug.com/1324194) See comment two about restricting metric
    // collection here.
    if (headers) {
      WriteResponseMetric(
          experimental_status_,
          net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
    }
    return;
  }
  if (ui::PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    refresh_count_++;
    return;
  }
  if (navigation_context->HasCommitted() && !protecting_navigations_) {
    Detach(web_state);
    // DO NOT add code past this point. `this` is destroyed.
  }
}

void CrossOtrTabHelper::WebStateDestroyed(web::WebState* web_state) {
  Detach(web_state);
  // DO NOT add code past this point. `this` is destroyed.
}

bool CrossOtrTabHelper::GetCrossOtrStateForTesting() const {
  return protecting_navigations_;
}

void CrossOtrTabHelper::SetExperimentalStatus(
    ClassificationExperimentStatus status) {
  experimental_status_ = status;
}

void CrossOtrTabHelper::Detach(web::WebState* web_state) {
  // TODO(https://crbug.com/1324194) See comment two about restricting metric
  // collection here.
  WriteRefreshMetric(experimental_status_, refresh_count_);
  web_state->RemoveObserver(this);
  web_state->RemoveUserData(CrossOtrTabHelper::UserDataKey());
}

WEB_STATE_USER_DATA_KEY_IMPL(CrossOtrTabHelper)

}  // namespace url_param_filter
