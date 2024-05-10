// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_MOCK_PASSWORD_AUTOFILL_AGENT_DELEGATE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_MOCK_PASSWORD_AUTOFILL_AGENT_DELEGATE_H_

#import <string>

#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/password_autofill_agent.h"
#import "testing/gmock/include/gmock/gmock.h"

namespace web {
class WebFrame;
}

namespace autofill {

class MockPasswordAutofillAgentDelegate : public PasswordAutofillAgentDelegate {
 public:
  MockPasswordAutofillAgentDelegate();
  ~MockPasswordAutofillAgentDelegate() override;

  MockPasswordAutofillAgentDelegate(const MockPasswordAutofillAgentDelegate&) =
      delete;
  MockPasswordAutofillAgentDelegate& operator=(
      const MockPasswordAutofillAgentDelegate&) = delete;

  MOCK_METHOD(void,
              DidFillField,
              (web::WebFrame*,
               std::optional<FormRendererId>,
               FieldRendererId,
               const std::u16string&),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_MOCK_PASSWORD_AUTOFILL_AGENT_DELEGATE_H_
