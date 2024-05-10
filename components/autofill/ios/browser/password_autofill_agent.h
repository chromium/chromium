// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_PASSWORD_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_PASSWORD_AUTOFILL_AGENT_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

// Delegate that handles things related to the Password Manager for the agent.
// This allows cutting the dependency cycle between the agent and Password
// Manager.
class PasswordAutofillAgentDelegate {
 public:
  PasswordAutofillAgentDelegate() = default;
  virtual ~PasswordAutofillAgentDelegate() = default;

  PasswordAutofillAgentDelegate(const PasswordAutofillAgentDelegate&) = delete;
  PasswordAutofillAgentDelegate& operator=(
      const PasswordAutofillAgentDelegate&) = delete;

  // Indicates that the user did fill the field with `field_value`.
  virtual void DidFillField(web::WebFrame* frame,
                            std::optional<autofill::FormRendererId> form_id,
                            autofill::FieldRendererId field_id,
                            const std::u16string& field_value) = 0;
};

// Agent that represents the Password Manager in Autofill. Plays the role of
// components/autofill/content/renderer/password_autofill_agent.h on the other
// platforms.
class PasswordAutofillAgent
    : public web::WebStateObserver,
      public web::WebStateUserData<PasswordAutofillAgent> {
 public:
  PasswordAutofillAgent();
  ~PasswordAutofillAgent() override;

  PasswordAutofillAgent(const PasswordAutofillAgent&) = delete;
  PasswordAutofillAgent& operator=(const PasswordAutofillAgent&) = delete;

  // Indicates to the agent that the user did an action on the form, e.g. fill
  // the form.
  void DidFillField(web::WebFrame* frame,
                    std::optional<autofill::FormRendererId> form_id,
                    autofill::FieldRendererId field_id,
                    const std::u16string& field_value);

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<PasswordAutofillAgent>;

  PasswordAutofillAgent(web::WebState* web_state,
                        PasswordAutofillAgentDelegate* delegate);

  raw_ptr<PasswordAutofillAgentDelegate> delegate_;
  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_PASSWORD_AUTOFILL_AGENT_H_
