// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include <stddef.h>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/manage_cards_prompt_metrics.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
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

  bool IsPaymentsSyncTransportEnabledWithoutSyncFeature() const override {
    return false;
  }

 private:
  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;
};

class SaveCardBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  SaveCardBubbleControllerImplTest() = default;
  SaveCardBubbleControllerImplTest(SaveCardBubbleControllerImplTest&) = delete;
  SaveCardBubbleControllerImplTest& operator=(
      SaveCardBubbleControllerImplTest&) = delete;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TestSaveCardBubbleControllerImpl::CreateForTesting(web_contents);
    test_clock_.SetNow(kArbitraryTime);
    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  }

  void TearDown() override {
    mock_sentiment_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void SetLegalMessage(
      const std::string& message_json,
      AutofillClient::SaveCreditCardOptions options =
          AutofillClient::SaveCreditCardOptions().with_show_prompt()) {
    absl::optional<base::Value> value(base::JSONReader::Read(message_json));
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->is_dict());
    LegalMessageLines legal_message_lines;
    LegalMessageLine::Parse(value->GetDict(), &legal_message_lines,
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
    if (controller()->ShouldShowPaymentSavedLabelAnimation()) {
      controller()->OnAnimationEnded();
    }
  }

 protected:
  TestSaveCardBubbleControllerImpl* controller() {
    return static_cast<TestSaveCardBubbleControllerImpl*>(
        TestSaveCardBubbleControllerImpl::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents()));
  }

  TestAutofillClock test_clock_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_ = nullptr;

 private:
  static void UploadSaveCardCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision,
      const AutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {}
  static void LocalSaveCardCallback(
      AutofillClient::SaveCardOfferUserDecision user_decision) {}
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

// Param of the SaveCardBubbleSingletonTestData:
// -- bool first_shown_is_local;
// -- bool second_and_third_shown_are_local;
typedef std::tuple<bool, bool> SaveCardBubbleSingletonTestData;

// One test case will be run several times till we cover all the param
// combinations of the |SaveCardBubbleSingletonTestData|. GetParam() will help
// get the specific param value for a particular run.
class SaveCardBubbleSingletonTest
    : public SaveCardBubbleControllerImplTest,
      public testing::WithParamInterface<SaveCardBubbleSingletonTestData> {
 public:
  SaveCardBubbleSingletonTest()
      : first_shown_is_local_(std::get<0>(GetParam())),
        second_and_third_shown_are_local_(std::get<1>(GetParam())) {}

  ~SaveCardBubbleSingletonTest() override = default;

  void ShowBubble(bool is_local) {
    is_local ? ShowLocalBubble() : ShowUploadBubble();
  }

  void TriggerFlow() {
    ShowBubble(first_shown_is_local_);
    ShowBubble(second_and_third_shown_are_local_);
    ShowBubble(second_and_third_shown_are_local_);
  }

  const bool first_shown_is_local_;
  const bool second_and_third_shown_are_local_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SaveCardBubbleSingletonTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool()));

TEST_P(SaveCardBubbleSingletonTest, OnlyOneActiveBubble) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  std::string suffix =
      first_shown_is_local_ ? ".Local.FirstShow" : ".Upload.FirstShow";

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer" + suffix,
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

// Note that even though in prod the four options in the SaveCreditCardOptions
// struct can be true at the same time, we don't support that in the test case
// (by the way we create histogram name suffixes).
struct SaveCardOptionParam {
  bool from_dynamic_change_form;
  bool has_non_focusable_field;
  bool should_request_name_from_user;
  bool should_request_expiration_date_from_user;
  bool has_multiple_legal_lines;
  bool has_same_last_four_as_server_card_but_different_expiration_date;
};

const SaveCardOptionParam kSaveCardOptionParam[] = {
    {false, false, false, false, false, false},
    {true, false, false, false, false, false},
    {false, true, false, false, false, false},
    {false, false, true, false, false, false},
    {false, false, false, true, false, false},
    {false, false, false, false, true, false},
    {false, false, false, false, false, true},
};

