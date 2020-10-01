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
#include "base/test/metrics/user_action_tester.h"
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

const base::Time kArbitraryTime = base::Time::FromTimeT(1234567890);

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
    test_clock_.SetNow(kArbitraryTime);
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

  void CloseBubble(PaymentsBubbleClosedReason closed_reason =
                       PaymentsBubbleClosedReason::kNotInteracted) {
    controller()->OnBubbleClosed(closed_reason);
  }

  void CloseAndReshowBubble() {
    CloseBubble();
    controller()->ReshowBubble();
  }

  void ClickSaveButton() {
    controller()->OnSaveButton({});
    controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kAccepted);
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
  CloseBubble();
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

// TODO(crbug.com/1070799): Delete these navigation tests once the new logging
// is launched.

// Ensures the bubble should still stick around even if the time since bubble
// showing is longer than kCardBubbleSurviveNavigationTime (5 seconds) when the
// feature is enabled.
TEST_F(SaveCardBubbleControllerImplTest,
       StickyBubble_ShouldNotDismissUponNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAutofillEnableStickyPaymentsBubble);

  ShowLocalBubble();
  base::HistogramTester histogram_tester;
  test_clock_.Advance(base::TimeDelta::FromSeconds(10));
  controller()->SimulateNavigation();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPrompt.Local.FirstShow", 0);
  EXPECT_NE(nullptr, controller()->GetSaveCardBubbleView());
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
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
  CloseBubble();
  // Fake-navigate after bubble has been visible for a long time.
  test_clock_.Advance(base::TimeDelta::FromMinutes(1));
  controller()->SimulateNavigation();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt.Upload.Reshows.RequestingExpirationDate",
      AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, 1);
}

// Param of the SaveCardBubbleSingletonTestData:
// -- bool metrics_revamp_experiment_enabled;
// -- bool first_shown_is_local;
// -- bool second_and_third_shown_are_local;
typedef std::tuple<bool, bool, bool> SaveCardBubbleSingletonTestData;

// One test case will be run several times till we cover all the param
// combinations of the |SaveCardBubbleSingletonTestData|. GetParam() will help
// get the specific param value for a particular run.
class SaveCardBubbleSingletonTest
    : public SaveCardBubbleControllerImplTest,
      public testing::WithParamInterface<SaveCardBubbleSingletonTestData> {
 public:
  SaveCardBubbleSingletonTest()
      : metrics_revamp_experiment_enabled_(std::get<0>(GetParam())),
        first_shown_is_local_(std::get<1>(GetParam())),
        second_and_third_shown_are_local_(std::get<2>(GetParam())) {
    if (metrics_revamp_experiment_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAutofillEnableFixedPaymentsBubbleLogging);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillEnableFixedPaymentsBubbleLogging);
    }
  }

  ~SaveCardBubbleSingletonTest() override = default;

  void ShowBubble(bool is_local) {
    is_local ? ShowLocalBubble() : ShowUploadBubble();
  }

  void TriggerFlow() {
    ShowBubble(first_shown_is_local_);
    ShowBubble(second_and_third_shown_are_local_);
    ShowBubble(second_and_third_shown_are_local_);
  }

  const bool metrics_revamp_experiment_enabled_;
  const bool first_shown_is_local_;
  const bool second_and_third_shown_are_local_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SaveCardBubbleSingletonTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(SaveCardBubbleSingletonTest, OnlyOneActiveBubble) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  std::string suffix =
      first_shown_is_local_ ? ".Local.FirstShow" : ".Upload.FirstShow";

  if (metrics_revamp_experiment_enabled_) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCreditCardPromptOffer" + suffix,
        AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.SaveCreditCardPromptOffer" + suffix, 0);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt" + suffix),
      ElementsAre(
          Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
          Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN_DEPRECATED, 1)));
}

