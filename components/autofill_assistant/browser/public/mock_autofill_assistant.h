// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_AUTOFILL_ASSISTANT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_AUTOFILL_ASSISTANT_H_

#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAutofillAssistant : public AutofillAssistant {
 public:
  MockAutofillAssistant();
  ~MockAutofillAssistant() override;

  MOCK_METHOD(void,
              GetCapabilitiesByHashPrefix,
              (uint32_t hash_prefix_length,
               const std::vector<uint64_t>& hash_prefix,
               const std::string& intent,
               GetCapabilitiesResponseCallback callback),
              (override));
  MOCK_METHOD(std::unique_ptr<HeadlessScriptController>,
              CreateHeadlessScriptController,
              (content::WebContents * web_contents,
               ExternalActionDelegate* action_extension_delegate,
               WebsiteLoginManager* website_login_manager),
              (override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_MOCK_AUTOFILL_ASSISTANT_H_
