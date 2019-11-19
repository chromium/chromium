// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#endif

namespace payments {

using ::testing::UnorderedElementsAre;

class PaymentRequestWebContentsManagerTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestWebContentsManagerTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestWebContentsManagerTest);
};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestWebContentsManagerTest, MultipleRequests) {
  NavigateTo("/payment_request_multiple_requests.html");
  const std::vector<PaymentRequest*> payment_requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(5U, payment_requests.size());
}

class PaymentRequestNoShippingTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestNoShippingTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestNoShippingTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, InactiveBrowserWindow) {
  NavigateTo("/payment_request_no_shipping_test.html");
  SetBrowserWindowInactive();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  EXPECT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));

  WaitForObservedEvent();

  ExpectBodyContains({"AbortError"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateTo404) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  NavigateTo("/non-existent.html");

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateToSame) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  NavigateTo("/payment_request_no_shipping_test.html");

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndReload) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndClickCancel) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest,
                       OrderSummaryAndClickCancel) {
  NavigateTo("/payment_request_no_shipping_test.html");
  InvokePaymentRequestUI();

  OpenOrderSummaryScreen();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, PayWithVisa) {
  NavigateTo("/payment_request_no_shipping_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  InvokePaymentRequestUI();

  ResetEventWaiter(DialogEvent::DIALOG_CLOSED);

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  WaitForObservedEvent();

  // The actual structure of the card response is unit-tested.
  ExpectBodyContains({"4111111111111111", "Test User", "11", "2022"});
  ExpectBodyContains({"John", "H.", "Doe", "Underworld", "666 Erebus St.",
                      "Apt 8", "Elysium", "CA", "91111", "US", "16502111111"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, InvalidSSL) {
  NavigateTo("/payment_request_no_shipping_test.html");
  SetInvalidSsl();

  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  ResetEventWaiter(DialogEvent::NOT_SUPPORTED_ERROR);

  EXPECT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));

  WaitForObservedEvent();

  ExpectBodyContains({"NotSupportedError"});
}

class PaymentRequestAbortTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestAbortTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestAbortTest);
};

// Testing the use of the abort() JS API.
IN_PROC_BROWSER_TEST_F(PaymentRequestAbortTest, OpenThenAbort) {
  NavigateTo("/payment_request_abort_test.html");
  InvokePaymentRequestUI();

  ResetEventWaiterForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  ExpectBodyContains({"Aborted"});

  // The web-modal dialog should now be closed.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestAbortTest,
                       AbortUnsuccessfulAfterCVCPromptShown) {
  NavigateTo("/payment_request_abort_test.html");
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  InvokePaymentRequestUI();
  OpenCVCPromptWithCVC(base::UTF8ToUTF16("123"));

  ResetEventWaiter(DialogEvent::ABORT_CALLED);

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  ExpectBodyContains({"Cannot abort"});
}

class PaymentRequestPaymentMethodIdentifierTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestPaymentMethodIdentifierTest() {}

  void InvokePaymentRequestWithJs(const std::string& js) {
    ResetEventWaiterForDialogOpened();

    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), js));

    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestPaymentMethodIdentifierTest);
};

// One network is specified in 'basic-card' data, one in supportedMethods.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentMethodIdentifierTest,
                       BasicCard_NetworksSpecified) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  InvokePaymentRequestWithJs("buy();");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  EXPECT_EQ(2u, supported_card_networks.size());
  // The networks appear in the order in which they were specified by the
  // merchant.
  EXPECT_EQ("mastercard", supported_card_networks[0]);
  EXPECT_EQ("visa", supported_card_networks[1]);
}

// Only specifying 'basic-card' with no supportedNetworks means all networks are
// supported.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentMethodIdentifierTest,
                       BasicCard_NoNetworksSpecified) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  InvokePaymentRequestWithJs("buyHelper([basicCardMethod]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  // The default ordering is alphabetical.
  EXPECT_EQ(8u, supported_card_networks.size());
  EXPECT_EQ("amex", supported_card_networks[0]);
  EXPECT_EQ("diners", supported_card_networks[1]);
  EXPECT_EQ("discover", supported_card_networks[2]);
  EXPECT_EQ("jcb", supported_card_networks[3]);
  EXPECT_EQ("mastercard", supported_card_networks[4]);
  EXPECT_EQ("mir", supported_card_networks[5]);
  EXPECT_EQ("unionpay", supported_card_networks[6]);
  EXPECT_EQ("visa", supported_card_networks[7]);
}