// Note that even though in prod the four options in the SaveCreditCardOptions
// struct can be true at the same time, we don't support that in the test case
// (by the way we create histogram name suffixes).
struct SaveCardOptionParam {
  bool from_dynamic_change_form;
  bool has_non_focusable_field;
  bool should_request_name_from_user;
  bool should_request_expiration_date_from_user;
};

const SaveCardOptionParam kSaveCardOptionParam[] = {
    {false, false, false, false}, {true, false, false, false},
    {false, true, false, false},  {false, false, true, false},
    {false, false, false, true},
};

// Param of the SaveCardBubbleSingletonTestData:
// -- bool metrics_revamp_experiment_enabled
// -- std::string destination
// -- std::string show
// -- SaveCardOptionParam save_card_option_param
typedef std::tuple<bool, std::string, std::string, SaveCardOptionParam>
    SaveCardBubbleLoggingTestData;

// Test class to ensure the save card bubble events are logged correctly.
class SaveCardBubbleLoggingTest
    : public SaveCardBubbleControllerImplTest,
      public ::testing::WithParamInterface<SaveCardBubbleLoggingTestData> {
 public:
  SaveCardBubbleLoggingTest()
      : metrics_revamp_experiment_enabled_(std::get<0>(GetParam())),
        destination_(std::get<1>(GetParam())),
        show_(std::get<2>(GetParam())) {
    if (metrics_revamp_experiment_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAutofillEnableFixedPaymentsBubbleLogging);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAutofillEnableFixedPaymentsBubbleLogging);
    }

    SaveCardOptionParam save_card_option_param = std::get<3>(GetParam());
    save_credit_card_options_ =
        AutofillClient::SaveCreditCardOptions()
            .with_from_dynamic_change_form(
                save_card_option_param.from_dynamic_change_form)
            .with_has_non_focusable_field(
                save_card_option_param.has_non_focusable_field)
            .with_should_request_name_from_user(
                save_card_option_param.should_request_name_from_user)
            .with_should_request_expiration_date_from_user(
                save_card_option_param
                    .should_request_expiration_date_from_user);
  }

  ~SaveCardBubbleLoggingTest() override = default;

  void TriggerFlow(bool show_prompt = true) {
    if (destination_ == "Local") {
      if (show_ == "FirstShow") {
        ShowLocalBubble(/*card=*/nullptr,
                        /*options=*/GetSaveCreditCardOptions().with_show_prompt(
                            show_prompt));
      } else {
        DCHECK_EQ(show_, "Reshows");
        ShowLocalBubble(/*card=*/nullptr,
                        /*options=*/GetSaveCreditCardOptions().with_show_prompt(
                            show_prompt));
        CloseAndReshowBubble();
      }
    } else {
      DCHECK_EQ(destination_, "Upload");
      if (show_ == "FirstShow") {
        ShowUploadBubble(
            GetSaveCreditCardOptions().with_show_prompt(show_prompt));
      } else {
        DCHECK_EQ(show_, "Reshows");
        ShowUploadBubble(
            GetSaveCreditCardOptions().with_show_prompt(show_prompt));
        CloseAndReshowBubble();
      }
    }
  }

  AutofillClient::SaveCreditCardOptions GetSaveCreditCardOptions() {
    return save_credit_card_options_;
  }

  std::string GetHistogramNameSuffix() {
    std::string result = "." + destination_ + "." + show_;

    if (GetSaveCreditCardOptions().from_dynamic_change_form)
      result += ".FromDynamicChangeForm";

    if (GetSaveCreditCardOptions().has_non_focusable_field)
      result += ".FromNonFocusableForm";

    if (GetSaveCreditCardOptions().should_request_name_from_user)
      result += ".RequestingCardholderName";

    if (GetSaveCreditCardOptions().should_request_expiration_date_from_user)
      result += ".RequestingExpirationDate";

    return result;
  }

  const bool metrics_revamp_experiment_enabled_;
  const std::string destination_;
  const std::string show_;

 private:
  AutofillClient::SaveCreditCardOptions save_credit_card_options_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SaveCardBubbleLoggingTest,
    testing::Combine(testing::Bool(),
                     testing::Values("Local", "Upload"),
                     testing::Values("FirstShow", "Reshows"),
                     testing::ValuesIn(kSaveCardOptionParam)));

