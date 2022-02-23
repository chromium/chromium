// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestModifiersTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestModifiersTest(const PaymentRequestModifiersTest&) = delete;
  PaymentRequestModifiersTest& operator=(const PaymentRequestModifiersTest&) =
      delete;

 protected:
  PaymentRequestModifiersTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PaymentRequestBrowserTestBase::SetUpInProcessBrowserTestFixture();

    feature_list_.InitWithFeatures(
        {::features::kPaymentRequestBasicCard, features::kWebPaymentsModifiers},
        {});
  }

  size_t GetLineCount() {
    auto* top = dialog_view()->view_stack_for_testing()->top();
    const auto* content =
        top->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW));
    return content->children().size();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       NoModifierAppliedIfNoSelectedInstrument) {
  NavigateTo("/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  InvokePaymentRequestUI();
  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1u, GetLineCount());
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestModifiersTest,
    ModifierNotAppliedIfSelectedInstrumentWithoutMatchingNetwork) {
  NavigateTo("/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(
      autofill::test::GetMaskedServerCard());  // Mastercard card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  ResetEventWaiterForDialogOpened();
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { "
      "document.getElementById('visa_supported_network')."
      "click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1u, GetLineCount());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       ModifierAppliedToBasicCardWithoutTypeOrNetwork) {
  NavigateTo("/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$4.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's the total line and the discount line.
  EXPECT_EQ(2u, GetLineCount());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       ModifierAppliedToUnknownTypeWithMatchingNetwork) {
  NavigateTo("/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  // Change to Mastercard to match the test case.
  card.SetRawInfo(autofill::CREDIT_CARD_NUMBER, u"5555555555554444");
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  ResetEventWaiterForDialogOpened();
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { "
      "document.getElementById('mastercard_any_supported_type')."
      "click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$4.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's the total line and the discount line.
  EXPECT_EQ(2u, GetLineCount());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       NoTotalInModifierDoesNotCrash) {
  NavigateTo("/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  // Change to Mastercard to match the test case.
  card.SetRawInfo(autofill::CREDIT_CARD_NUMBER, u"5555555555554444");
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  ResetEventWaiterForDialogOpened();
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('no_total').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  // The price is the global total, because the modifier does not have total.
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // Only global total is available.
  EXPECT_EQ(1u, GetLineCount());
}

class PaymentRequestModifiersBasicCardDisabledTest
    : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestModifiersBasicCardDisabledTest(
      const PaymentRequestModifiersBasicCardDisabledTest&) = delete;
  PaymentRequestModifiersBasicCardDisabledTest& operator=(
      const PaymentRequestModifiersBasicCardDisabledTest&) = delete;

 protected:
  PaymentRequestModifiersBasicCardDisabledTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PaymentRequestBrowserTestBase::SetUpInProcessBrowserTestFixture();

    feature_list_.InitWithFeatures({features::kWebPaymentsModifiers},
                                   {::features::kPaymentRequestBasicCard});
  }

  size_t GetLineCount() {
    auto* top = dialog_view()->view_stack_for_testing()->top();
    const auto* content =
        top->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW));
    return content->children().size();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersBasicCardDisabledTest,
                       NoModifierAppliedIfNoSelectedInstrument) {
  // We need to control the default-selected payment app in the UI, to know that
  // the modifiers should not apply. To achieve this, we install one of the apps
  // without an icon - this makes ServiceWorkerPaymentApp::CanPreselect false
  // for it and so the other app should be selected.
  std::string payment_method_name_1;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &payment_method_name_1);
  std::string payment_method_name_2;
  InstallPaymentAppWithoutIcon("b.com", "payment_request_success_responder.js",
                               &payment_method_name_2);

  NavigateTo("/payment_request_with_modifiers_test.html");

  InvokePaymentRequestUIWithJs(
      content::JsReplace("modifierToSecondaryMethod([{supportedMethods:$1}, "
                         "{supportedMethods:$2}]);",
                         payment_method_name_1, payment_method_name_2));

  OpenOrderSummaryScreen();

  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1u, GetLineCount());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersBasicCardDisabledTest,
                       NoTotalInModifierDoesNotCrash) {
  // The modifier without the total applies to the first method; to make sure
  // that it is always selected we install the second method without an icon.
  std::string payment_method_name_1;
  InstallPaymentApp("a.com", "payment_request_success_responder.js",
                    &payment_method_name_1);
  std::string payment_method_name_2;
  InstallPaymentAppWithoutIcon("b.com", "payment_request_success_responder.js",
                               &payment_method_name_2);

  NavigateTo("/payment_request_with_modifiers_test.html");

  InvokePaymentRequestUIWithJs(content::JsReplace(
      "modifierWithNoTotal([{supportedMethods:$1}, {supportedMethods:$2}]);",
      payment_method_name_1, payment_method_name_2));

  OpenOrderSummaryScreen();

  // The price is the global total, because the modifier does not have total.
  EXPECT_EQ(u"$5.00",
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // Only global total is available.
  EXPECT_EQ(1u, GetLineCount());
}

}  // namespace payments
