// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_prompt_view_tester.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace autofill {

namespace {

// Forms of the dialog that can be invoked.
constexpr const char kExpiryExpired[] = "expired";
constexpr const char kExpiryValidTemporaryError[] = "valid_TemporaryError";
constexpr const char kExpiryValidPermanentError[] = "valid_PermanentError";

class TestCardUnmaskDelegate : public CardUnmaskDelegate {
 public:
  TestCardUnmaskDelegate() = default;

  TestCardUnmaskDelegate(const TestCardUnmaskDelegate&) = delete;
  TestCardUnmaskDelegate& operator=(const TestCardUnmaskDelegate&) = delete;

  virtual ~TestCardUnmaskDelegate() = default;

  // CardUnmaskDelegate:
  void OnUnmaskPromptAccepted(
      const UserProvidedUnmaskDetails& details) override {
    details_ = details;
  }
  void OnUnmaskPromptCancelled() override {}
  bool ShouldOfferFidoAuth() const override { return false; }

  base::WeakPtr<TestCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  UserProvidedUnmaskDetails details() { return details_; }

 private:
  UserProvidedUnmaskDetails details_;

  base::WeakPtrFactory<TestCardUnmaskDelegate> weak_factory_{this};
};

class TestCardUnmaskPromptController : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptController(
      content::WebContents* contents,
      scoped_refptr<content::MessageLoopRunner> runner,
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate)
      : CardUnmaskPromptControllerImpl(
            user_prefs::UserPrefs::Get(contents->GetBrowserContext()),
            card,
            card_unmask_prompt_options,
            delegate),
        runner_(runner) {}
  TestCardUnmaskPromptController(const TestCardUnmaskPromptController&) =
      delete;
  TestCardUnmaskPromptController& operator=(
      const TestCardUnmaskPromptController&) = delete;

  // CardUnmaskPromptControllerImpl:.
  // When the confirm button is clicked.
  void OnUnmaskPromptAccepted(const std::u16string& cvc,
                              const std::u16string& exp_month,
                              const std::u16string& exp_year,
                              bool enable_fido_auth,
                              bool was_checkbox_visible) override {
    // Call the original implementation.
    CardUnmaskPromptControllerImpl::OnUnmaskPromptAccepted(
        cvc, exp_month, exp_year, enable_fido_auth, was_checkbox_visible);

    // Wait some time and show verification result. An empty message means
    // success is shown.
    std::u16string verification_message;
    if (expected_failure_temporary_) {
      verification_message = u"Check your CVC and try again";
    } else if (expected_failure_permanent_) {
      verification_message = u"This card can't be verified right now.";
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestCardUnmaskPromptController::ShowVerificationResult,
                       weak_factory_.GetWeakPtr(), verification_message,
                       /*allow_retry=*/expected_failure_temporary_),
        GetSuccessMessageDuration());
  }

  base::TimeDelta GetSuccessMessageDuration() const override {
    // Change this to ~4000 if you're in --test-launcher-interactive mode and
    // would like to see the progress/success overlay.
    return base::Milliseconds(10);
  }

  payments::PaymentsAutofillClient::PaymentsRpcResult GetVerificationResult()
      const override {
    if (expected_failure_temporary_)
      return payments::PaymentsAutofillClient::PaymentsRpcResult::
          kTryAgainFailure;
    if (expected_failure_permanent_)
      return payments::PaymentsAutofillClient::PaymentsRpcResult::
          kPermanentFailure;

    return payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess;
  }

  void set_expected_verification_failure(bool allow_retry) {
    if (allow_retry) {
      expected_failure_temporary_ = true;
    } else {
      expected_failure_permanent_ = true;
    }
  }

  using CardUnmaskPromptControllerImpl::view;

 private:
  void ShowVerificationResult(const std::u16string verification_message,
                              bool allow_retry) {
    // It's possible the prompt has been closed.
    if (!view())
      return;
    view()->GotVerificationResult(verification_message, allow_retry);
  }

  bool expected_failure_temporary_ = false;
  bool expected_failure_permanent_ = false;
  const scoped_refptr<content::MessageLoopRunner> runner_;
  base::WeakPtrFactory<TestCardUnmaskPromptController> weak_factory_{this};
};