// Param of the SaveCardBubbleSingletonTestData:
// -- std::string destination
// -- std::string show
// -- SaveCardOptionParam save_card_option_param
typedef std::tuple<std::string, std::string, SaveCardOptionParam>
    SaveCardBubbleLoggingTestData;

// Test class to ensure the save card bubble events are logged correctly.
class SaveCardBubbleLoggingTest
    : public SaveCardBubbleControllerImplTest,
      public ::testing::WithParamInterface<SaveCardBubbleLoggingTestData> {
 public:
  SaveCardBubbleLoggingTest()
      : destination_(std::get<0>(GetParam())), show_(std::get<1>(GetParam())) {
    SaveCardOptionParam save_card_option_param = std::get<2>(GetParam());
    save_credit_card_options_ =
        AutofillClient::SaveCreditCardOptions()
            .with_from_dynamic_change_form(
                save_card_option_param.from_dynamic_change_form)
            .with_has_non_focusable_field(
                save_card_option_param.has_non_focusable_field)
            .with_should_request_name_from_user(
                save_card_option_param.should_request_name_from_user)
            .with_should_request_expiration_date_from_user(
                save_card_option_param.should_request_expiration_date_from_user)
            .with_has_multiple_legal_lines(
                save_card_option_param.has_multiple_legal_lines)
            .with_same_last_four_as_server_card_but_different_expiration_date(
                save_card_option_param
                    .has_same_last_four_as_server_card_but_different_expiration_date);
  }

  ~SaveCardBubbleLoggingTest() override = default;

  void TriggerFlow(bool show_prompt = true) {
    if (destination_ == "Local") {
      if (show_ == "FirstShow") {
        ShowLocalBubble(/*card=*/nullptr,
                        /*options=*/GetSaveCreditCardOptions().with_show_prompt(
                            show_prompt));
      } else {
        ASSERT_EQ(show_, "Reshows");
        ShowLocalBubble(/*card=*/nullptr,
                        /*options=*/GetSaveCreditCardOptions().with_show_prompt(
                            show_prompt));
        CloseAndReshowBubble();
      }
    } else {
      ASSERT_EQ(destination_, "Upload");
      if (show_ == "FirstShow") {
        ShowUploadBubble(
            GetSaveCreditCardOptions().with_show_prompt(show_prompt));
      } else {
        ASSERT_EQ(show_, "Reshows");
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

    if (GetSaveCreditCardOptions().has_multiple_legal_lines) {
      result += ".WithMultipleLegalLines";
    }

    if (GetSaveCreditCardOptions()
            .has_same_last_four_as_server_card_but_different_expiration_date) {
      result += ".WithSameLastFourButDifferentExpiration";
    }

    return result;
  }

  const std::string destination_;
  const std::string show_;

 private:
  AutofillClient::SaveCreditCardOptions save_credit_card_options_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SaveCardBubbleLoggingTest,
    testing::Combine(testing::Values("Local", "Upload"),
                     testing::Values("FirstShow", "Reshows"),
                     testing::ValuesIn(kSaveCardOptionParam)));

TEST_P(SaveCardBubbleLoggingTest, Metrics_ShowBubble) {
  base::HistogramTester histogram_tester;
  TriggerFlow();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_ShowIconOnly) {
  // This case does not happen when it is a reshow.
  if (show_ == "Reshows")
    return;

  base::HistogramTester histogram_tester;
  TriggerFlow(/*show_prompt=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_SaveButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  controller()->OnSaveButton({});
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_CancelButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kCancelled);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptResult::kCancelled, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_Closed) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptResult::kClosed, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_NotInteracted) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptResult::kNotInteracted, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_LostFocus) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptResult::kLostFocus, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_Unknown) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult" + GetHistogramNameSuffix(),
      autofill_metrics::SaveCardPromptResult::kUnknown, 1);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_SecurityLevel) {
  base::HistogramTester histogram_tester;
  controller()->set_security_level(security_state::SecurityLevel::SECURE);
  TriggerFlow();

  int expected_count = (show_ == "Reshows") ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer." + destination_ + ".SECURE",
      autofill_metrics::SaveCardPromptOffer::kShown, expected_count);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_LegalMessageLinkedClicked) {
  if (destination_ == "Local")
    return;

  TriggerFlow();
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_CreditCardUpload_LegalMessageLinkClicked"));
}

class SaveCvcBubbleLoggingTest
    : public SaveCardBubbleControllerImplTest,
      public testing::WithParamInterface<std::string> {
 public:
  SaveCvcBubbleLoggingTest() : show_(GetParam()) {}
  ~SaveCvcBubbleLoggingTest() override = default;

  void TriggerFlow(bool show_prompt = true) {
    if (show_ == "FirstShow") {
      ShowLocalBubble(
          /*card=*/nullptr,
          /*options=*/AutofillClient::SaveCreditCardOptions()
              .with_card_save_type(AutofillClient::CardSaveType::kCvcSaveOnly)
              .with_show_prompt(show_prompt));
    } else {
      ASSERT_EQ(show_, "Reshows");
      ShowLocalBubble(
          /*card=*/nullptr,
          /*options=*/AutofillClient::SaveCreditCardOptions()
              .with_card_save_type(AutofillClient::CardSaveType::kCvcSaveOnly)
              .with_show_prompt(show_prompt));
      CloseAndReshowBubble();
    }
  }

  const std::string show_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SaveCvcBubbleLoggingTest,
                         testing::Values("FirstShow", "Reshows"));

