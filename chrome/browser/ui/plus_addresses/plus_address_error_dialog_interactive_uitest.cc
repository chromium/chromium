// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/callback.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/plus_addresses/plus_address_error_dialog.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace plus_addresses {

namespace {

class PlusAddressErrorDialogInteractiveUiTest : public InteractiveBrowserTest {
 public:
  PlusAddressErrorDialogInteractiveUiTest() = default;

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  InteractiveBrowserTestApi::StepBuilder ShowAffiliationErrorDialog(
      base::OnceClosure on_accepted) {
    return Do([this, cb = std::move(on_accepted)]() mutable {
      ShowInlineCreationAffiliationErrorDialog(web_contents(), u"foo.com",
                                               u"foo@bar.com", std::move(cb));
    });
  }

  InteractiveBrowserTestApi::StepBuilder ShowErrorDialog(
      PlusAddressErrorDialogType type,
      base::OnceClosure on_accepted) {
    return Do([this, type, cb = std::move(on_accepted)]() mutable {
      ShowInlineCreationErrorDialog(web_contents(), type, std::move(cb));
    });
  }

  InteractiveTestApi::StepBuilder CheckUserAction(std::string_view user_action,
                                                  int expected_count) {
    return Do([this, user_action, expected_count]() {
      EXPECT_EQ(user_action_tester_.GetActionCount(user_action),
                expected_count);
    });
  }

 private:
  base::UserActionTester user_action_tester_;
};

IN_PROC_BROWSER_TEST_F(PlusAddressErrorDialogInteractiveUiTest,
                       ShowAndAcceptAffiliationErrorDialog) {
  base::test::TestFuture<void> on_accepted;
  RunTestSequence(
      ShowAffiliationErrorDialog(on_accepted.GetCallback()),
      EnsurePresent(kPlusAddressErrorDialogAcceptButton),
      PressButton(kPlusAddressErrorDialogAcceptButton),
      Check([&]() { return on_accepted.IsReady(); }),
      CheckUserAction("PlusAddresses.AffiliationErrorFilledExisting", 1));
}

IN_PROC_BROWSER_TEST_F(PlusAddressErrorDialogInteractiveUiTest,
                       ShowAndCancelAffiliationErrorDialog) {
  base::test::TestFuture<void> on_accepted;
  RunTestSequence(ShowAffiliationErrorDialog(on_accepted.GetCallback()),
                  EnsurePresent(kPlusAddressErrorDialogCancelButton),
                  PressButton(kPlusAddressErrorDialogCancelButton),
                  Check([&]() { return !on_accepted.IsReady(); }),
                  CheckUserAction("PlusAddresses.AffiliationErrorCanceled", 1));
}

IN_PROC_BROWSER_TEST_F(PlusAddressErrorDialogInteractiveUiTest,
                       ShowAndAcceptTimeoutErrorDialog) {
  base::test::TestFuture<void> on_accepted;
  RunTestSequence(
      ShowErrorDialog(PlusAddressErrorDialogType::kTimeout,
                      on_accepted.GetCallback()),
      EnsurePresent(kPlusAddressErrorDialogCancelButton),
      PressButton(kPlusAddressErrorDialogAcceptButton),
      Check([&]() { return on_accepted.IsReady(); }),
      CheckUserAction("PlusAddresses.CreateErrorTryAgainClicked", 1));
}

IN_PROC_BROWSER_TEST_F(PlusAddressErrorDialogInteractiveUiTest,
                       ShowAndCancelGenericErrorDialog) {
  base::test::TestFuture<void> on_accepted;
  RunTestSequence(ShowErrorDialog(PlusAddressErrorDialogType::kGenericError,
                                  on_accepted.GetCallback()),
                  EnsurePresent(kPlusAddressErrorDialogCancelButton),
                  PressButton(kPlusAddressErrorDialogCancelButton),
                  Check([&]() { return !on_accepted.IsReady(); }),
                  CheckUserAction("PlusAddresses.CreateErrorCanceled", 1));
}

IN_PROC_BROWSER_TEST_F(PlusAddressErrorDialogInteractiveUiTest,
                       ShowAndAcceptQuotaErrorDialog) {
  base::test::TestFuture<void> on_accepted;
  // Quota error dialogs do not have a cancel button.
  RunTestSequence(ShowErrorDialog(PlusAddressErrorDialogType::kQuotaExhausted,
                                  on_accepted.GetCallback()),
                  EnsureNotPresent(kPlusAddressErrorDialogCancelButton),
                  PressButton(kPlusAddressErrorDialogAcceptButton),
                  Check([&]() { return on_accepted.IsReady(); }),
                  CheckUserAction("PlusAddresses.QuotaErrorAccepted", 1));
}

}  // namespace

}  // namespace plus_addresses