class CardUnmaskPromptViewBrowserTest : public DialogBrowserTest {
 public:
  CardUnmaskPromptViewBrowserTest() = default;

  CardUnmaskPromptViewBrowserTest(const CardUnmaskPromptViewBrowserTest&) =
      delete;
  CardUnmaskPromptViewBrowserTest& operator=(
      const CardUnmaskPromptViewBrowserTest&) = delete;

  ~CardUnmaskPromptViewBrowserTest() override = default;

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    runner_ = new content::MessageLoopRunner;

    delegate_ = std::make_unique<TestCardUnmaskDelegate>();
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    DialogBrowserTest::TearDownOnMainThread();
  }

  void ShowUi(const std::string& name) override {
    CreditCard card = test::GetMaskedServerCard();
    if (name == kExpiryExpired)
      card.SetExpirationYear(2016);

    CardUnmaskPromptOptions card_unmask_prompt_options =
        CardUnmaskPromptOptions(
            /*challenge_option=*/
            std::nullopt,
            payments::PaymentsAutofillClient::UnmaskCardReason::kAutofill);
    controller_ = std::make_unique<TestCardUnmaskPromptController>(
        contents(), runner_, card, card_unmask_prompt_options,
        delegate_->GetWeakPtr());
    controller_->ShowPrompt(base::BindOnce(&CreateCardUnmaskPromptView,
                                           base::Unretained(controller()),
                                           base::Unretained(contents())));
    // Setting error expectations and confirming the dialogs for some test
    // cases.
    if (name == kExpiryValidPermanentError ||
        name == kExpiryValidTemporaryError) {
      controller_->set_expected_verification_failure(
          /*allow_retry*/ name == kExpiryValidTemporaryError);
      CardUnmaskPromptViewTester::For(controller()->view())
          ->EnterCVCAndAccept(u"123");
    }
  }

  void FreeDelegate() { delegate_.reset(); }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  TestCardUnmaskPromptController* controller() { return controller_.get(); }
  TestCardUnmaskDelegate* delegate() { return delegate_.get(); }

 protected:
  // This member must outlive the controller.
  scoped_refptr<content::MessageLoopRunner> runner_;

 private:
  std::unique_ptr<TestCardUnmaskPromptController> controller_;
  std::unique_ptr<TestCardUnmaskDelegate> delegate_;
};

IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest, InvokeUi_expired) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest, InvokeUi_valid) {
  ShowAndVerifyUi();
}

// This dialog will show a temporary error when Confirm is clicked.
IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest,
                       InvokeUi_valid_TemporaryError) {
  ShowAndVerifyUi();
}

// This dialog will show a permanent error when Confirm is clicked.
IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest,
                       InvokeUi_valid_PermanentError) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest, DisplayUI) {
  ShowUi(kExpiryExpired);
}

// Makes sure the user can close the dialog while the verification success
// message is showing.
IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest,
                       EarlyCloseAfterSuccess) {
  ShowUi(kExpiryExpired);
  controller()->OnUnmaskPromptAccepted(u"123", u"10",
                                       base::ASCIIToUTF16(test::NextYear()),
                                       /*enable_fido_auth=*/false,
                                       /*was_checkbox_visible=*/false);
  EXPECT_EQ(u"123", delegate()->details().cvc);
  controller()->OnVerificationResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess);

  // Simulate the user clicking [x] before the "Success!" message disappears.
  CardUnmaskPromptViewTester::For(controller()->view())->Close();
  // Wait a little while; there should be no crash.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&content::MessageLoopRunner::Quit,
                     base::Unretained(runner_.get())),
      2 * controller()->GetSuccessMessageDuration());
  runner_->Run();
}

// Makes sure the tab can be closed while the dialog is showing.
// https://crbug.com/484376
IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest,
                       CloseTabWhileDialogShowing) {
  ShowUi(kExpiryExpired);
  // Simulate BrowserAutofillManager (the delegate in production code) being
  // destroyed before CardUnmaskPromptViewBridge::OnConstrainedWindowClosed() is
  // called.
  FreeDelegate();
  contents()->Close();

  content::RunAllPendingInMessageLoop();
}

}  // namespace

}  // namespace autofill
