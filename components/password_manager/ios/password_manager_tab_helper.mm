// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_manager_tab_helper.h"

#import "base/metrics/histogram_functions.h"
#import "components/password_manager/ios/password_form_helper.h"

namespace password_manager {

// static
PasswordManagerTabHelper* PasswordManagerTabHelper::GetOrCreateForWebState(
    web::WebState* web_state) {
  PasswordManagerTabHelper* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
    DCHECK(helper);
  }
  return helper;
}

PasswordManagerTabHelper::PasswordManagerTabHelper(web::WebState* web_state) {}
PasswordManagerTabHelper::~PasswordManagerTabHelper() = default;

void PasswordManagerTabHelper::ScriptMessageReceived(
    const web::ScriptMessage& message) {
  HandleSubmittedFormStatus status =
      [password_form_helper_ handleFormSubmittedMessage:message];
  base::UmaHistogramEnumeration(kHandleFormSubmitEventHistogram, status);
}

void PasswordManagerTabHelper::SetFormHelper(PasswordFormHelper* form_helper) {
  password_form_helper_ = form_helper;
}

WEB_STATE_USER_DATA_KEY_IMPL(PasswordManagerTabHelper)

}  // namespace password_manager
