// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class SaveCardBubbleControllerImplTest
    : public DialogBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  SaveCardBubbleControllerImplTest()
      : base::test::WithFeatureOverride(
            features::kAutofillShowBubblesBasedOnPriorities) {}
  SaveCardBubbleControllerImplTest(const SaveCardBubbleControllerImplTest&) =
      delete;
  SaveCardBubbleControllerImplTest& operator=(
      const SaveCardBubbleControllerImplTest&) = delete;

  LegalMessageLines GetTestLegalMessage() {
    std::optional<base::Value> value(base::JSONReader::Read(
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"The legal documents are: {0} and {1}.\","
        "     \"template_parameter\" : [ {"
        "        \"display_text\" : \"Terms of Service\","
        "        \"url\": \"http://www.example.com/tos\""
        "     }, {"
        "        \"display_text\" : \"Privacy Policy\","
        "        \"url\": \"http://www.example.com/pp\""
        "     } ]"
        "  } ]"
        "}",
        base::JSON_PARSE_CHROMIUM_EXTENSIONS));
    EXPECT_TRUE(value->is_dict());
    LegalMessageLines legal_message_lines;
    LegalMessageLine::Parse(value->GetDict(), &legal_message_lines,
                            /*escape_apostrophes=*/true);
    return legal_message_lines;
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Ensure that the browser window is active.
    CHECK(ui_test_utils::BringBrowserWindowToFront(browser()));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of SaveCardBubbleControllerImpl. Alternative:
    // invoke via ChromeAutofillClient.
    SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
    controller_ = SaveCardBubbleControllerImpl::FromWebContents(web_contents);
    CHECK(controller_);

    payments::PaymentsAutofillClient::SaveCreditCardOptions options =
        payments::PaymentsAutofillClient::SaveCreditCardOptions()
            .with_should_request_name_from_user(
                name.find("WithCardholderNameTextfield") != std::string::npos)
            .with_should_request_expiration_date_from_user(
                name.find("WithCardExpirationDateDropDownBox") !=
                std::string::npos)
            .with_card_save_type(name.find("CvcSave") != std::string::npos
                                     ? payments::PaymentsAutofillClient::
                                           CardSaveType::kCvcSaveOnly
                                     : payments::PaymentsAutofillClient::
                                           CardSaveType::kCardSaveOnly)
            .with_show_prompt(true);

    PaymentsBubbleType bubble_type = PaymentsBubbleType::kInactive;
    if (name.find("LocalSave") != std::string::npos) {
      bubble_type = PaymentsBubbleType::kLocalSave;
    }
    if (name.find("LocalCvcSave") != std::string::npos) {
      bubble_type = PaymentsBubbleType::kLocalCvcSave;
    }
    if (name.find("ServerSave") != std::string::npos) {
      bubble_type = PaymentsBubbleType::kUploadSave;
    }
    if (name.find("ServerCvcSave") != std::string::npos) {
      bubble_type = PaymentsBubbleType::kUploadCvcSave;
    }
    if (name.find("Manage") != std::string::npos) {
      bubble_type = PaymentsBubbleType::kManageCards;
    }

    switch (bubble_type) {
      case PaymentsBubbleType::kLocalSave:
        controller_->OfferLocalSave(test::GetCreditCard(), options,
                                    base::DoNothing());
        break;
      case PaymentsBubbleType::kLocalCvcSave:
        controller_->OfferLocalSave(test::GetCreditCard(), options,
                                    base::DoNothing());
        break;
      case PaymentsBubbleType::kUploadSave:
        controller_->OfferUploadSave(test::GetMaskedServerCard(),
                                     GetTestLegalMessage(), options,
                                     base::DoNothing());
        break;
      case PaymentsBubbleType::kUploadCvcSave:
        controller_->OfferUploadSave(test::GetMaskedServerCard(),
                                     GetTestLegalMessage(), options,
                                     base::DoNothing());
        break;
      case PaymentsBubbleType::kManageCards:
        controller_->ShowBubbleForManageCardsForTesting(test::GetCreditCard());
        break;
      case PaymentsBubbleType::kUploadInProgress:
      case PaymentsBubbleType::kUploadComplete:
      case PaymentsBubbleType::kInactive:
        break;
    }
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  SaveCardBubbleControllerImpl* controller() { return controller_; }

 private:
  raw_ptr<SaveCardBubbleControllerImpl> controller_ = nullptr;
};

// Invokes a bubble asking the user if they want to save a credit card locally.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest, InvokeUi_LocalSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save the CVC for a credit
// card locally.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest,
                       InvokeUi_LocalCvcSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest, InvokeUi_ServerSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save the CVC for a credit
// card to Google Payments.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest,
                       InvokeUi_ServerCvcSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with an added textfield for entering/confirming cardholder name.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest,
                       InvokeUi_ServerSave_WithCardholderNameTextfield) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with a pair of dropdowns for entering expiration date.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest,
                       InvokeUi_ServerSave_WithCardExpirationDateDropDownBox) {
  ShowAndVerifyUi();
}

// Invokes a sign-in promo bubble.
// TODO(crbug.com/40581833): This browsertest isn't emulating the environment
//   quite correctly; disabling test for now until cause is found.
/*
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_Promo) {
  ShowAndVerifyUi();
}
*/

// Invokes a bubble displaying the card just saved and an option to
// manage cards.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest, InvokeUi_Manage) {
  ShowAndVerifyUi();
}

// Tests that opening a new tab will hide the save card bubble.
IN_PROC_BROWSER_TEST_P(SaveCardBubbleControllerImplTest, NewTabHidesDialog) {
  ShowUi("LocalSave");
  AutofillBubbleBase* bubble_base = controller()->GetPaymentBubbleView();
  ASSERT_NE(nullptr, bubble_base);

  SaveCardBubbleViews* bubble_view =
      static_cast<SaveCardBubbleViews*>(bubble_base);

  // Create a waiter that will return once the bubble's widget is destroyed.
  views::test::WidgetDestroyedWaiter waiter(bubble_view->GetWidget());

  // Open a new tab page in the foreground.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Wait until the bubble is actually gone.
  waiter.Wait();

  EXPECT_EQ(nullptr, controller()->GetPaymentBubbleView());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SaveCardBubbleControllerImplTest);

}  // namespace autofill
