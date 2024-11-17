// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"

#import "base/check_op.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"

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
                                                   const FormData& form_data,
                                                   bool has_user_gesture) {
  DCHECK_EQ(web_state, web_state_);
  if ([owner_ respondsToSelector:@selector
              (webState:
                  didSubmitDocumentWithFormData:hasUserGesture:inFrame:)]) {
    [owner_ webState:web_state
        didSubmitDocumentWithFormData:form_data
                       hasUserGesture:has_user_gesture
                              inFrame:sender_frame];
  }
}

void FormActivityObserverBridge::FormRemoved(web::WebState* web_state,
                                             web::WebFrame* sender_frame,
                                             const FormRemovalParams& params) {
  DCHECK_EQ(web_state, web_state_);
  if ([owner_ respondsToSelector:@selector(webState:
                                     didRegisterFormRemoval:inFrame:)]) {
    [owner_ webState:web_state
        didRegisterFormRemoval:params
                       inFrame:sender_frame];
  }
}
}  // namespace autofill
