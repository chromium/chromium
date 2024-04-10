// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/test_card_unmask_prompt_waiter.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_prompt_view_tester.h"

namespace autofill {

class TestCardUnmaskPromptControllerImpl
    : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptControllerImpl(
      PrefService* pref_service,
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate)
      : CardUnmaskPromptControllerImpl(pref_service,
                                       card,
                                       card_unmask_prompt_options,
                                       delegate) {}
  TestCardUnmaskPromptControllerImpl(
      const TestCardUnmaskPromptControllerImpl&) = delete;
  TestCardUnmaskPromptControllerImpl& operator=(
      const TestCardUnmaskPromptControllerImpl&) = delete;

  void ShowPrompt(CardUnmaskPromptViewFactory view_factory) override {
    CardUnmaskPromptControllerImpl::ShowPrompt(std::move(view_factory));
    run_loop_.Quit();
  }

  testing::AssertionResult Wait() {
    bool timeout = false;
    base::test::ScopedRunLoopTimeout run_loop_timeout(
        FROM_HERE, base::Seconds(10),
        base::BindRepeating(
            [](bool* timeout) {
              *timeout = new bool(true);
              return std::string(
                  "\"Enter CVC\" dialog did not pop up within timout.");
            },
            base::Unretained(&timeout)));
    run_loop_.Run();
    return timeout ? testing::AssertionFailure() : testing::AssertionSuccess();
  }

  using CardUnmaskPromptControllerImpl::view;

 private:
  base::RunLoop run_loop_;
};

TestCardUnmaskPromptWaiter::TestCardUnmaskPromptWaiter(
    content::WebContents* web_contents)
    : client_(ChromeAutofillClient::FromWebContentsForTesting(web_contents)
                  ->GetPaymentsAutofillClient()) {
  old_controller_ = client_->ExtractCardUnmaskControllerForTesting();
  auto injected_controller =
      std::make_unique<TestCardUnmaskPromptControllerImpl>(
          old_controller_->pref_service_for_testing(),
          old_controller_->card_for_testing(),
          old_controller_->card_unmask_prompt_options_for_testing(),
          old_controller_->delegate_for_testing());
  injected_controller_ = injected_controller.get();
  client_->SetCardUnmaskControllerForTesting(std::move(injected_controller));
}

TestCardUnmaskPromptWaiter::~TestCardUnmaskPromptWaiter() {
  if (injected_controller_) {
    injected_controller_ = nullptr;
    client_->SetCardUnmaskControllerForTesting(std::move(old_controller_));
  }
}

testing::AssertionResult TestCardUnmaskPromptWaiter::Wait() {
  DCHECK(injected_controller_);
  return injected_controller_->Wait();
}

bool TestCardUnmaskPromptWaiter::EnterAndAcceptCvcDialog(
    const std::u16string& cvc) {
  // Enter CVC and accept to dismiss "Enter CVC" prompt dialog with
  // TestCardUnmaskController.
  DCHECK(injected_controller_);
  bool success = false;
  if (injected_controller_->view()) {
    CardUnmaskPromptViewTester::For(injected_controller_->view())
        ->EnterCVCAndAccept(cvc);
    success = true;
  }
  injected_controller_ = nullptr;
  client_->SetCardUnmaskControllerForTesting(std::move(old_controller_));
  return success;
}

}  // namespace autofill