// Specifying 'basic-card' with some networks after having explicitely included
// the same networks does not yield duplicates and has the expected order.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentMethodIdentifierTest,
                       RepeatedBasicCardWithSameNetworkAreDeduped) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  InvokePaymentRequestWithJs(
      "buyHelper([{"
      "  supportedMethods: 'basic-card',"
      "  data: {"
      "    supportedNetworks: ['mastercard'],"
      "  }"
      "}, {"
      "  supportedMethods: 'basic-card',"
      "  data: {"
      "    supportedNetworks: ['visa', 'mastercard', 'jcb'],"
      "  }"
      "}]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  EXPECT_EQ(3u, supported_card_networks.size());
  EXPECT_EQ("mastercard", supported_card_networks[0]);
  EXPECT_EQ("visa", supported_card_networks[1]);
  EXPECT_EQ("jcb", supported_card_networks[2]);
}

// A url-based payment method identifier is only supported if it has an https
// scheme.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentMethodIdentifierTest, Url_Valid) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  InvokePaymentRequestWithJs(
      "buyHelper([{"
      "  supportedMethods: 'https://bobpay.xyz'"
      "}, {"
      "  supportedMethods: 'basic-card'"
      "}]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<GURL> url_payment_method_identifiers =
      requests[0]->spec()->url_payment_method_identifiers();
  EXPECT_EQ(1u, url_payment_method_identifiers.size());
  EXPECT_EQ(GURL("https://bobpay.xyz"), url_payment_method_identifiers[0]);
}

// Specifiying multiple different types of payment method identifiers still
// yields the correct supported methods in payment request.
IN_PROC_BROWSER_TEST_F(PaymentRequestPaymentMethodIdentifierTest,
                       MultiplePaymentMethodIdentifiers) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  InvokePaymentRequestWithJs(
      "buyHelper([{"
      "  supportedMethods: 'https://bobpay.xyz'"
      "}, {"
      "  supportedMethods: 'https://bobpay.xyz'"
      "}, {"
      "  supportedMethods: 'https://alicepay.com'"
      "}, {"
      "  supportedMethods: 'basic-card',"
      "  data: {"
      "    supportedNetworks: ['visa', 'mastercard', 'jcb'],"
      "  }"
      "}]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());

  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  EXPECT_THAT(supported_card_networks,
              UnorderedElementsAre("mastercard", "visa", "jcb"));

  std::vector<GURL> url_payment_method_identifiers =
      requests[0]->spec()->url_payment_method_identifiers();
  EXPECT_EQ(2u, url_payment_method_identifiers.size());
  EXPECT_EQ(GURL("https://bobpay.xyz"), url_payment_method_identifiers[0]);
  EXPECT_EQ(GURL("https://alicepay.com"), url_payment_method_identifiers[1]);
}

// Test harness integrating with DialogBrowserTest to present the dialog in an
// interactive manner for visual testing.
class PaymentsRequestVisualTest
    : public SupportsTestDialog<PaymentRequestNoShippingTest> {
 protected:
  PaymentsRequestVisualTest() {}

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override { InvokePaymentRequestUI(); }

  bool AlwaysCloseAsynchronously() override {
    // Bypassing Widget::CanClose() causes payments::JourneyLogger to see the
    // show, but not the close, resulting in a DCHECK in its destructor.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsRequestVisualTest);
};

IN_PROC_BROWSER_TEST_F(PaymentsRequestVisualTest, InvokeUi_NoShipping) {
  NavigateTo("/payment_request_no_shipping_test.html");
  ShowAndVerifyUi();
}

class PaymentRequestSettingsLinkTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestSettingsLinkTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSettingsLinkTest);
};

// Tests that clicking the settings link brings the user to settings.
IN_PROC_BROWSER_TEST_F(PaymentRequestSettingsLinkTest, ClickSettingsLink) {
#if defined(OS_CHROMEOS)
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
#endif

  NavigateTo("/payment_request_no_shipping_test.html");
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Click on the settings link in the payment request dialog window.
  InvokePaymentRequestUI();
  views::StyledLabel* styled_label =
      static_cast<views::StyledLabel*>(dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::DATA_SOURCE_LABEL)));
  EXPECT_TRUE(styled_label);
  // The Link is the only child of the StyledLabel.
  content::WebContentsAddedObserver web_contents_added_observer;
  styled_label->LinkClicked(nullptr, 0);
  content::WebContents* new_tab_contents =
      web_contents_added_observer.GetWebContents();

  EXPECT_EQ(
      std::string(chrome::kChromeUISettingsURL) + chrome::kPaymentsSubPage,
      new_tab_contents->GetVisibleURL().spec());
}

}  // namespace payments
