// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_TAB_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_TAB_HELPER_H_

#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state_user_data.h"

@class PasswordFormHelper;

namespace password_manager {

// Histogram to keep track of the status of handling password form submit
// events which only concerns the events from buttons that aren't
// <input type="submit">.
inline constexpr char kHandleFormSubmitEventHistogram[] =
    "PasswordManager.iOS.HandleFormSubmitEvent";

// Tab helper which associates password manager's PasswordFormHelper with the
// given WebState. This allows access to the PasswordFormHelper instance given
// a WebState.
class PasswordManagerTabHelper
    : public web::WebStateUserData<PasswordManagerTabHelper> {
 public:
  PasswordManagerTabHelper(const PasswordManagerTabHelper&) = delete;
  PasswordManagerTabHelper& operator=(const PasswordManagerTabHelper&) = delete;

  ~PasswordManagerTabHelper() override;

  static PasswordManagerTabHelper* GetOrCreateForWebState(
      web::WebState* web_state);

  // Handles `message` received from the associated web state.
  void ScriptMessageReceived(const web::ScriptMessage& message);

  // Sets `form_helper` as the current PasswordFormHelper instance for the
  // associated web state.
  void SetFormHelper(PasswordFormHelper* form_helper);

 private:
  friend class web::WebStateUserData<PasswordManagerTabHelper>;

  explicit PasswordManagerTabHelper(web::WebState* web_state);

  // The current PasswordFormHelper for the associated web state.
  __weak PasswordFormHelper* password_form_helper_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_TAB_HELPER_H_
