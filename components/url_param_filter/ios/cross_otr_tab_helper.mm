// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/ios/cross_otr_tab_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/url_param_filter/core/cross_otr_observer.h"
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

}  // namespace

CrossOtrTabHelper::CrossOtrTabHelper(web::WebState* web_state)
    : CrossOtrObserver(ObserverType::kIos) {
  DCHECK(web_state);
  web_state->AddObserver(this);
}

CrossOtrTabHelper::~CrossOtrTabHelper() = default;

void CrossOtrTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  OnNavigationStart(/*is_primary_frame=*/absl::nullopt,
                    /*user_activated=*/navigation_context->HasUserGesture(),
                    /*is_client_redirect=*/
                    (navigation_context->GetPageTransition() &
                     ui::PAGE_TRANSITION_CLIENT_REDIRECT),
                    /*init_cross_otr_on_ios=*/
                    IsOpenInIncognito(web_state, navigation_context));
}

void CrossOtrTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  bool should_detach = OnNavigationFinish(
      /*is_primary_frame=*/absl::nullopt,
      /*is_same_document=*/navigation_context->IsSameDocument(),
      /*headers=*/navigation_context->GetResponseHeaders(),
      /*is_reload=*/
      ui::PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD),
      /*has_committed=*/navigation_context->HasCommitted());
  if (should_detach) {
    Detach(web_state);
    // DO NOT add code past this point. `this` is destroyed.
  }
}

void CrossOtrTabHelper::WebStateDestroyed(web::WebState* web_state) {
  Detach(web_state);
  // DO NOT add code past this point. `this` is destroyed.
}

void CrossOtrTabHelper::SetExperimentalStatus(
    ClassificationExperimentStatus status) {
  // Since this TabHelper is only created when params are filtered, always set
  // did_filter_params_ = true.
  SetDidFilterParams(true, status);
}

void CrossOtrTabHelper::Detach(web::WebState* web_state) {
  WriteRefreshMetric();
  web_state->RemoveObserver(this);
  web_state->RemoveUserData(CrossOtrTabHelper::UserDataKey());
}

WEB_STATE_USER_DATA_KEY_IMPL(CrossOtrTabHelper)

}  // namespace url_param_filter
