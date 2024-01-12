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

  void set_autofill_bubble_view(AutofillBubbleBase* bubble_view) {
    set_bubble_view(bubble_view);
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
    TestSaveCardBubbleControllerImpl::CreateForTesting(active_web_contents());
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
    std::optional<base::Value> value(base::JSONReader::Read(message_json));
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
    if (options.card_save_type == AutofillClient::CardSaveType::kCvcSaveOnly) {
      SetLegalMessage("{}", options);
      return;
    }
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
    controller()->ReshowBubble(/*is_user_gesture=*/true);
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
            active_web_contents()));
  }

  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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
  bool should_request_name_from_user;
  bool should_request_expiration_date_from_user;
  bool has_multiple_legal_lines;
  bool has_same_last_four_as_server_card_but_different_expiration_date;
  AutofillClient::CardSaveType card_save_type;
};

const SaveCardOptionParam kSaveCardOptionParam[] = {
    {false, false, false, false, AutofillClient::CardSaveType::kCardSaveOnly},
    {true, false, false, false, AutofillClient::CardSaveType::kCardSaveOnly},
    {false, true, false, false, AutofillClient::CardSaveType::kCardSaveOnly},
    {false, false, true, false, AutofillClient::CardSaveType::kCardSaveOnly},
    {false, false, false, true, AutofillClient::CardSaveType::kCardSaveOnly},
    {false, false, false, false,
     AutofillClient::CardSaveType::kCardSaveWithCvc}};

// Param of the SaveCardBubbleSingletonTestData:
// -- std::string save_destination
// -- std::string show_type
// -- SaveCardOptionParam save_card_option_param
typedef std::tuple<std::string, std::string, SaveCardOptionParam>
    SaveCardBubbleLoggingTestData;

// Test class to ensure the save card bubble events are logged correctly.
class SaveCardBubbleLoggingTest
    : public SaveCardBubbleControllerImplTest,
      public ::testing::WithParamInterface<SaveCardBubbleLoggingTestData> {
 public:
  SaveCardBubbleLoggingTest()
      : save_destination_(std::get<0>(GetParam())),
        show_type_(std::get<1>(GetParam())) {
    SaveCardOptionParam save_card_option_param = std::get<2>(GetParam());
    save_credit_card_options_ =
        AutofillClient::SaveCreditCardOptions()
            .with_should_request_name_from_user(
                save_card_option_param.should_request_name_from_user)
            .with_should_request_expiration_date_from_user(
                save_card_option_param.should_request_expiration_date_from_user)
            .with_has_multiple_legal_lines(
                save_card_option_param.has_multiple_legal_lines)
            .with_same_last_four_as_server_card_but_different_expiration_date(
                save_card_option_param
                    .has_same_last_four_as_server_card_but_different_expiration_date)
            .with_card_save_type(save_card_option_param.card_save_type);
  }

  ~SaveCardBubbleLoggingTest() override = default;

  void TriggerFlow(bool show_prompt = true) {
    if (save_destination_ == "Local") {
      if (show_type_ == "FirstShow") {
        ShowLocalBubble(/*card=*/nullptr,
                        /*options=*/GetSaveCreditCardOptions().with_show_prompt(
                            show_prompt));
      } else {
        ASSERT_EQ(show_type_, "Reshows");
        ShowLocalBubble(/*card=*/nullptr,
                        /*options=*/GetSaveCreditCardOptions().with_show_prompt(
                            show_prompt));
        CloseAndReshowBubble();
      }
    } else {
      ASSERT_EQ(save_destination_, "Upload");
      if (show_type_ == "FirstShow") {
        ShowUploadBubble(
            GetSaveCreditCardOptions().with_show_prompt(show_prompt));
      } else {
        ASSERT_EQ(show_type_, "Reshows");
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
    std::string result = "." + save_destination_ + "." + show_type_;

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
    if (GetSaveCreditCardOptions().card_save_type ==
        AutofillClient::CardSaveType::kCardSaveWithCvc) {
      result += ".SavingWithCvc";
    }

    return result;
  }

  const std::string save_destination_;
  const std::string show_type_;

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
  if (show_type_ == "Reshows") {
    return;
  }

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

  int expected_count = (show_type_ == "Reshows") ? 2 : 1;

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer." + save_destination_ + ".SECURE",
      autofill_metrics::SaveCardPromptOffer::kShown, expected_count);
}

TEST_P(SaveCardBubbleLoggingTest, Metrics_LegalMessageLinkedClicked) {
  if (save_destination_ == "Local") {
    return;
  }

  TriggerFlow();
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  controller()->OnLegalMessageLinkClicked(GURL("http://www.example.com"));

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_CreditCardUpload_LegalMessageLinkClicked"));
}

// Param of the SaveCvcBubbleLoggingTest:
// -- std::string show_type: decides if the view is shown first time or
// re-shown.
// -- std::string save_destination decides if card or CVC will be saved locally
// or to the server.
class SaveCvcBubbleLoggingTest
    : public SaveCardBubbleControllerImplTest,
      public testing::WithParamInterface<std::tuple<std::string, std::string>> {
 public:
  SaveCvcBubbleLoggingTest()
      : show_type_(std::get<0>(GetParam())),
        save_destination_(std::get<1>(GetParam())) {}
  ~SaveCvcBubbleLoggingTest() override = default;

  void TriggerFlow(bool show_prompt = true) {
    ASSERT_TRUE(show_type_ == "FirstShow" || show_type_ == "Reshows");
    if (save_destination_ == "Upload") {
      ShowUploadBubble(
          /*options=*/AutofillClient::SaveCreditCardOptions()
              .with_card_save_type(AutofillClient::CardSaveType::kCvcSaveOnly)
              .with_show_prompt(show_prompt));
    } else {
      ASSERT_EQ(save_destination_, "Local");
      ShowLocalBubble(
          /*card=*/nullptr,
          /*options=*/AutofillClient::SaveCreditCardOptions()
              .with_card_save_type(AutofillClient::CardSaveType::kCvcSaveOnly)
              .with_show_prompt(show_prompt));
    }

    if (show_type_ == "Reshows") {
      CloseAndReshowBubble();
    }
  }

  const std::string show_type_;
  const std::string save_destination_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SaveCvcBubbleLoggingTest,
                         testing::Combine(testing::Values("FirstShow",
                                                          "Reshows"),
                                          testing::Values("Upload", "Local")));

TEST_P(SaveCvcBubbleLoggingTest, Metrics_ShowBubble) {
  base::HistogramTester histogram_tester;
  TriggerFlow();

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptOffer." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_ShowIconOnly) {
  // This case does not happen when it is a reshow.
  if (show_type_ == "Reshows") {
    return;
  }

  base::HistogramTester histogram_tester;
  TriggerFlow(/*show_prompt=*/false);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptOffer." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptOffer::kNotShownMaxStrikesReached, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_SaveButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  controller()->OnSaveButton({});
  CloseBubble(PaymentsBubbleClosedReason::kAccepted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_CancelButton) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kCancelled);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptResult::kCancelled, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_Closed) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kClosed);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptResult::kClosed, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_NotInteracted) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kNotInteracted);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptResult::kNotInteracted, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_LostFocus) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kLostFocus);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult." + save_destination_ + "." + show_type_,
      autofill_metrics::SaveCardPromptResult::kLostFocus, 1);
}

