// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_TEST_CARD_UNMASK_PROMPT_WAITER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_TEST_CARD_UNMASK_PROMPT_WAITER_H_

#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestCardUnmaskPromptControllerImpl;

// RAII type that injects a `TestCardUnmaskPromptController` with the ability to
// wait until the CVC prompt is shown. It also helps to accept the CVC prompt
// with a given CVC value.
// Example:
//   TestCardUnmaskPromptWaiter test_card_unmask_prompt_waiter(
//          web_contents,
//          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext()));
//   ASSERT_TRUE(test_card_unmask_prompt_waiter.Wait());
//   test_card_unmask_prompt_waiter.EnterAndAcceptCvcDialog(cvc);
class TestCardUnmaskPromptWaiter {
 public:
  explicit TestCardUnmaskPromptWaiter(content::WebContents* web_contents);
  ~TestCardUnmaskPromptWaiter();

  // Blocks until the prompt is shown.
  testing::AssertionResult Wait();

  bool EnterAndAcceptCvcDialog(const std::u16string& cvc);

 private:
  raw_ptr<autofill::payments::ChromePaymentsAutofillClient> client_;
  raw_ptr<TestCardUnmaskPromptControllerImpl> injected_controller_;
  std::unique_ptr<autofill::CardUnmaskPromptControllerImpl> old_controller_;
};

}  // namespace autofill
#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_TEST_CARD_UNMASK_PROMPT_WAITER_H_
