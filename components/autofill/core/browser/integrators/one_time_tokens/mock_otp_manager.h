// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_MOCK_OTP_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_MOCK_OTP_MANAGER_H_

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockOtpManager : public OtpManager {
 public:
  MockOtpManager();
  MockOtpManager(const MockOtpManager&) = delete;
  MockOtpManager& operator=(const MockOtpManager&) = delete;
  ~MockOtpManager() override;

  MOCK_METHOD(void,
              GetOtpSuggestions,
              (OtpManager::GetOtpSuggestionsCallback callback),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_MOCK_OTP_MANAGER_H_