TEST_P(SaveCvcBubbleLoggingTest, Metrics_Unknown) {
  base::HistogramTester histogram_tester;
  TriggerFlow();
  CloseBubble(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCvcPromptResult." + save_destination_ + "." + show_type_,
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

  // Show the local card save with CVC bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/AutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save card?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card and encrypted security "
            u"code to your device");
}

TEST_F(SaveCardBubbleControllerImplTest, UploadCardSaveWithCvcDialogContent) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableCvcStorageAndFilling);

  // Show the server card save with CVC bubble.
  ShowUploadBubble(
      /*options=*/AutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::UPLOAD_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card, encrypted security "
            u"code, and billing address in your Google Account");
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
            u"This card's CVC will be encrypted and saved to your device for "
            u"faster checkout");
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

TEST_F(SaveCardBubbleControllerImplTest, HideIconAndBubbleAfterUpload) {
  ShowUploadBubble();

  EXPECT_TRUE(controller()->IsIconVisible());
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  controller()->HideIconAndBubbleAfterUpload();
  CloseBubble();

  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::INACTIVE);
}

TEST_F(SaveCardBubbleControllerImplTest, UploadCvcOnlySaveDialogContent) {
  // Show the server CVC save bubble.
  ShowUploadBubble(
      /*options=*/AutofillClient::SaveCreditCardOptions()
          .with_card_save_type(AutofillClient::CardSaveType::kCvcSaveOnly)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::UPLOAD_CVC_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save security code?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"This card's CVC will be encrypted and saved in your Google "
            u"Account for faster checkout");
  EXPECT_TRUE(controller()->GetLegalMessageLines().empty());
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

class MockAutofillBubbleBase final : public AutofillBubbleBase {
 public:
  MOCK_METHOD(void, Hide, (), (override));
};

class SaveCardBubbleControllerImplTestWithLoadingAndConfirmation
    : public SaveCardBubbleControllerImplTest {
 public:
  SaveCardBubbleControllerImplTestWithLoadingAndConfirmation() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableSaveCardLoadingAndConfirmation);
  }

  void SetUp() override {
    SaveCardBubbleControllerImplTest::SetUp();

    // Set the visibility to VISIBLE as the web contents are initially hidden.
    active_web_contents()->UpdateWebContentsVisibility(
        content::Visibility::VISIBLE);
  }

 protected:
  TabHandle tab_handle() {
    return browser()->tab_strip_model()->GetTabHandleAt(
        browser()->tab_strip_model()->active_index());
  }
};

