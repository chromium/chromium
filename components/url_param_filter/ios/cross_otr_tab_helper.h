// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_IOS_CROSS_OTR_TAB_HELPER_H_
#define COMPONENTS_URL_PARAM_FILTER_IOS_CROSS_OTR_TAB_HELPER_H_

#include "components/url_param_filter/core/cross_otr_observer.h"
#include "components/url_param_filter/core/url_param_filterer.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"
#include "ui/base/page_transition_types.h"

namespace web {
class WebState;
class NavigationContext;
}  // namespace web

namespace url_param_filter {

// This class is created to measure the effect of experimentally filtering
// URLs. It is only attached to WebStates created via an "Open In Incognito"
// press.
//
// The state-machine logic measuring refreshes in class should be kept in sync
// with the CrossOtrObserver at components/url_param_filter/content/ which
// performs similar observations.
class CrossOtrTabHelper : public CrossOtrObserver,
                          public web::WebStateObserver,
                          public web::WebStateUserData<CrossOtrTabHelper> {
 public:
  ~CrossOtrTabHelper() override;

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Returns whether this observer is in Cross-OTR state, used for testing.
  bool GetCrossOtrStateForTesting() const;

  // Stores the experimental status of the params being filtered for use in
  // sending metrics.
  void SetExperimentalStatus(ClassificationExperimentStatus status);

 private:
  friend class WebStateUserData<CrossOtrTabHelper>;

  explicit CrossOtrTabHelper(web::WebState* web_state);

  // Flushes metrics and removes the observer from the WebState.
  void Detach(web::WebState* web_state);

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace url_param_filter

#endif  // COMPONENTS_URL_PARAM_FILTER_IOS_CROSS_OTR_TAB_HELPER_H_
