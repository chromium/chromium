// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveCardBubbleControllerImplTest : public DialogBrowserTest {
 public:
  SaveCardBubbleControllerImplTest() = default;
  SaveCardBubbleControllerImplTest(const SaveCardBubbleControllerImplTest&) =
      delete;
  SaveCardBubbleControllerImplTest& operator=(
      const SaveCardBubbleControllerImplTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DialogBrowserTest::SetUpCommandLine(command_line);
  }

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
        "}"));
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

    BubbleType bubble_type = BubbleType::INACTIVE;
    if (name.find("LocalSave") != std::string::npos) {
      bubble_type = BubbleType::LOCAL_SAVE;
    }
    if (name.find("LocalCvcSave") != std::string::npos) {
      bubble_type = BubbleType::LOCAL_CVC_SAVE;
    }
    if (name.find("ServerSave") != std::string::npos) {
      bubble_type = BubbleType::UPLOAD_SAVE;
    }
    if (name.find("ServerCvcSave") != std::string::npos) {
      bubble_type = BubbleType::UPLOAD_CVC_SAVE;
    }
    if (name.find("Manage") != std::string::npos) {
      bubble_type = BubbleType::MANAGE_CARDS;
    }

    switch (bubble_type) {
      case BubbleType::LOCAL_SAVE:
        controller_->OfferLocalSave(test::GetCreditCard(), options,
                                    base::DoNothing());
        break;
      case BubbleType::LOCAL_CVC_SAVE:
        controller_->OfferLocalSave(test::GetCreditCard(), options,
                                    base::DoNothing());
        break;
      case BubbleType::UPLOAD_SAVE:
        controller_->OfferUploadSave(test::GetMaskedServerCard(),
                                     GetTestLegalMessage(), options,
                                     base::DoNothing());
        break;
      case BubbleType::UPLOAD_CVC_SAVE:
        controller_->OfferUploadSave(test::GetMaskedServerCard(),
                                     GetTestLegalMessage(), options,
                                     base::DoNothing());
        break;
      case BubbleType::MANAGE_CARDS:
        controller_->ShowBubbleForManageCardsForTesting(test::GetCreditCard());
        break;
      case BubbleType::UPLOAD_IN_PROGRESS:
      case BubbleType::UPLOAD_COMPLETED:
      case BubbleType::INACTIVE:
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
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_LocalSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save the CVC for a credit
// card locally.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeUi_LocalCvcSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_ServerSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save the CVC for a credit
// card to Google Payments.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeUi_ServerCvcSave) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with an added textfield for entering/confirming cardholder name.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeUi_ServerSave_WithCardholderNameTextfield) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with a pair of dropdowns for entering expiration date.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
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
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_Manage) {
  ShowAndVerifyUi();
}

// Tests that opening a new tab will hide the save card bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, NewTabHidesDialog) {
  ShowUi("LocalSave");
  EXPECT_NE(nullptr, controller()->GetPaymentBubbleView());
  // Open a new tab page in the foreground.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(nullptr, controller()->GetPaymentBubbleView());
}

}  // namespace autofill
