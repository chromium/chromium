// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_observer_bridge.h"

#include "base/logging.h"
#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
FormActivityObserverBridge::FormActivityObserverBridge(
    web::WebState* web_state,
    id<FormActivityObserver> owner)
    : web_state_(web_state), owner_(owner) {
  FormActivityTabHelper::GetOrCreateForWebState(web_state)->AddObserver(this);
}

FormActivityObserverBridge::~FormActivityObserverBridge() {
  FormActivityTabHelper::GetOrCreateForWebState(web_state_)
      ->RemoveObserver(this);
}

void FormActivityObserverBridge::FormActivityRegistered(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const FormActivityParams& params) {
  DCHECK_EQ(web_state, web_state_);
  if ([owner_ respondsToSelector:@selector
              (webState:didRegisterFormActivity:inFrame:)]) {
    [owner_ webState:web_state
        didRegisterFormActivity:params
                        inFrame:sender_frame];
  }
}

void FormActivityObserverBridge::DocumentSubmitted(web::WebState* web_state,
                                                   web::WebFrame* sender_frame,
                                                   const std::string& form_name,
                                                   const std::string& form_data,
                                                   bool has_user_gesture,
                                                   bool form_in_main_frame) {
  DCHECK_EQ(web_state, web_state_);
  if ([owner_ respondsToSelector:@selector
              (webState:didSubmitDocumentWithFormNamed:withData:hasUserGesture
                          :formInMainFrame:inFrame:)]) {
    [owner_ webState:web_state
        didSubmitDocumentWithFormNamed:form_name
                              withData:form_data
                        hasUserGesture:has_user_gesture
                       formInMainFrame:form_in_main_frame
                               inFrame:sender_frame];
  }
}
}  // namespace autofill
