// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace payments {
namespace {

class PaymentRequestShowPromiseTest : public PaymentRequestBrowserTestBase {
 public:
  PaymentRequestShowPromiseTest(const PaymentRequestShowPromiseTest&) = delete;
  PaymentRequestShowPromiseTest& operator=(
      const PaymentRequestShowPromiseTest&) = delete;

 protected:
  PaymentRequestShowPromiseTest() = default;
  ~PaymentRequestShowPromiseTest() override = default;

  // Installs the payment handler for window.location.origin payment method that
  // responds to "paymentrequest" events by echoing back the "total" object.
  void InstallEchoPaymentHandler() {
    InstallPaymentApp("a.com", "/show_promise/app.js", &payment_method_);
  }

  // Shows the browser payment sheet.
  void ShowBrowserPaymentSheet() {
    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::SPEC_DONE_UPDATING,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::DIALOG_OPENED});
    ASSERT_TRUE(
        content::ExecJs(GetActiveWebContents(),
                        content::JsReplace("buy($1)", payment_method_)));
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(web_modal::WebContentsModalDialogManager::FromWebContents(
                    GetActiveWebContents())
                    ->IsDialogActive());
  }

  // Verifies that the payment sheet total is |total_amount_string|.
  void ExpectTotal(const std::string& total_amount_string) {
    EXPECT_EQ(base::ASCIIToUTF16(total_amount_string),
              GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  }

  // Verifies that there're no payment items.
  void ExpectNoDisplayItems() {
    views::View* view = dialog_view()->GetViewByID(
        static_cast<int>(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
    EXPECT_TRUE(!view || !view->GetVisible() ||
                static_cast<views::Label*>(view)->GetText().empty())
        << "Found unexpected display item: "
        << static_cast<views::Label*>(view)->GetText();
  }

  // Verifies that the shipping address section does not display any warning
  // messages.
  void ExpectNoShippingWarningMessage() {
    views::View* view = dialog_view()->GetViewByID(
        static_cast<int>(DialogViewID::WARNING_LABEL));
    if (!view || !view->GetVisible())
      return;

    EXPECT_EQ(std::u16string(), static_cast<views::Label*>(view)->GetText());
  }

  // Verifies that the shipping address section has |expected_message| in the
  // header.
  void ExpectShippingWarningMessage(const std::string& expected_message) {
    EXPECT_EQ(base::ASCIIToUTF16(expected_message),
              GetLabelText(DialogViewID::WARNING_LABEL));
  }

  // Selects another shipping address.
  void SelectAnotherShippingAddress() {
    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::SPEC_DONE_UPDATING});
    ClickOnChildInListViewAndWait(
        /*child_index=*/1, /*total_num_children=*/2,
        DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);
  }

  // Selects the only shipping address.
  void SelectTheOnlyShippingAddress() {
    ResetEventWaiterForSequence({DialogEvent::PROCESSING_SPINNER_SHOWN,
                                 DialogEvent::PROCESSING_SPINNER_HIDDEN,
                                 DialogEvent::SPEC_DONE_UPDATING});
    ClickOnChildInListViewAndWait(
        /*child_index=*/0, /*total_num_children=*/1,
        DialogViewID::SHIPPING_ADDRESS_SHEET_LIST_VIEW);
  }

  // Verifies that the first shipping option cost is |amount_string|.
  void ExpectShippingCost(const std::string& amount_string) {
    EXPECT_EQ(base::ASCIIToUTF16(amount_string),
              GetLabelText(DialogViewID::SHIPPING_OPTION_AMOUNT));
  }

  // Clicks the "Pay" button and waits for the dialog to close.
  void Pay() {
    ResetEventWaiterForSequence(
        {DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
    ClickOnDialogViewAndWait(DialogViewID::PAY_BUTTON, dialog_view());
  }

  std::string payment_method_;
};

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest, SingleOptionShipping) {
  NavigateTo("/show_promise/single_option_shipping.html");
  InstallEchoPaymentHandler();
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());
  ShowBrowserPaymentSheet();

  EXPECT_TRUE(IsPayButtonEnabled());

  OpenOrderSummaryScreen();

  ExpectTotal("$1.00");

  ClickOnBackArrow();
  OpenShippingAddressSectionScreen();

  ExpectNoShippingWarningMessage();

  SelectAnotherShippingAddress();

  ExpectNoShippingWarningMessage();

  ClickOnBackArrow();
  OpenShippingOptionSectionScreen();

  ExpectShippingCost("$0.00");

  ClickOnBackArrow();

  EXPECT_TRUE(IsPayButtonEnabled());

  Pay();

  ExpectBodyContains({R"({"currency":"USD","value":"1.00"})"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest,
                       SingleOptionShippingWithUpdate) {
  NavigateTo("/show_promise/single_option_shipping_with_update.html");
  InstallEchoPaymentHandler();
  AddAutofillProfile(autofill::test::GetFullProfile());
  AddAutofillProfile(autofill::test::GetFullProfile2());
  ShowBrowserPaymentSheet();

  EXPECT_TRUE(IsPayButtonEnabled());

  OpenOrderSummaryScreen();

  ExpectTotal("$1.00");

  ClickOnBackArrow();
  OpenShippingAddressSectionScreen();

  ExpectNoShippingWarningMessage();

  SelectAnotherShippingAddress();

  ExpectNoShippingWarningMessage();

  ClickOnBackArrow();
  OpenShippingOptionSectionScreen();

  ExpectShippingCost("$0.00");

  ClickOnBackArrow();

  EXPECT_TRUE(IsPayButtonEnabled());

  Pay();

  ExpectBodyContains({R"({"currency":"USD","value":"1.00"})"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest, CannotShipError) {
  NavigateTo("/show_promise/us_only_shipping.html");
  InstallEchoPaymentHandler();
  AddAutofillProfile(autofill::test::GetFullCanadianProfile());
  ShowBrowserPaymentSheet();

  EXPECT_FALSE(IsPayButtonEnabled());

  OpenOrderSummaryScreen();

  ExpectTotal("$1.00");

  ClickOnBackArrow();
  OpenShippingAddressSectionScreen();

  ExpectShippingWarningMessage(
      "To see shipping methods and requirements, select an address");

  SelectTheOnlyShippingAddress();

  ExpectShippingWarningMessage("Cannot ship outside of US.");

  ClickOnBackArrow();

  EXPECT_FALSE(IsPayButtonEnabled());

  ClickOnCancel();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest, SkipUI) {
  base::HistogramTester histogram_tester;
  NavigateTo("/show_promise/digital_goods.html");
  InstallEchoPaymentHandler();
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      content::JsReplace("create($1)", payment_method_)));
  ResetEventWaiterForSequence(
      {DialogEvent::PROCESSING_SPINNER_SHOWN,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::SPEC_DONE_UPDATING,
       DialogEvent::PROCESSING_SPINNER_HIDDEN, DialogEvent::DIALOG_OPENED,
       DialogEvent::PROCESSING_SPINNER_SHOWN, DialogEvent::DIALOG_CLOSED});
  ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "buy();"));
  ASSERT_TRUE(WaitForObservedEvent());

  ExpectBodyContains({R"({"currency":"USD","value":"1.00"})"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest, Reject) {
  NavigateTo("/show_promise/reject.html");
  InstallEchoPaymentHandler();
  EXPECT_EQ("AbortError: rejected",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("buy($1)", payment_method_)));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest, Timeout) {
  NavigateTo("/show_promise/timeout.html");
  InstallEchoPaymentHandler();
  EXPECT_EQ(
      "AbortError: Timed out waiting for a PaymentRequest.show(promise) to "
      "resolve.",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("buy($1)", payment_method_)));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest,
                       UnsupportedPaymentMethod) {
  NavigateTo("/show_promise/unsupported.html");
  EXPECT_EQ(R"(NotSupportedError: The payment method "foo" is not supported.)",
            content::EvalJs(GetActiveWebContents(), "buy();"));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest, InvalidDetails) {
  NavigateTo("/show_promise/invalid_details.html");
  InstallEchoPaymentHandler();
  EXPECT_EQ(
      "TypeError: Failed to construct 'PaymentDetailsUpdate': Total amount "
      "value should be non-negative",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("buy($1)", payment_method_)));
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest,
                       ResolveWithEmptyDictionary) {
  NavigateTo("/show_promise/resolve_with_empty_dictionary.html");
  InstallEchoPaymentHandler();
  AddAutofillProfile(autofill::test::GetFullProfile());
  ShowBrowserPaymentSheet();

  EXPECT_TRUE(IsPayButtonEnabled());

  OpenOrderSummaryScreen();

  ExpectTotal("$3.00");
  EXPECT_EQ(u"$1.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_1));
  EXPECT_EQ(u"$1.00", GetLabelText(DialogViewID::ORDER_SUMMARY_LINE_ITEM_2));

  ClickOnBackArrow();
  OpenShippingAddressSectionScreen();

  ExpectNoShippingWarningMessage();

  ClickOnBackArrow();
  Pay();

  ExpectBodyContains({R"("details":{"currency":"USD","value":"3.00"})",
                      R"("shippingOption":"shipping-option-identifier")"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestShowPromiseTest,
                       ResolveWithEmptyListsOfItems) {
  NavigateTo("/show_promise/resolve_with_empty_lists.html");
  InstallEchoPaymentHandler();
  AddAutofillProfile(autofill::test::GetFullProfile());
  ShowBrowserPaymentSheet();

  EXPECT_FALSE(IsPayButtonEnabled());

  OpenOrderSummaryScreen();

  ExpectTotal("$1.00");
  ExpectNoDisplayItems();

  ClickOnBackArrow();
  OpenShippingAddressSectionScreen();

  ExpectShippingWarningMessage(
      "To see shipping methods and requirements, select an address");
}

}  // namespace
}  // namespace payments