TEST_P(SaveCvcBubbleLoggingTest, Metrics_ShowBubble) {
  base::HistogramTester histogram_tester;
  TriggerFlow();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptOffer.Local." + show_,
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_ShowIconOnly) {
  // This case does not happen when it is a reshow.
  if (show_ == "Reshows") {
    return;
  }

  base::HistogramTester histogram_tester;
  TriggerFlow(/*show_prompt=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptOffer.Local." + show_,
      autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_SaveButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  controller()->OnSaveButton({});
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult.Local." + show_,
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_CancelButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kCancelled);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult.Local." + show_,
      autofill_metrics::SaveCardPromptResult::kCancelled, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_Closed) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult.Local." + show_,
      autofill_metrics::SaveCardPromptResult::kClosed, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_NotInteracted) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult.Local." + show_,
      autofill_metrics::SaveCardPromptResult::kNotInteracted, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_LostFocus) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult.Local." + show_,
      autofill_metrics::SaveCardPromptResult::kLostFocus, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_Unknown) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult.Local." + show_,
      autofill_metrics::SaveCardPromptResult::kUnknown, 1);
}

TEST_F(SaveCardBubbleControllerImplTest, LocalCardSaveOnlyDialogContent) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableCvcStorageAndFilling);

  // Show the local card save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/AutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveOnly)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save card?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card to your device");
}

TEST_F(SaveCardBubbleControllerImplTest, LocalCardSaveWithCvcDialogContent) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableCvcStorageAndFilling);

  // Show the local card save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/AutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save card?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card, and security code to "
            u"your device");
}

TEST_F(SaveCardBubbleControllerImplTest, LocalCvcOnlySaveDialogContent) {
  // Show the local CVC save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/AutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCvcSaveOnly)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_CVC_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save security code?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"For faster checkout, save the CVC for this card to your device");
}

TEST_F(SaveCardBubbleControllerImplTest, UploadCardSaveDialogContent) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableNewSaveCardBubbleUi);

  // Show the server card save bubble.
  ShowUploadBubble(
      /*options=*/AutofillClient::SaveCreditCardOptions().with_show_prompt(
          true));

  ASSERT_EQ(BubbleType::UPLOAD_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save card?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"Pay faster next time and protect your card with Google’s "
            u"industry-leading security.");
}

