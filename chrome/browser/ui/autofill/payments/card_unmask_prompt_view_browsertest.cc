// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_prompt_view_tester.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace autofill {

namespace {

// Forms of the dialog that can be invoked.
constexpr const char kExpiryExpired[] = "expired";
constexpr const char kExpiryValidTemporaryError[] = "valid_TemporaryError";
constexpr const char kExpiryValidPermanentError[] = "valid_PermanentError";

class TestCardUnmaskDelegate : public CardUnmaskDelegate {
 public:
  TestCardUnmaskDelegate() {}

  virtual ~TestCardUnmaskDelegate() {}

  // CardUnmaskDelegate:
  void OnUnmaskPromptAccepted(
      const UserProvidedUnmaskDetails& details) override {
    details_ = details;
  }
  void OnUnmaskPromptClosed() override {}

  base::WeakPtr<TestCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  UserProvidedUnmaskDetails details() { return details_; }

 private:
  UserProvidedUnmaskDetails details_;

  base::WeakPtrFactory<TestCardUnmaskDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestCardUnmaskDelegate);
};

class TestCardUnmaskPromptController : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptController(
      content::WebContents* contents,
      scoped_refptr<content::MessageLoopRunner> runner)
      : CardUnmaskPromptControllerImpl(
            user_prefs::UserPrefs::Get(contents->GetBrowserContext()),
            false),
        runner_(runner) {}

  // CardUnmaskPromptControllerImpl:.
  // When the confirm button is clicked.
  void OnUnmaskPromptAccepted(const base::string16& cvc,
                              const base::string16& exp_month,
                              const base::string16& exp_year,
                              bool should_store_pan,
                              bool enable_fido_auth) override {
    // Call the original implementation.
    CardUnmaskPromptControllerImpl::OnUnmaskPromptAccepted(
        cvc, exp_month, exp_year, should_store_pan, enable_fido_auth);

    // Wait some time and show verification result. An empty message means
    // success is shown.
    base::string16 verification_message;
    if (expected_failure_temporary_) {
      verification_message = base::ASCIIToUTF16("Check your CVC and try again");
    } else if (expected_failure_permanent_) {
      verification_message =
          base::ASCIIToUTF16("This card can't be verified right now.");
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestCardUnmaskPromptController::ShowVerificationResult,
                       weak_factory_.GetWeakPtr(), verification_message,
                       /*allow_retry=*/expected_failure_temporary_),
        GetSuccessMessageDuration());
  }

  base::TimeDelta GetSuccessMessageDuration() const override {
    // Change this to ~4000 if you're in --test-launcher-interactive mode and
    // would like to see the progress/success overlay.
    return base::TimeDelta::FromMilliseconds(10);
  }

  AutofillClient::PaymentsRpcResult GetVerificationResult() const override {
    if (expected_failure_temporary_)
      return AutofillClient::TRY_AGAIN_FAILURE;
    if (expected_failure_permanent_)
      return AutofillClient::PERMANENT_FAILURE;

    return AutofillClient::SUCCESS;
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
  void ShowVerificationResult(const base::string16 verification_message,
                              bool allow_retry) {
    // It's possible the prompt has been closed.
    if (!view())
      return;
    view()->GotVerificationResult(verification_message, allow_retry);
  }

  bool expected_failure_temporary_ = false;
  bool expected_failure_permanent_ = false;
  scoped_refptr<content::MessageLoopRunner> runner_;
  base::WeakPtrFactory<TestCardUnmaskPromptController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestCardUnmaskPromptController);
};

class CardUnmaskPromptViewBrowserTest : public DialogBrowserTest {
 public:
  CardUnmaskPromptViewBrowserTest() {}

  ~CardUnmaskPromptViewBrowserTest() override {}

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    runner_ = new content::MessageLoopRunner;
    contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    controller_ =
        std::make_unique<TestCardUnmaskPromptController>(contents_, runner_);
    delegate_ = std::make_unique<TestCardUnmaskDelegate>();
  }

  void ShowUi(const std::string& name) override {
    CreditCard card = test::GetMaskedServerCard();
    if (name == kExpiryExpired)
      card.SetExpirationYear(2016);

    controller()->ShowPrompt(
        base::Bind(&CreateCardUnmaskPromptView, base::Unretained(controller()),
                   base::Unretained(contents())),
        card, AutofillClient::UNMASK_FOR_AUTOFILL, delegate()->GetWeakPtr());
    // Setting error expectations and confirming the dialogs for some test
    // cases.
    if (name == kExpiryValidPermanentError ||
        name == kExpiryValidTemporaryError) {
      controller()->set_expected_verification_failure(
          /*allow_retry*/ name == kExpiryValidTemporaryError);
      CardUnmaskPromptViewTester::For(controller()->view())
          ->EnterCVCAndAccept();
    }
  }

  void FreeDelegate() { delegate_.reset(); }

  content::WebContents* contents() { return contents_; }
  TestCardUnmaskPromptController* controller() { return controller_.get(); }
  TestCardUnmaskDelegate* delegate() { return delegate_.get(); }

 protected:
  // This member must outlive the controller.
  scoped_refptr<content::MessageLoopRunner> runner_;

 private:
  content::WebContents* contents_;
  std::unique_ptr<TestCardUnmaskPromptController> controller_;
  std::unique_ptr<TestCardUnmaskDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewBrowserTest);
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
  controller()->OnUnmaskPromptAccepted(
      base::ASCIIToUTF16("123"), base::ASCIIToUTF16("10"),
      base::ASCIIToUTF16("2020"), /*should_store_locally=*/false,
      /*enable_fido_auth=*/false);
  EXPECT_EQ(base::ASCIIToUTF16("123"), delegate()->details().cvc);
  controller()->OnVerificationResult(AutofillClient::SUCCESS);

  // Simulate the user clicking [x] before the "Success!" message disappears.
  CardUnmaskPromptViewTester::For(controller()->view())->Close();
  // Wait a little while; there should be no crash.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  // Simulate AutofillManager (the delegate in production code) being destroyed
  // before CardUnmaskPromptViewBridge::OnConstrainedWindowClosed() is called.
  FreeDelegate();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  content::RunAllPendingInMessageLoop();
}

}  // namespace

}  // namespace autofill
