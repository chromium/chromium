// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_IOS_INSECURE_INPUT_TAB_HELPER_H_
#define COMPONENTS_SECURITY_STATE_IOS_INSECURE_INPUT_TAB_HELPER_H_

#include <string>

#include "base/macros.h"
#include "components/autofill/ios/form_util/form_activity_observer.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// Observes user activity on forms and receives notifications about page content
// from autofill and updates the page's |SSLStatusUserData| to track insecure
// input events. Such events may change the page's Security Level.
class InsecureInputTabHelper
    : public web::WebStateObserver,
      public autofill::FormActivityObserver,
      public web::WebStateUserData<InsecureInputTabHelper> {
 public:
  ~InsecureInputTabHelper() override;

  static InsecureInputTabHelper* GetOrCreateForWebState(
      web::WebState* web_state);

  // This method should be called when the user edits a field in a non-secure
  // context.
  void DidEditFieldInInsecureContext();

 private:
  friend class web::WebStateUserData<InsecureInputTabHelper>;

  explicit InsecureInputTabHelper(web::WebState* web_state);

  // FormActivityObserver implementation.
  void FormActivityRegistered(
      web::WebState* web_state,
      web::WebFrame* sender_frame,
      const autofill::FormActivityParams& params) override;

  // WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(InsecureInputTabHelper);
};

#endif  // COMPONENTS_SECURITY_STATE_IOS_INSECURE_INPUT_TAB_HELPER_H_
