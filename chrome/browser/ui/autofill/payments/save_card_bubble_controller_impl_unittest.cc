// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_view.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {

class TestSaveCardBubbleControllerImpl : public SaveCardBubbleControllerImpl {
 public:
  static void CreateForTesting(content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<TestSaveCardBubbleControllerImpl>(web_contents));
  }

  // Overriding because parent function requires a browser window to redirect
  // properly, which is not available in unit tests.
  void ShowPaymentsSettingsPage() override {}

  explicit TestSaveCardBubbleControllerImpl(content::WebContents* web_contents)
      : SaveCardBubbleControllerImpl(web_contents) {}

  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }

  void SimulateNavigation() {
    content::MockNavigationHandle handle;
    handle.set_has_committed(true);
    DidFinishNavigation(&handle);
  }

 protected:
  security_state::SecurityLevel GetSecurityLevel() const override {
    return security_level_;
  }

  AutofillSyncSigninState GetSyncState() const override {
    return AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled;
  }

 private:
  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;
};

class SaveCardBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  SaveCardBubbleControllerImplTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestSaveCardBubbleControllerImpl::CreateForTesting(web_contents);
    user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())
        ->SetInteger(
            prefs::kAutofillAcceptSaveCreditCardPromptState,
            prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  }

  void SetLegalMessage(
      const std::string& message_json,
      AutofillClient::SaveCreditCardOptions options =
          AutofillClient::SaveCreditCardOptions().with_show_prompt()) {
    std::unique_ptr<base::Value> value(
        base::JSONReader::ReadDeprecated(message_json));
    ASSERT_TRUE(value);
    base::DictionaryValue* dictionary;
    ASSERT_TRUE(value->GetAsDictionary(&dictionary));
    LegalMessageLines legal_message_lines;
    LegalMessageLine::Parse(*dictionary, &legal_message_lines,
                            /*escape_apostrophes=*/true);
    controller()->OfferUploadSave(CreditCard(), legal_message_lines, options,
                                  base::BindOnce(&UploadSaveCardCallback));
  }

  void ShowLocalBubble(
      CreditCard* card = nullptr,
      AutofillClient::SaveCreditCardOptions options =
          AutofillClient::SaveCreditCardOptions().with_show_prompt()) {
    controller()->OfferLocalSave(
        card ? CreditCard(*card)
             : autofill::test::GetCreditCard(),  // Visa by default
        options, base::BindOnce(&LocalSaveCardCallback));
  }

  void ShowUploadBubble(
      AutofillClient::SaveCreditCardOptions options =
          AutofillClient::SaveCreditCardOptions().with_show_prompt()) {
    SetLegalMessage(
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"This is the entire message.\""
        "  } ]"
        "}",
        options);
  }

  void CloseAndReshowBubble() {
    controller()->OnBubbleClosed();
    controller()->ReshowBubble();
  }

  void ClickSaveButton() {
    controller()->OnSaveButton({});
    if (controller()->ShouldShowCardSavedLabelAnimation())
      controller()->OnAnimationEnded();
  }

 protected:
  TestSaveCardBubbleControllerImpl* controller() {
    return static_cast<TestSaveCardBubbleControllerImpl*>(
        TestSaveCardBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

  TestAutofillClock test_clock_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  static void UploadSaveCardCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      const AutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {}
  static void LocalSaveCardCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision) {}

  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImplTest);
};

// Tests that the legal message lines vector is empty when doing a local save so
// that no legal messages will be shown to the user in that case.
TEST_F(SaveCardBubbleControllerImplTest, LegalMessageLinesEmptyOnLocalSave) {
  ShowUploadBubble();
  controller()->OnBubbleClosed();
  ShowLocalBubble();
  EXPECT_TRUE(controller()->GetLegalMessageLines().empty());
}

TEST_F(SaveCardBubbleControllerImplTest,
       PropagateShouldRequestNameFromUserWhenFalse) {
  ShowUploadBubble();
  EXPECT_FALSE(controller()->ShouldRequestNameFromUser());
}

