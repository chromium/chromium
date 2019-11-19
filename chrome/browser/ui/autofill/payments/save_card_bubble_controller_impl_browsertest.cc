// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveCardBubbleControllerImplTest : public DialogBrowserTest {
 public:
  SaveCardBubbleControllerImplTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DialogBrowserTest::SetUpCommandLine(command_line);
  }

  LegalMessageLines GetTestLegalMessage() {
    std::unique_ptr<base::Value> value(base::JSONReader::ReadDeprecated(
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
    base::DictionaryValue* dictionary;
    value->GetAsDictionary(&dictionary);
    LegalMessageLines legal_message_lines;
    LegalMessageLine::Parse(*dictionary, &legal_message_lines,
                            /*escape_apostrophes=*/true);
    return legal_message_lines;
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of SaveCardBubbleControllerImpl. Alternative:
    // invoke via ChromeAutofillClient.
    SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
    controller_ = SaveCardBubbleControllerImpl::FromWebContents(web_contents);
    DCHECK(controller_);

    AutofillClient::SaveCreditCardOptions options =
        AutofillClient::SaveCreditCardOptions()
            .with_should_request_name_from_user(
                name.find("WithCardholderNameTextfield") != std::string::npos)
            .with_should_request_expiration_date_from_user(
                name.find("WithCardExpirationDateDropDownBox") !=
                std::string::npos)
            .with_show_prompt(true);

    BubbleType bubble_type = BubbleType::INACTIVE;
    if (name.find("Local") != std::string::npos)
      bubble_type = BubbleType::LOCAL_SAVE;
    if (name.find("Server") != std::string::npos)
      bubble_type = BubbleType::UPLOAD_SAVE;
    if (name.find("Promo") != std::string::npos)
      bubble_type = BubbleType::SIGN_IN_PROMO;
    if (name.find("Manage") != std::string::npos)
      bubble_type = BubbleType::MANAGE_CARDS;
    if (name.find("Failure") != std::string::npos)
      bubble_type = BubbleType::FAILURE;

    switch (bubble_type) {
      case BubbleType::LOCAL_SAVE:
        controller_->OfferLocalSave(
            test::GetCreditCard(),
            AutofillClient::SaveCreditCardOptions().with_show_prompt(true),
            base::DoNothing());
        break;
      case BubbleType::UPLOAD_SAVE:
        controller_->OfferUploadSave(test::GetMaskedServerCard(),
                                     GetTestLegalMessage(), options,
                                     base::DoNothing());
        break;
      case BubbleType::SIGN_IN_PROMO:
        controller_->MaybeShowBubbleForSignInPromo();
        break;
      case BubbleType::MANAGE_CARDS:
        controller_->ShowBubbleForManageCardsForTesting(test::GetCreditCard());
        break;
      case BubbleType::FAILURE:
        controller_->ShowBubbleForSaveCardFailureForTesting();
        break;
      case BubbleType::UPLOAD_IN_PROGRESS:
      case BubbleType::INACTIVE:
        break;
    }
  }

  SaveCardBubbleControllerImpl* controller() { return controller_; }

 private:
  SaveCardBubbleControllerImpl* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImplTest);
};

// Invokes a bubble asking the user if they want to save a credit card locally.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_Local) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_Server) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with an added textfield for entering/confirming cardholder name.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeUi_Server_WithCardholderNameTextfield) {
  ShowAndVerifyUi();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with a pair of dropdowns for entering expiration date.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeUi_Server_WithCardExpirationDateDropDownBox) {
  ShowAndVerifyUi();
}

// Invokes a sign-in promo bubble.
// TODO(crbug.com/855186): This browsertest isn't emulating the environment
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

// Invokes a bubble displaying the card saving just failed.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeUi_Failure) {
  ShowAndVerifyUi();
}

// Tests that opening a new tab will hide the save card bubble.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, NewTabHidesDialog) {
  ShowUi("Local");
  EXPECT_NE(nullptr, controller()->GetSaveCardBubbleView());
  // Open a new tab page in the foreground.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(nullptr, controller()->GetSaveCardBubbleView());
}

}  // namespace autofill