TEST_F(SaveCardBubbleControllerImplTest,
       LocalCard_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);

  // Show the local card save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/AutofillClient::SaveCreditCardOptions().with_card_save_type(
          AutofillClient::CardSaveType::kCardSaveOnly));
  ClickSaveButton();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring up the
  // Manage cards bubble. Verify that the icon tooltip, the title for the
  // bubble, and the save animation reflect the correct info.
  ASSERT_EQ(BubbleType::MANAGE_CARDS, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Card saved");
  EXPECT_EQ(controller()->GetSavePaymentIconTooltipText(), u"Save card");
  EXPECT_EQ(controller()->GetSaveSuccessAnimationStringId(),
            IDS_AUTOFILL_CARD_SAVED);
}

TEST_F(SaveCardBubbleControllerImplTest,
       LocalCvc_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);

  // Show the local CVC save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/AutofillClient::SaveCreditCardOptions().with_card_save_type(
          AutofillClient::CardSaveType::kCvcSaveOnly));
  ClickSaveButton();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring up the
  // Manage cards bubble. Verify that the icon tooltip, the title for the
  // bubble, and the save animation reflect the correct info.
  ASSERT_EQ(BubbleType::MANAGE_CARDS, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"CVC saved");
  EXPECT_EQ(controller()->GetSavePaymentIconTooltipText(), u"Save CVC");
  EXPECT_EQ(controller()->GetSaveSuccessAnimationStringId(),
            IDS_AUTOFILL_CVC_SAVED);
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_ClickManageCardsDoneButton) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  ASSERT_EQ(BubbleType::MANAGE_CARDS, controller()->GetBubbleType());

  ClickSaveButton();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(ManageCardsPromptMetric::kManageCardsShown, 1),
                  Bucket(ManageCardsPromptMetric::kManageCardsDone, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Local_ClickManageCardsManageCardsButton) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  controller()->OnManageCardsClicked();
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(ManageCardsPromptMetric::kManageCardsShown, 1),
                  Bucket(ManageCardsPromptMetric::kManageCardsManageCards, 1)));
}

TEST_F(
    SaveCardBubbleControllerImplTest,
    Metrics_Local_FirstShow_SaveButton_Close_Reshow_Close_Reshow_ManageCards) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(ManageCardsPromptMetric::kManageCardsShown, 2)));
}

TEST_F(
    SaveCardBubbleControllerImplTest,
    Metrics_Local_FirstShow_SaveButton_SigninPromo_Close_Reshow_ManageCards) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
  base::HistogramTester histogram_tester;
  ShowLocalBubble();
  ClickSaveButton();
  CloseAndReshowBubble();
  // After closing the sign-in promo, clicking the icon should bring
  // up the Manage cards bubble.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt.Local"),
      ElementsAre(Bucket(ManageCardsPromptMetric::kManageCardsShown, 1)));
}

TEST_F(SaveCardBubbleControllerImplTest,
       Upload_FirstShow_SaveButton_NoSigninPromo) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
  ShowUploadBubble();
  ClickSaveButton();
  // Icon should disappear after an upload save,
  // even when this flag is enabled.
  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(nullptr, controller()->GetPaymentBubbleView());
}

TEST_F(SaveCardBubbleControllerImplTest,
       Metrics_Upload_FirstShow_SaveButton_NoSigninPromo) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
  base::HistogramTester histogram_tester;
  ShowUploadBubble();
  ClickSaveButton();
  // No other bubbles should have popped up.
  histogram_tester.ExpectTotalCount("Autofill.SignInPromo", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Local", 0);
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt.Upload", 0);
}

TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_FirstShow_ManageCards) {
  EXPECT_CALL(*mock_sentiment_service_, SavedCard()).Times(1);
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
