// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_TEST_MOCK_AUTOFILL_AI_VERIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_TEST_MOCK_AUTOFILL_AI_VERIFICATION_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_verification_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillAiVerificationView : public AutofillAiVerificationView {
 public:
  MockAutofillAiVerificationView();
  MockAutofillAiVerificationView(const MockAutofillAiVerificationView&) =
      delete;
  MockAutofillAiVerificationView& operator=(
      const MockAutofillAiVerificationView&) = delete;
  ~MockAutofillAiVerificationView() override;

  MOCK_METHOD(bool, Show, (ViewState initial_state), (override));
  MOCK_METHOD(void, SetState, (ViewState state), (override));
  MOCK_METHOD(void, Hide, (), (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_TEST_MOCK_AUTOFILL_AI_VERIFICATION_VIEW_H_