TEST_F(SaveCardBubbleControllerImplTest,
       PropagateShouldRequestNameFromUserWhenTrue) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  EXPECT_TRUE(controller()->ShouldRequestNameFromUser());
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_FirstShow_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_Reshows_ShowBubble) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.Reshows"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_FromDynamicChangeForm_ShowBubble) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_from_dynamic_change_form(true)
                                        .with_show_prompt());

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.Reshows.FromDynamicChangeForm"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_FromNonFocusableForm_ShowBubble) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_has_non_focusable_field(true)
                                        .with_show_prompt());

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.Reshows.FromNonFocusableForm"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_FirstShow_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_DynamicChange_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "FirstShow.FromDynamicChangeForm"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromNonFocusableForm_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "FirstShow.FromNonFocusableForm"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingCardholderName_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "FirstShow.RequestingCardholderName"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingExpirationDate_ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "FirstShow.RequestingExpirationDate"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_Reshows_ShowBubble) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.Reshows"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_FromNonFocusableForm_ShowBubble) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "Reshows.FromNonFocusableForm"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_FromDynamicChangeForm_ShowBubble) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "Reshows.FromDynamicChangeForm"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingCardholderName_ShowBubble) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "Reshows.RequestingCardholderName"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingExpirationDate_ShowBubble) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());
  base::HistogramTester histogram_tester;
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "Reshows.RequestingExpirationDate"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_ShowBubbleFalse) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_ShowBubbleFalse) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions());

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_FirstShow_SaveButton) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_FromDynamicChangeForm_SaveButton) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_from_dynamic_change_form(true)
                                        .with_show_prompt());
  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_FromNonFocusableForm_SaveButton) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_has_non_focusable_field(true)
                                        .with_show_prompt());
  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_Reshows_SaveButton) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingCardholderName_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingExpirationDate_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_FromNonFocusableForm_SaveButton) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_has_non_focusable_field(true)
                                        .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromNonFocusableForm_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_FromDynamicChangeForm_SaveButton) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_from_dynamic_change_form(true)
                                        .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromDynamicChangeForm_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingCardholderName_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingExpirationDate_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_FromDynamicChangeForm_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());

  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_FromNonFocusableForm_SaveButton) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());

  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  ClickSaveButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_FirstShow_CancelButton) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Local_Reshows_CancelButton) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_CancelButton_FirstShow) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  ShowLocalBubble();
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 2),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 2),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow.PreviouslyDenied"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Local."
                                     "FirstShow.NoPreviousDecision"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_CancelButton_FirstShow_SaveButton_FirstShow) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  ShowLocalBubble();
  ClickSaveButton();
  controller()->OnBubbleClosed();

  ShowLocalBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 3),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 3),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Local."
                                     "FirstShow.NoPreviousDecision"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow.PreviouslyDenied"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow.PreviouslyAccepted"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_CancelButton_FirstShow_SaveButton_FirstShow) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();
  controller()->OnCancelButton();
  controller()->OnBubbleClosed();

  ShowUploadBubble();
  ClickSaveButton();
  controller()->OnBubbleClosed();

  ShowUploadBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 3),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 3),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Upload."
                                     "FirstShow.NoPreviousDecision"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow.PreviouslyDenied"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow.PreviouslyAccepted"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_CancelButton_Reshows) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Local."
                                     "Reshows.NoPreviousDecision"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_Reshows_Reshows) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  CloseAndReshowBubble();
  CloseAndReshowBubble();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.Reshows"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 2),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 2)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt.Local."
                                     "Reshows.NoPreviousDecision"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 2),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 2)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_NavigateWhileShowing) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_FromDynamicChangeForm_NavigateWhileShowing) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_from_dynamic_change_form(true)
                                        .with_show_prompt());

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_FromNonFocusableForm_NavigateWhileShowing) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_has_non_focusable_field(true)
                                        .with_show_prompt());

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_NavigateWhileShowing) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Local.Reshows", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_NavigateWhileShowing) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromNonFocusableForm_NavigateWhileShowing) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());
  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromNonFocusableForm", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromDynamicChangeForm_NavigateWhileShowing) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());
  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromDynamicChangeForm",
      0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingCardholderName_NavigateWhileShowing) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingExpirationDate_NavigateWhileShowing) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingExpirationDate",
      0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_NavigateWhileShowing) {
  ShowUploadBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows", 0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingCardholderName_NavigateWhileShowing) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingCardholderName",
      0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingExpirationDate_NavigateWhileShowing) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  // The bubble should still stick around for up to
  // kCardBubbleSurviveNavigationTime (5 seconds) regardless of navigation.
  // Start by waiting for 3 seconds, simulating navigation, and ensuring the
  // bubble has not changed.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingExpirationDate",
      0);

  // Wait 3 more seconds (6 total); bubble should go away on next navigation.
  test_clock_.Advance(base::TimeDelta::FromSeconds(3));

  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_NavigateWhileHidden) {
  ShowLocalBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_Reshows_NavigateWhileHidden) {
  ShowLocalBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_NavigateWhileHidden) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_FromNonFocusableForm_NavigateWhileHidden) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_has_non_focusable_field(true)
                                        .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_FromDynamicChangeForm_NavigateWhileHidden) {
  ShowLocalBubble(/*card=*/nullptr, AutofillClient::SaveCreditCardOptions()
                                        .with_from_dynamic_change_form(true)
                                        .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromDynamicChangeForm_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_FromNonFocusableForm_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingCardholderName_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingExpirationDate_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_NavigateWhileHidden) {
  ShowUploadBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_FromDynamicChangeForm_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_from_dynamic_change_form(true)
                       .with_show_prompt());

  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.FromDynamicChangeForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_FromNonFocusableForm_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_has_non_focusable_field(true)
                       .with_show_prompt());

  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.FromNonFocusableForm",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingCardholderName_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingExpirationDate_NavigateWhileHidden) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnBubbleClosed();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_LegalMessageLink) {
  ShowUploadBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingCardholderName_LegalMessageLink) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_RequestingExpirationDate_LegalMessageLink) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.FirstShow.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_LegalMessageLink) {
  ShowUploadBubble();
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingCardholderName_LegalMessageLink) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingCardholderName",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_Reshows_RequestingExpirationDate_LegalMessageLink) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());
  CloseAndReshowBubble();

  base::HistogramTester histogram_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, OnlyOneActiveBubble_RepeatedLocal) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ShowLocalBubble();
  ShowLocalBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, OnlyOneActiveBubble_RepeatedUpload) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();
  ShowUploadBubble();
  ShowUploadBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest, OnlyOneActiveBubble_LocalThenUpload) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ShowUploadBubble();
  ShowUploadBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples("Autofill.SaveCreditCardPrompt.Upload.FirstShow")
          .empty());
}

