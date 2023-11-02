// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentHandlerHeaderViewUITest : public PaymentRequestBrowserTestBase {
 public:
  PaymentHandlerHeaderViewUITest() = default;
  ~PaymentHandlerHeaderViewUITest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerHeaderViewUITest, BasicHeader) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "install()"));

  ResetEventWaiterForDialogOpened();
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "paymentRequestWithOptions({requestShipping: true})"));
  WaitForObservedEvent();

  EXPECT_TRUE(IsPayButtonEnabled());
  EXPECT_FALSE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));

  ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                               DialogEvent::PROCESSING_SPINNER_HIDDEN,
                               DialogEvent::PAYMENT_HANDLER_WINDOW_OPENED});
  ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON);

  EXPECT_TRUE(IsViewVisible(DialogViewID::BACK_BUTTON));
  EXPECT_TRUE(IsViewVisible(DialogViewID::PAYMENT_APP_OPENED_WINDOW_SHEET));

  NavigateTo("/payment_handler.html");
}

}  // namespace
}  // namespace payments
