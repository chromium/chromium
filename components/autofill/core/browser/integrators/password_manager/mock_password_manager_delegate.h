// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_PASSWORD_MANAGER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_PASSWORD_MANAGER_DELEGATE_H_

#include "components/autofill/core/browser/integrators/password_manager/password_manager_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockPasswordManagerDelegate : public PasswordManagerDelegate {
 public:
  MockPasswordManagerDelegate();
  MockPasswordManagerDelegate(const MockPasswordManagerDelegate&) = delete;
  MockPasswordManagerDelegate& operator=(const MockPasswordManagerDelegate&) =
      delete;
  ~MockPasswordManagerDelegate() override;

  MOCK_METHOD((void),
              ShowSuggestions,
              (const autofill::TriggeringField&),
              (override));

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD((void),
              ShowKeyboardReplacingSurface,
              (const autofill::PasswordSuggestionRequest&),
              (override));
#endif  // BUILDFLAG(IS_ANDROID)

  MOCK_METHOD(std::optional<Suggestion>,
              GetWebauthnSignInWithAnotherDeviceSuggestion,
              (),
              (const, override));

  MOCK_METHOD(void,
              SelectSuggestion,
              (const Suggestion& suggestion),
              (override));
  MOCK_METHOD(void,
              AcceptSuggestion,
              (const Suggestion&,
               const AutofillSuggestionDelegate::SuggestionMetadata&),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_PASSWORD_MANAGER_DELEGATE_H_
