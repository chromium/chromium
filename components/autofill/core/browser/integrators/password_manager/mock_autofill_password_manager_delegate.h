// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_AUTOFILL_PASSWORD_MANAGER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_AUTOFILL_PASSWORD_MANAGER_DELEGATE_H_

#include "components/autofill/core/browser/integrators/password_manager/autofill_password_manager_delegate.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillPasswordManagerDelegate
    : public AutofillPasswordManagerDelegate {
 public:
  MockAutofillPasswordManagerDelegate();
  MockAutofillPasswordManagerDelegate(
      const MockAutofillPasswordManagerDelegate&) = delete;
  MockAutofillPasswordManagerDelegate& operator=(
      const MockAutofillPasswordManagerDelegate&) = delete;
  ~MockAutofillPasswordManagerDelegate() override;

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
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_AUTOFILL_PASSWORD_MANAGER_DELEGATE_H_