TEST_P(SaveCardBubbleLoggingTest, Metrics_ShowBubble) {
  base::HistogramTester histogram_tester;
  TriggerFlow();

  if (metrics_revamp_experiment_enabled_) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCreditCardPromptOffer" + GetHistogramNameSuffix(),
        AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.SaveCreditCardPromptOffer" + GetHistogramNameSuffix(), 0);
  }

  // Verifies legacy metrics are logged correctly. This does not depend on the
  // experiment flag.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt" +
                                     GetHistogramNameSuffix()),
      ElementsAre(
          Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
          Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN_DEPRECATED, 1)));
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_ShowIconOnly) {
  // This case does not happen when it is a reshow.
  if (show_ == "Reshows")
    return;

  base::HistogramTester histogram_tester;
  TriggerFlow(/*show_prompt=*/false);

  if (metrics_revamp_experiment_enabled_) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCreditCardPromptOffer" + GetHistogramNameSuffix(),
        AutofillMetrics::SAVE_CARD_PROMPT_NOT_SHOWN_MAX_STRIKES_REACHED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.SaveCreditCardPromptOffer" + GetHistogramNameSuffix(), 0);
  }

  // Verifies legacy metrics are logged correctly. This does not depend on the
  // experiment flag.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt" + GetHistogramNameSuffix(),
      AutofillMetrics::SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_SaveButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  controller()->OnSaveButton({});
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  if (metrics_revamp_experiment_enabled_) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
        AutofillMetrics::SAVE_CARD_PROMPT_ACCEPTED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(), 0);
  }

  // Verifies legacy metrics are logged correctly. This does not depend on the
  // experiment flag.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt" +
                                     GetHistogramNameSuffix()),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN_DEPRECATED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, 1)));
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_CancelButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  controller()->OnCancelButton();
  CloseBubble(PaymentsBubbleClosedReason::kCancelled);

  if (metrics_revamp_experiment_enabled_) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
        AutofillMetrics::SAVE_CARD_PROMPT_CANCELLED, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(), 0);
  }

  // Verifies legacy metrics are logged correctly. This does not depend on the
  // experiment flag.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt" +
                                     GetHistogramNameSuffix()),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN_DEPRECATED, 1),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, 1)));
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_Closed) {
  if (!metrics_revamp_experiment_enabled_)
    return;

  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      AutofillMetrics::SAVE_CARD_PROMPT_CLOSED, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_NotInteracted) {
  if (!metrics_revamp_experiment_enabled_)
    return;

  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      AutofillMetrics::SAVE_CARD_PROMPT_NOT_INTERACTED, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_LostFocus) {
  if (!metrics_revamp_experiment_enabled_)
    return;

  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      AutofillMetrics::SAVE_CARD_PROMPT_LOST_FOCUS, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_Unknown) {
  if (!metrics_revamp_experiment_enabled_)
    return;

  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      AutofillMetrics::SAVE_CARD_PROMPT_RESULT_UNKNOWN, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_SecurityLevel) {
  base::HistogramTester histogram_tester;
  controller()->set_security_level(security_state::SecurityLevel::SECURE);
  TriggerFlow();

  int expected_count = (show_ == "Reshows") ? 2 : 1;

  if (metrics_revamp_experiment_enabled_) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.SaveCreditCardPromptOffer." + destination_ + ".SECURE",
        AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, expected_count);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.SaveCreditCardPromptOffer." + destination_ + ".SECURE", 0);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SaveCreditCardPrompt." +
                                     destination_ + ".SECURE"),
      ElementsAre(Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED,
                         expected_count),
                  Bucket(AutofillMetrics::SAVE_CARD_PROMPT_SHOWN_DEPRECATED,
                         expected_count)));
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_LegalMessageLinkedClicked) {
  if (destination_ == "Local")
    return;

  TriggerFlow();
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  if (metrics_revamp_experiment_enabled_) {
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_CreditCardUpload_LegalMessageLinkClicked"));
  } else {
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Autofill_CreditCardUpload_LegalMessageLinkClicked"));
  }

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPrompt." + destination_ + "." + show_,
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE, 1);
}

