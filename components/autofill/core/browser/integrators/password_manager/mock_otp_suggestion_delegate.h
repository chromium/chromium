// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_OTP_SUGGESTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_OTP_SUGGESTION_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/integrators/password_manager/otp_suggestion_delegate.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockOtpSuggestionDelegate : public OtpSuggestionDelegate {
 public:
  MockOtpSuggestionDelegate();
  MockOtpSuggestionDelegate(const MockOtpSuggestionDelegate&) = delete;
  MockOtpSuggestionDelegate& operator=(const MockOtpSuggestionDelegate&) =
      delete;
  ~MockOtpSuggestionDelegate() override;

  MOCK_METHOD(bool,
              IsFieldEligibleForOtpFilling,
              (const FormGlobalId& form_id, const FieldGlobalId& field_id),
              (const, override));
  MOCK_METHOD(void,
              GetOtpSuggestions,
              (const FormGlobalId& form_id,
               const FieldGlobalId& field_id,
               base::OnceCallback<void(std::vector<std::string>)> callback),
              (const, override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PASSWORD_MANAGER_MOCK_OTP_SUGGESTION_DELEGATE_H_
