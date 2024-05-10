// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/password_autofill_agent.h"

namespace autofill {

PasswordAutofillAgent::PasswordAutofillAgent() = default;

PasswordAutofillAgent::~PasswordAutofillAgent() = default;

void PasswordAutofillAgent::DidFillField(
    web::WebFrame* frame,
    std::optional<autofill::FormRendererId> form_id,
    autofill::FieldRendererId field_id,
    const std::u16string& field_value) {
  if (delegate_) {
    delegate_->DidFillField(frame, form_id, field_id, field_value);
  }
}

void PasswordAutofillAgent::WebStateDestroyed(web::WebState* web_state) {
  delegate_ = nullptr;
  web_state->RemoveObserver(this);
}

PasswordAutofillAgent::PasswordAutofillAgent(
    web::WebState* web_state,
    PasswordAutofillAgentDelegate* delegate)
    : delegate_(delegate) {
  web_state->AddObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(PasswordAutofillAgent)

}  // namespace autofill