// Test that `Accepted` metric is recorded on upload card save.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_Upload_OnSave) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);
}

// Test that metrics are not recorded in
// SaveCardBubbleController::OnSaveButton() on local card save.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_Local_OnSave) {
  base::HistogramTester histogram_tester;

  ShowLocalBubble();
  controller()->OnSaveButton({});

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 0);
}

// Test that metrics are not recorded when the save card bubble is
// programmatically closed after the save card upload completes. They should be
// recorded at the time save is accepted, because accepting save no longer
// immediately closes the bubble.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_Upload_HideAfterUpload_CloseBubble) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->HideIconAndBubbleAfterUpload();
  CloseBubble();

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 0);
}

// Test that after changing tabs, when returning to the tab with the save card,
// the bubble view is no longer showing but can be accessed through the icon.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Upload_HideBubble) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  MockAutofillBubbleBase save_card_bubble_view;
  controller()->set_autofill_bubble_view(&save_card_bubble_view);
  EXPECT_CALL(save_card_bubble_view, Hide);

  // Simulate switching to a different tab.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kUnknown);

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 1);

  // Simulate returning to tab where bubble was previously shown.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());
}

// Test that after a link is clicked in the save card bubble view; and one
// returns to the tab with the save card, the bubble view is automatically
// re-shown without user prompt.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Upload_ReshowAfterLinkClick) {
  TabHandle tab = tab_handle();

  ShowUploadBubble();

  MockAutofillBubbleBase save_card_bubble_view;
  testing::MockFunction<void(std::string check_point_name)> check_hide;
  {
    testing::InSequence s;

    EXPECT_CALL(save_card_bubble_view, Hide);
    EXPECT_CALL(check_hide, Call("1"));
    EXPECT_CALL(save_card_bubble_view, Hide);
    EXPECT_CALL(check_hide, Call("2"));
  }

  controller()->set_autofill_bubble_view(&save_card_bubble_view);

  controller()->OnLegalMessageLinkClicked(GURL("about:blank"));
  // Change active web contents back to previous tab so that
  // active_web_contents() and controller() return the correct object.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfTab(tab));

  // Usually, the visibility changes when changing tabs but it doesn't in the
  // test so it needs to be simulated.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  // Simulate AutofillBubbleBase::Hide() by calling
  // SaveCardBubbleControllerImpl::OnBubbleClosed().
  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kUnknown);
  check_hide.Call("1");

  // Check that the bubble is shown when returning to the tab which previously
  // showed the bubble.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);
  controller()->set_autofill_bubble_view(&save_card_bubble_view);

  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());

  // Check that the WebContents showing a subsequent time does not show the
  // bubble view.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kUnknown);
  check_hide.Call("2");

  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
  EXPECT_TRUE(controller()->IsIconVisible());
}

// Test the metrics for reshowing the bubble view after a link is clicked.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Metric_Upload_ReshowAfterLinkClick) {
  base::HistogramTester histogram_tester;
  TabHandle tab = tab_handle();

  ShowUploadBubble();
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer.Upload.FirstShow",
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer.Upload.Reshows",
      autofill_metrics::SaveCardPromptOffer::kShown, 0);

  controller()->OnLegalMessageLinkClicked(GURL("about:blank"));
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfTab(tab));

  // Usually, the visibility changes when changing tabs but it doesn't in the
  // test so it needs to be simulated.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kUnknown);

  // Ensure that closing the bubble through clicking a link does not get logged
  // to the metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.Reshows", 0);

  // Reshow bubble view.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  // Expect the prompt metric not to change from the initial bubble showing
  // because this is a reshowing after returning to the original tab after a
  // link click.
  // TODO(b/316391673): Determine if a different metric (or the re-show metric)
  // should be tracking this re-show.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer.Upload.FirstShow",
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer.Upload.Reshows",
      autofill_metrics::SaveCardPromptOffer::kShown, 0);

  // Ensure that metrics are recorded on a subsequent bubble close.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  controller()->OnBubbleClosed(PaymentsBubbleClosedReason::kUnknown);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.Reshows", 1);
}

}  // namespace autofill