// TODO(crbug.com/932818): Delete (manage card) or move (sign in promo) below
// tests when feature is fully launched.
class SaveCardBubbleControllerImplTestWithoutStatusChip
    : public SaveCardBubbleControllerImplTest {
 protected:
  SaveCardBubbleControllerImplTestWithoutStatusChip()
      : SaveCardBubbleControllerImplTest() {}
  ~SaveCardBubbleControllerImplTestWithoutStatusChip() override {}
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAutofillCreditCardUploadFeedback,
                               features::kAutofillEnableToolbarStatusChip});
    SaveCardBubbleControllerImplTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Local_FirstShow_SaveButton_SigninPromo) {
  ShowLocalBubble();
  ClickSaveButton();
  // Sign-in promo should be shown after accepting local save.
  EXPECT_EQ(BubbleType::SIGN_IN_PROMO, controller()->GetBubbleType());
  EXPECT_NE(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_Close_Navigate) {
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  CloseBubble();
  test_clock_.Advance(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();
  // Icon should disappear after navigating away.
  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_EQ(BubbleType::MANAGE_CARDS, controller()->GetBubbleType());
  EXPECT_NE(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Metrics_Local_ClickManageCardsDoneButton) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  EXPECT_EQ(BubbleType::MANAGE_CARDS, controller()->GetBubbleType());

  ClickSaveButton();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_DONE, 1)));
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Metrics_Local_ClickManageCardsManageCardsButton) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  controller()->OnManageCardsClicked();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1),
                  Bucket(AutofillMetrics::MANAGE_CARDS_MANAGE_CARDS, 1)));
}

TEST_F(
    SaveCardBubbleControllerImplTestWithoutStatusChip,
    Metrics_Local_FirstShow_SaveButton_Close_Reshow_Close_Reshow_ManageCards) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 2)));
}

TEST_F(
    SaveCardBubbleControllerImplTestWithoutStatusChip,
    Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_Close_Navigate) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  CloseBubble();
  test_clock_.Advance(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1)));
}

TEST_F(
    SaveCardBubbleControllerImplTestWithoutStatusChip,
    Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_Navigate) {
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  test_clock_.Advance(base::TimeDelta::FromSeconds(6));
  controller()->SimulateNavigation();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(AutofillMetrics::MANAGE_CARDS_SHOWN, 1)));
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Upload_FirstShow_SaveButton_NoSigninPromo) {
  ShowUploadBubble();
  ClickSaveButton();
  // Icon should disappear after an upload save,
  // even when this flag is enabled.
  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(nullptr, controller()->GetSaveCardBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Metrics_Upload_FirstShow_SaveButton_NoSigninPromo) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();
  ClickSaveButton();
  // No other bubbles should have popped up.
  histogram_tester.ExpectTotalCount("Autofill.SignInPromo", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Local", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Upload", 0);
}

TEST_F(SaveCardBubbleControllerImplTestWithoutStatusChip,
       Metrics_Upload_FirstShow_ManageCards) {
  base::HistogramTester histogram_tester;
  ShowUploadBubble();
  ClickSaveButton();
  controller()->ShowBubbleForManageCardsForTesting(
      autofill::test::GetCreditCard());
  // Icon should disappear after an upload save,
  // even when this flag is enabled.
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Local", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Upload", 1);
}

}  // namespace autofill
