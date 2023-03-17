// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_TAB_HELPER_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_TAB_HELPER_H_

#include "base/observer_list.h"
#include "base/values.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class ScriptMessage;
class WebState;
}  // namespace web

namespace autofill {

class FormActivityObserver;

// Processes user activity messages for web page forms and forwards the form
// activity event to FormActivityObserver.
class FormActivityTabHelper
    : public web::WebStateUserData<FormActivityTabHelper> {
 public:
  FormActivityTabHelper(const FormActivityTabHelper&) = delete;
  FormActivityTabHelper& operator=(const FormActivityTabHelper&) = delete;

  ~FormActivityTabHelper() override;

  static FormActivityTabHelper* GetOrCreateForWebState(
      web::WebState* web_state);

  // Handler for "form.*" JavaScript command. Dispatch to more specific handler.
  void OnFormMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message);

  // Observer registration methods.
  virtual void AddObserver(FormActivityObserver* observer);
  virtual void RemoveObserver(FormActivityObserver* observer);

 private:
  friend class web::WebStateUserData<FormActivityTabHelper>;

  // TestFormActivityTabHelper can be used by tests that want to simulate form
  // events without loading page and executing JavaScript.
  // To trigger events, TestFormActivityTabHelper will access |observer_|.
  friend class TestFormActivityTabHelper;

  explicit FormActivityTabHelper(web::WebState* web_state);

  // Handler for form activity.
  void HandleFormActivity(web::WebState* web_state,
                          const web::ScriptMessage& message);

  // Handler for form removal.
  void HandleFormRemoval(web::WebState* web_state,
                         const web::ScriptMessage& message);

  // Handler for the submission of a form.
  void FormSubmissionHandler(web::WebState* web_state,
                             const web::ScriptMessage& message);

  // The observers.
  base::ObserverList<FormActivityObserver>::Unchecked observers_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_FORM_ACTIVITY_TAB_HELPER_H_