TEST_F(SaveCardBubbleControllerImplTest, OnlyOneActiveBubble_UploadThenLocal) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();
  ShowLocalBubble();
  ShowLocalBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.FirstShow"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples("Autofill.SaveCreditCardPrompt.Local.FirstShow")
          .empty());
}

TEST_F(SaveCardBubbleControllerImplTest,
       LogSaveCardPromptMetricBySecurityLevel_Local) {
  base::HistogramTester histogram_tester;
  controller()->set_security_level(security_state::SecurityLevel::SECURE);
  ShowLocalBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Local.SECURE"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
  EXPECT_TRUE(histogram_tester
                  .GetAllSamples("Autofill.SaveCreditCardPrompt.Upload.SECURE")
                  .empty());
}

TEST_F(SaveCardBubbleControllerImplTest,
       LogSaveCardPromptMetricBySecurityLevel_Upload) {
  base::HistogramTester histogram_tester;
  controller()->set_security_level(security_state::SecurityLevel::EV_SECURE);
  ShowUploadBubble();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.SaveCreditCardPrompt.Upload.EV_SECURE"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1)));
  EXPECT_TRUE(
      histogram_tester
          .GetAllSamples("Autofill.SaveCreditCardPrompt.Local.EV_SECURE")
          .empty());
}

// Tests for Sign-In after Local Save.

TEST_F(SaveCardBubbleControllerImplTest,
       Local_FirstShow_SaveButton_SigninPromo) {
  ShowLocalBubble();
  ClickSaveButton();

  // Sign-in promo should be shown after accepting local save.
  EXPECT_EQ(BubbleType::SIGN_IN_PROMO, controller()->GetBubbleType());
  EXPECT_NE(nullptr, controller()->GetSaveCardBubbleView());
}

// Tests for Manage Cards.

TEST_F(SaveCardBubbleControllerImplTest,
       Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();

  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_EQ(BubbleType::MANAGE_CARDS, controller()->GetBubbleType());
  EXPECT_NE(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(
    SaveCardBubbleControllerImplTest,
    Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});
  CloseAndReshowBubble();

  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1)));
}

TEST_F(
    SaveCardBubbleControllerImplTest,
    Metrics_Local_FirstShow_SaveButton_Close_Reshow_Close_Reshow_ManageCards) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});
  CloseAndReshowBubble();
  CloseAndReshowBubble();

  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 2)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_Close_Navigate) {
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  controller()->OnBubbleClosed();

  test_clock_.Advance(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();

  // Icon should disappear after navigating away.
  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_Navigate) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});
  CloseAndReshowBubble();

  test_clock_.Advance(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1)));
}

TEST_F(
    SaveCardBubbleControllerImplTest,
    Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_Close_Navigate) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});
  CloseAndReshowBubble();
  controller()->OnBubbleClosed();

  test_clock_.Advance(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_ClickManageCardsDoneButton) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});
  CloseAndReshowBubble();
  controller()->OnSaveButton({});

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_DONE, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_ClickManageCardsManageCardsButton) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});
  CloseAndReshowBubble();
  controller()->OnManageCardsClicked();

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Upload_FirstShow_SaveButton_NoSigninPromo) {
  ShowUploadBubble();
  ClickSaveButton();

  // Icon should disappear after an upload save,
  // even when this flag is enabled.
  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_SaveButton_NoSigninPromo) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});

  // No other bubbles should have popped up.
  histogram_tester.ExpectTotalCount("Autofill.SignInPromo", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Local", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Upload", 0);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_FirstShow_ManageCards) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});
  controller()->ShowBubbleForManageCardsForTesting(
      autofill::test::GetCreditCard());

  // Icon should disappear after an upload save,
  // even when this flag is enabled.
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Local", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Upload", 1);
}

TEST_F(SaveCardBubbleControllerImplTest,
       PropagateShouldRequestExpirationDateFromUserWhenFalse) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  EXPECT_FALSE(controller()->ShouldRequestExpirationDateFromUser());
}

TEST_F(SaveCardBubbleControllerImplTest,
       PropagateShouldRequestExpirationDateFromUserWhenTrue) {
  ShowUploadBubble(AutofillClient::SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  EXPECT_TRUE(controller()->ShouldRequestExpirationDateFromUser());
}

}  // namespace autofill
