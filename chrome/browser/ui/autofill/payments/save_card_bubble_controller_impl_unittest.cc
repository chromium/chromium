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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_card_ui.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
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
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::Bucket;
using testing::ElementsAre;

namespace autofill {
namespace {

using CardSaveType = payments::PaymentsAutofillClient::CardSaveType;
using SaveCreditCardOptions =
    payments::PaymentsAutofillClient::SaveCreditCardOptions;

const base::Time kArbitraryTime = base::Time::FromTimeT(1234567890);

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager =
      std::make_unique<autofill::TestPersonalDataManager>();
  personal_data_manager->test_payments_data_manager()
      .SetAutofillPaymentMethodsEnabled(true);
  return personal_data_manager;
}

// Test AutofillBubbleBase implementation which:
// - Notifies the controller when the bubble hides (to match prod).
// - Tracks the bubble's visibility.
class ObserveHideTestAutofillBubble : public AutofillBubbleBase {
 public:
  explicit ObserveHideTestAutofillBubble(content::WebContents* web_contents)
      : web_contents_(web_contents->GetWeakPtr()) {}
  virtual ~ObserveHideTestAutofillBubble() = default;

  void Show() { is_visible_ = true; }

  void Hide() override {
    // Call OnBubbleClosed() because the real implementation does so.
    if (web_contents_) {
      auto* controller = static_cast<SaveCardBubbleControllerImpl*>(
          SaveCardBubbleControllerImpl::FromWebContents(web_contents_.get()));
      controller->OnBubbleClosed(PaymentsBubbleClosedReason::kUnknown);
    }

    is_visible_ = false;
  }

  bool is_visible() { return is_visible_; }

 private:
  // WeakPtr because ObserveHideTestAutofillBubble outlives the WebContents in
  // tests.
  base::WeakPtr<content::WebContents> web_contents_;

  bool is_visible_;
};

// TestAutofillBubbleHandler which provides access to bubbles it creates.
class ExposeBubbleAutofillBubbleHandler : public TestAutofillBubbleHandler {
 public:
  ExposeBubbleAutofillBubbleHandler() = default;
  ExposeBubbleAutofillBubbleHandler(const ExposeBubbleAutofillBubbleHandler&) =
      delete;
  ExposeBubbleAutofillBubbleHandler& operator=(
      const ExposeBubbleAutofillBubbleHandler&) = delete;
  ~ExposeBubbleAutofillBubbleHandler() override = default;

  AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller,
      bool is_use_gesture) override {
    if (!save_card_bubble_) {
      save_card_bubble_ =
          std::make_unique<ObserveHideTestAutofillBubble>(web_contents);
    }
    save_card_bubble_->Show();
    return save_card_bubble_.get();
  }

  AutofillBubbleBase* ShowSaveCardConfirmationBubble(
      content::WebContents* web_contents,
      SaveCardBubbleController* controller) override {
    if (!confirmation_bubble_) {
      confirmation_bubble_ =
          std::make_unique<ObserveHideTestAutofillBubble>(web_contents);
    }
    confirmation_bubble_->Show();
    return confirmation_bubble_.get();
  }

  bool is_save_card_bubble_visible() {
    return save_card_bubble_ && save_card_bubble_->is_visible();
  }

  bool is_confirmation_bubble_visible() {
    return confirmation_bubble_ && confirmation_bubble_->is_visible();
  }

 private:
  std::unique_ptr<ObserveHideTestAutofillBubble> save_card_bubble_;
  std::unique_ptr<ObserveHideTestAutofillBubble> confirmation_bubble_;
};

class TestBrowserWindowWithAutofillHandler : public TestBrowserWindow {
 public:
  TestBrowserWindowWithAutofillHandler() = default;
  ~TestBrowserWindowWithAutofillHandler() override = default;
  TestBrowserWindowWithAutofillHandler(
      const TestBrowserWindowWithAutofillHandler&) = delete;
  TestBrowserWindowWithAutofillHandler& operator=(
      const TestBrowserWindowWithAutofillHandler&) = delete;

  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() override {
    return &handler_;
  }

 private:
  ExposeBubbleAutofillBubbleHandler handler_;
};

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

  void SimulateNavigation() {
    content::MockNavigationHandle handle;
    handle.set_has_committed(true);
    DidFinishNavigation(&handle);
  }

 protected:
  bool IsPaymentsSyncTransportEnabledWithoutSyncFeature() const override {
    return false;
  }
};

class SaveCardBubbleControllerImplTest : public BrowserWithTestWindowTest {
 public:
  explicit SaveCardBubbleControllerImplTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : BrowserWithTestWindowTest(time_source),
        dependency_manager_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &SaveCardBubbleControllerImplTest::SetTestingFactories,
                    base::Unretained(this)))) {
    scoped_feature_list_.InitWithFeatureStates({
        {features::kAutofillEnableCvcStorageAndFilling, false},
        {features::kAutofillEnableSaveCardLoadingAndConfirmation, false},
    });
  }

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
    did_on_confirmation_closed_callback_run_ = false;
    personal_data_manager()->test_payments_data_manager().ClearCreditCards();
    BrowserWithTestWindowTest::TearDown();
  }

  // BrowserWithTestWindowTest:
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    std::unique_ptr<TestBrowserWindowWithAutofillHandler> window =
        std::make_unique<TestBrowserWindowWithAutofillHandler>();
    window->set_is_active(true);
    return std::move(window);
  }

  ExposeBubbleAutofillBubbleHandler* GetAutofillBubbleHandler() {
    return static_cast<ExposeBubbleAutofillBubbleHandler*>(
        window()->GetAutofillBubbleHandler());
  }

  bool IsSaveCardBubbleVisible() {
    return GetAutofillBubbleHandler()->is_save_card_bubble_visible();
  }

  bool IsConfirmationBubbleVisible() {
    return GetAutofillBubbleHandler()->is_confirmation_bubble_visible();
  }

  void SetLegalMessage(const std::string& message_json,
                       SaveCreditCardOptions options =
                           SaveCreditCardOptions().with_show_prompt()) {
    std::optional<base::Value> value(base::JSONReader::Read(message_json));
    ASSERT_TRUE(value);
    ASSERT_TRUE(value->is_dict());
    LegalMessageLines legal_message_lines;
    LegalMessageLine::Parse(value->GetDict(), &legal_message_lines,
                            /*escape_apostrophes=*/true);
    controller()->OfferUploadSave(CreditCard(), legal_message_lines, options,
                                  base::BindOnce(&UploadSaveCardCallback));
  }

  void ShowLocalBubble(const CreditCard* card = nullptr,
                       SaveCreditCardOptions options =
                           SaveCreditCardOptions().with_show_prompt()) {
    controller()->OfferLocalSave(
        card ? CreditCard(*card)
             : autofill::test::GetCreditCard(),  // Visa by default
        options, base::BindOnce(&LocalSaveCardCallback));
  }

  void ShowUploadBubble(SaveCreditCardOptions options =
                            SaveCreditCardOptions().with_show_prompt()) {
    if (options.card_save_type == CardSaveType::kCvcSaveOnly) {
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

  void ShowConfirmationBubbleView(bool card_saved) {
    controller()->ShowConfirmationBubbleView(
        /*card_saved=*/card_saved,
        /*on_confirmation_closed_callback=*/base::BindOnce(
            &SaveCardBubbleControllerImplTest::OnConfirmationClosedCallback,
            weak_ptr_factory_.GetWeakPtr()));
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

  void AddCreditCard(const CreditCard& card) {
    personal_data_manager()->test_payments_data_manager().AddCreditCard(card);
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

  TestPersonalDataManager* personal_data_manager() {
    return static_cast<TestPersonalDataManager*>(
        PersonalDataManagerFactory::GetForBrowserContext(profile()));
  }

  TestAutofillClock test_clock_;
  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_ = nullptr;
  bool did_on_confirmation_closed_callback_run_ = false;

 private:
  static void UploadSaveCardCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
      const payments::PaymentsAutofillClient::UserProvidedCardDetails&
          user_provided_card_details) {}
  static void LocalSaveCardCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision
          user_decision) {}
  void OnConfirmationClosedCallback() {
    did_on_confirmation_closed_callback_run_ = true;
  }
  void SetTestingFactories(content::BrowserContext* context) {
    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestPersonalDataManager));
  }

  base::CallbackListSubscription dependency_manager_subscription_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::WeakPtrFactory<SaveCardBubbleControllerImplTest> weak_ptr_factory_{
      this};
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
  ShowUploadBubble(SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());
  EXPECT_TRUE(controller()->ShouldRequestNameFromUser());
}

TEST_F(SaveCardBubbleControllerImplTest,
       PropagateShouldRequestExpirationDateFromUserWhenFalse) {
  ShowUploadBubble(SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_show_prompt());

  EXPECT_FALSE(controller()->ShouldRequestExpirationDateFromUser());
}

TEST_F(SaveCardBubbleControllerImplTest,
       PropagateShouldRequestExpirationDateFromUserWhenTrue) {
  ShowUploadBubble(SaveCreditCardOptions()
                       .with_should_request_name_from_user(true)
                       .with_should_request_expiration_date_from_user(true)
                       .with_show_prompt());

  EXPECT_TRUE(controller()->ShouldRequestExpirationDateFromUser());
}

using SaveCreditCardPromptResultMetricTestData =
    std::tuple<PaymentsBubbleClosedReason,
               autofill_metrics::SaveCardPromptResult>;

// Test fixture to ensure the correct reporting of UMA metric
// Autofill.SaveCreditCardPromptResult{SaveDestination}.{UserGroup}.
class SaveCreditCardPromptResultMetricTest
    : public SaveCardBubbleControllerImplTest,
      public testing::WithParamInterface<
          SaveCreditCardPromptResultMetricTestData> {
 public:
  SaveCreditCardPromptResultMetricTest()
      : closed_reason_(std::get<0>(GetParam())),
        prompt_result_(std::get<1>(GetParam())) {}
  ~SaveCreditCardPromptResultMetricTest() override = default;

 protected:
  const PaymentsBubbleClosedReason closed_reason_;
  const autofill_metrics::SaveCardPromptResult prompt_result_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SaveCreditCardPromptResultMetricTest,
    testing::Values(SaveCreditCardPromptResultMetricTestData(
                        PaymentsBubbleClosedReason::kAccepted,
                        autofill_metrics::SaveCardPromptResult::kAccepted),
                    SaveCreditCardPromptResultMetricTestData(
                        PaymentsBubbleClosedReason::kCancelled,
                        autofill_metrics::SaveCardPromptResult::kCancelled),
                    SaveCreditCardPromptResultMetricTestData(
                        PaymentsBubbleClosedReason::kClosed,
                        autofill_metrics::SaveCardPromptResult::kClosed),
                    SaveCreditCardPromptResultMetricTestData(
                        PaymentsBubbleClosedReason::kNotInteracted,
                        autofill_metrics::SaveCardPromptResult::kNotInteracted),
                    SaveCreditCardPromptResultMetricTestData(
                        PaymentsBubbleClosedReason::kLostFocus,
                        autofill_metrics::SaveCardPromptResult::kLostFocus)));

// Tests that after the user interacts with a "save *local* card" dialog and
// *does not* have any card data on file, metrics
// Autofill.SaveCreditCardPromptResult.Local.Aggregate and .UserHasNoCards are
// recorded.
TEST_P(SaveCreditCardPromptResultMetricTest,
       EmitsSavePromptResultLocalHasNoCards) {
  personal_data_manager()->test_payments_data_manager().ClearCreditCards();
  base::HistogramTester histogram_tester;
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/SaveCreditCardOptions().with_show_prompt(true));
  if (closed_reason_ == PaymentsBubbleClosedReason::kAccepted) {
    controller()->OnSaveButton({});
  }
  CloseBubble(closed_reason_);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Local.Aggregate", prompt_result_, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Local.UserHasNoCards",
      prompt_result_, 1);
}

// Tests that after the user interacts with a "save *server* card" dialog and
// *does not* have any card data on file, metrics
// Autofill.SaveCreditCardPromptResult.Upload.Aggregate and .UserHasNoCards are
// recorded.
TEST_P(SaveCreditCardPromptResultMetricTest,
       EmitsSavePromptResultUploadHasNoCards) {
  personal_data_manager()->test_payments_data_manager().ClearCreditCards();
  base::HistogramTester histogram_tester;
  ShowUploadBubble(SaveCreditCardOptions().with_show_prompt(true));
  if (closed_reason_ == PaymentsBubbleClosedReason::kAccepted) {
    controller()->OnSaveButton({});
  }
  CloseBubble(closed_reason_);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.Aggregate", prompt_result_,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.UserHasNoCards",
      prompt_result_, 1);
}

// Tests that after the user interacts with a "save *local* card" dialog and
// *does* have card data on file, metrics
// Autofill.SaveCreditCardPromptResult.Local.Aggregate and .UserHasSavedCards
// are recorded.
TEST_P(SaveCreditCardPromptResultMetricTest,
       EmitsSavePromptResultLocalHasSavedCards) {
  personal_data_manager()->test_payments_data_manager().ClearCreditCards();
  AddCreditCard(test::GetCreditCard());
  base::HistogramTester histogram_tester;
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/SaveCreditCardOptions().with_show_prompt(true));
  if (closed_reason_ == PaymentsBubbleClosedReason::kAccepted) {
    controller()->OnSaveButton({});
  }
  CloseBubble(closed_reason_);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Local.Aggregate", prompt_result_, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Local.UserHasSavedCards",
      prompt_result_, 1);
}

// Tests that after the user interacts with a "save *server* card" dialog and
// *does* have card data on file, metrics
// Autofill.SaveCreditCardPromptResult.Upload.Aggregate and .UserHasSavedCards
// are recorded.
TEST_P(SaveCreditCardPromptResultMetricTest,
       EmitsSavePromptResultUploadHasSavedCards) {
  personal_data_manager()->test_payments_data_manager().ClearCreditCards();
  AddCreditCard(test::GetCreditCard());
  base::HistogramTester histogram_tester;
  ShowUploadBubble(SaveCreditCardOptions().with_show_prompt(true));
  if (closed_reason_ == PaymentsBubbleClosedReason::kAccepted) {
    controller()->OnSaveButton({});
  }
  CloseBubble(closed_reason_);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.Aggregate", prompt_result_,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.UserHasSavedCards",
      prompt_result_, 1);
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
  CardSaveType card_save_type;
};

const SaveCardOptionParam kSaveCardOptionParam[] = {
    {false, false, false, false, CardSaveType::kCardSaveOnly},
    {true, false, false, false, CardSaveType::kCardSaveOnly},
    {false, true, false, false, CardSaveType::kCardSaveOnly},
    {false, false, true, false, CardSaveType::kCardSaveOnly},
    {false, false, false, true, CardSaveType::kCardSaveOnly},
    {false, false, false, false, CardSaveType::kCardSaveWithCvc}};

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
        SaveCreditCardOptions()
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

  SaveCreditCardOptions GetSaveCreditCardOptions() {
    return save_credit_card_options_;
  }

  std::string GetHistogramNameSuffix() {
    std::string result = "." + save_destination_ + "." + show_type_;

    if (GetSaveCreditCardOptions().should_request_name_from_user) {
      result += ".RequestingCardholderName";
    }

    if (GetSaveCreditCardOptions().should_request_expiration_date_from_user) {
      result += ".RequestingExpirationDate";
    }

    if (GetSaveCreditCardOptions().has_multiple_legal_lines) {
      result += ".WithMultipleLegalLines";
    }

    if (GetSaveCreditCardOptions()
            .has_same_last_four_as_server_card_but_different_expiration_date) {
      result += ".WithSameLastFourButDifferentExpiration";
    }
    if (GetSaveCreditCardOptions().card_save_type ==
        CardSaveType::kCardSaveWithCvc) {
      result += ".SavingWithCvc";
    }

    return result;
  }

  const std::string save_destination_;
  const std::string show_type_;

 private:
  SaveCreditCardOptions save_credit_card_options_;
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
          /*options=*/SaveCreditCardOptions()
              .with_card_save_type(CardSaveType::kCvcSaveOnly)
              .with_show_prompt(show_prompt));
    } else {
      ASSERT_EQ(save_destination_, "Local");
      ShowLocalBubble(
          /*card=*/nullptr,
          /*options=*/SaveCreditCardOptions()
              .with_card_save_type(CardSaveType::kCvcSaveOnly)
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

TEST_F(SaveCardBubbleControllerImplTest, LocalCvcOnlySaveDialogContent) {
  // Show the local CVC save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/SaveCreditCardOptions()
          .with_card_save_type(CardSaveType::kCvcSaveOnly)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_CVC_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save security code?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"This card's CVC will be encrypted and saved to your device for "
            u"faster checkout");
}

TEST_F(SaveCardBubbleControllerImplTest, UploadCardSaveBubbleType) {
  ShowUploadBubble();
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_SAVE);
  EXPECT_TRUE(controller()->IsIconVisible());
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  // TODO(crbug.com/309627643): Change the bubble type when the
  // AutofillEnableSaveCardLoadingAndConfirmation feature flag is enabled.
  controller()->OnSaveButton({});
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_SAVE);

  // ShowConfirmationBubbleView() should not change the bubble type or hide
  // the bubble view if the AutofillEnableSaveCardLoadingAndConfirmation feature
  // flag is not enabled.
  ShowConfirmationBubbleView(/*card_saved=*/true);
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_SAVE);
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  CloseBubble(PaymentsBubbleClosedReason::kAccepted);
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::INACTIVE);
  EXPECT_FALSE(controller()->IsIconVisible());
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
}

TEST_F(SaveCardBubbleControllerImplTest, UploadCvcOnlySaveDialogContent) {
  // Show the server CVC save bubble.
  ShowUploadBubble(
      /*options=*/SaveCreditCardOptions()
          .with_card_save_type(CardSaveType::kCvcSaveOnly)
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
      /*options=*/SaveCreditCardOptions().with_card_save_type(
          CardSaveType::kCardSaveOnly));
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
      /*options=*/SaveCreditCardOptions().with_card_save_type(
          CardSaveType::kCvcSaveOnly));
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
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt"),
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
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt"),
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
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt"),
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
      histogram_tester.GetAllSamples("Autofill.ManageCardsPrompt"),
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
  histogram_tester.ExpectTotalCount("Autofill.ManageCardsPrompt", 0);
}

// Test that the credit card upload loading and confirmation metrics are
// recorded as false when the loading and confirmation views are not shown.
TEST_F(SaveCardBubbleControllerImplTest, Metrics_Upload_LoadingConfirmation) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});
  ShowConfirmationBubbleView(/*card_saved=*/true);
  CloseBubble();

  histogram_tester.ExpectUniqueSample("Autofill.CreditCardUpload.LoadingShown",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardUploaded", false, 1);
}

class SaveCardBubbleControllerImplTestWithCvCStorageAndFilling
    : public SaveCardBubbleControllerImplTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableCvcStorageAndFilling};
};

TEST_F(SaveCardBubbleControllerImplTestWithCvCStorageAndFilling,
       LocalCardSaveOnlyDialogContent) {
  // Show the local card save bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/SaveCreditCardOptions()
          .with_card_save_type(CardSaveType::kCardSaveOnly)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save card?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card to your device");
}

TEST_F(SaveCardBubbleControllerImplTestWithCvCStorageAndFilling,
       LocalCardSaveWithCvcDialogContent) {
  // Show the local card save with CVC bubble.
  ShowLocalBubble(
      /*card=*/nullptr,
      /*options=*/SaveCreditCardOptions()
          .with_card_save_type(CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::LOCAL_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetWindowTitle(), u"Save card?");
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card and encrypted security "
            u"code to your device");
}

TEST_F(SaveCardBubbleControllerImplTestWithCvCStorageAndFilling,
       UploadCardSaveWithCvcDialogContent) {
  // Show the server card save with CVC bubble.
  ShowUploadBubble(
      /*options=*/SaveCreditCardOptions()
          .with_card_save_type(CardSaveType::kCardSaveWithCvc)
          .with_show_prompt(true));

  ASSERT_EQ(BubbleType::UPLOAD_SAVE, controller()->GetBubbleType());
  ASSERT_NE(nullptr, controller()->GetPaymentBubbleView());
  EXPECT_EQ(controller()->GetExplanatoryMessage(),
            u"To pay faster next time, save your card, encrypted security "
            u"code, and billing address in your Google Account");
}

class SaveCardBubbleControllerImplTestWithLoadingAndConfirmation
    : public SaveCardBubbleControllerImplTest {
 public:
  SaveCardBubbleControllerImplTestWithLoadingAndConfirmation()
      : SaveCardBubbleControllerImplTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    SaveCardBubbleControllerImplTest::SetUp();

    // Set the visibility to VISIBLE as the web contents are initially hidden.
    active_web_contents()->UpdateWebContentsVisibility(
        content::Visibility::VISIBLE);
  }

 protected:
  tabs::TabHandle tab_handle() {
    return browser()->tab_strip_model()->GetTabHandleAt(
        browser()->tab_strip_model()->active_index());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillEnableSaveCardLoadingAndConfirmation};
};

// Test the entire upload save flow with the ShowConfirmationBubbleView()
// callback.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Upload_OnSave_ShowConfirmationBubbleView) {
  ShowUploadBubble();
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_SAVE);
  EXPECT_TRUE(controller()->IsIconVisible());
  EXPECT_TRUE(IsSaveCardBubbleVisible());

  controller()->OnSaveButton({});
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_IN_PROGRESS);
  EXPECT_TRUE(IsSaveCardBubbleVisible());
  EXPECT_FALSE(IsConfirmationBubbleVisible());

  ShowConfirmationBubbleView(/*card_saved=*/true);
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_COMPLETED);
  EXPECT_FALSE(IsSaveCardBubbleVisible());
  EXPECT_TRUE(IsConfirmationBubbleVisible());
  EXPECT_TRUE(controller()->GetConfirmationUiParams().is_success);

  controller()->HideSaveCardBubble();
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::INACTIVE);
  EXPECT_FALSE(IsConfirmationBubbleVisible());
  EXPECT_FALSE(controller()->IsIconVisible());
}

// Test that when passing in "card_saved=false" for ShowConfirmationBubbleView()
// the confirmation UI model has "is_success" set to false.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Upload_OnShowConfirmation_ShowFailureUIModel) {
  ShowConfirmationBubbleView(/*card_saved=*/false);
  EXPECT_FALSE(IsSaveCardBubbleVisible());
  EXPECT_TRUE(IsConfirmationBubbleVisible());
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_COMPLETED);
  EXPECT_FALSE(controller()->GetConfirmationUiParams().is_success);
}

// Test that when showing the upload bubble when the confirmation bubble view is
// still up, the confirmation bubble view is closed and the upload bubble view
// is still shown.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Upload_OnShowConfirmationBubbleView_ThenShowUploadView) {
  ShowConfirmationBubbleView(/*card_saved=*/true);
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_COMPLETED);
  EXPECT_TRUE(IsConfirmationBubbleVisible());
  EXPECT_TRUE(controller()->GetConfirmationUiParams().is_success);

  ShowUploadBubble();
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_SAVE);
  EXPECT_TRUE(IsSaveCardBubbleVisible());
  EXPECT_FALSE(IsConfirmationBubbleVisible());
  EXPECT_TRUE(controller()->IsIconVisible());
}

// Test that the `Accepted` upload result metric is recorded on upload card save
// and that upload result metrics are not recorded but the confirmation shown &
// result metrics are recorded when the save card bubble is closed after the
// save card upload completes.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_Upload_AfterSave_OnClose) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);

  ShowConfirmationBubbleView(/*card_saved=*/true);
  CloseBubble();

  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardUploaded", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationResult.CardUploaded",
      autofill_metrics::SaveCardPromptResult::kNotInteracted, 1);
  // Expect the metric not to change from the save button interaction.
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 1);
}

// Test that the `CardNotUploaded` confirmation shown & result metrics are
// recorded when the save card bubble is closed after the save card upload
// completes without the card being saved.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_Upload_AfterFailedSave_OnClose) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});
  ShowConfirmationBubbleView(/*card_saved=*/false);
  CloseBubble();

  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationShown.CardNotUploaded", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.ConfirmationResult.CardNotUploaded",
      autofill_metrics::SaveCardPromptResult::kNotInteracted, 1);
}

// Test that the `Accepted` upload result metric is not recorded and the loading
// view shown & closed metrics are recorded when the save card bubble is closed
// before the save card upload completes.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_Upload_DuringSave_OnClose) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  controller()->OnSaveButton({});

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow",
      autofill_metrics::SaveCardPromptResult::kAccepted, 1);

  CloseBubble();

  histogram_tester.ExpectUniqueSample("Autofill.CreditCardUpload.LoadingShown",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardUpload.LoadingResult",
      autofill_metrics::SaveCardPromptResult::kNotInteracted, 1);
  // Expect the upload result metric not to change from the save button
  // interaction.
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 1);
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

// Test that after changing tabs, when returning to the tab with the save card,
// the bubble view is no longer showing but can be accessed through the icon.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Upload_HideBubble) {
  base::HistogramTester histogram_tester;

  ShowUploadBubble();
  EXPECT_TRUE(IsSaveCardBubbleVisible());

  // Simulate switching to a different tab.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  EXPECT_FALSE(IsSaveCardBubbleVisible());

  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 1);

  // Simulate returning to tab where bubble was previously shown.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  EXPECT_FALSE(IsSaveCardBubbleVisible());
  EXPECT_TRUE(controller()->IsIconVisible());
}

// Test that after a link is clicked in the save card bubble view; and one
// returns to the tab with the save card, the bubble view is automatically
// re-shown without user prompt.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Upload_ReshowAfterLinkClick) {
  tabs::TabHandle tab = tab_handle();

  ShowUploadBubble();
  EXPECT_TRUE(IsSaveCardBubbleVisible());

  controller()->OnLegalMessageLinkClicked(GURL("about:blank"));
  // Change active web contents back to previous tab so that
  // active_web_contents() and controller() return the correct object.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfTab(tab));

  // Usually, the visibility changes when changing tabs but it doesn't in the
  // test so it needs to be simulated.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  EXPECT_FALSE(IsSaveCardBubbleVisible());

  // Check that the bubble is shown when returning to the tab which previously
  // showed the bubble.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);
  EXPECT_TRUE(IsSaveCardBubbleVisible());
  EXPECT_TRUE(controller()->IsIconVisible());

  // Check that the WebContents showing a subsequent time does not show the
  // bubble view.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  EXPECT_FALSE(IsSaveCardBubbleVisible());

  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  EXPECT_FALSE(IsSaveCardBubbleVisible());
  EXPECT_TRUE(controller()->IsIconVisible());
}

// Test that while in the UPLOAD_IN_PROGRESS state, after changing tabs and
// returning to the tab with the save card, the state will remain as
// UPLOAD_IN_PROGRESS.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Upload_InProgressState_Retained) {
  ShowUploadBubble();
  controller()->OnSaveButton({});
  EXPECT_TRUE(IsSaveCardBubbleVisible());
  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_IN_PROGRESS);

  // Simulate switching to a different tab and back to the original tab.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  EXPECT_FALSE(IsSaveCardBubbleVisible());
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  EXPECT_EQ(controller()->GetBubbleType(), BubbleType::UPLOAD_IN_PROGRESS);
}

// Test that while in the UPLOAD_IN_PROGRESS state, if the tab is changed and
// the upload is completed, upon returning to the original tab with the save
// card, the confirmation bubble will be showing.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       VisibilityChange_Upload_InProgressStateTransitionIntoCompletedState) {
  tabs::TabHandle tab = tab_handle();

  ShowUploadBubble();
  controller()->OnSaveButton({});
  EXPECT_TRUE(IsSaveCardBubbleVisible());

  // Need to save a reference to the controller to call while on a different tab
  // because controller() grabs the controller from active_web_contents()
  // which is based on the active tab.
  TestSaveCardBubbleControllerImpl* save_card_controller = controller();

  // Switch to a different tab.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  AddTab(browser(), GURL("about:blank"));
  EXPECT_FALSE(IsSaveCardBubbleVisible());

  // Simulate that the upload is completed.
  save_card_controller->ShowConfirmationBubbleView(
      /*card_saved=*/true,
      /*on_confirmation_closed_callback=*/std::nullopt);

  // Expect that the confirmation bubble doesn't show up on the other tab.
  EXPECT_FALSE(IsConfirmationBubbleVisible());

  // Return to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfTab(tab));
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::VISIBLE);

  // Expect that the confirmation bubble is visible.
  EXPECT_TRUE(IsConfirmationBubbleVisible());
}

// Test the metrics for reshowing the bubble view after a link is clicked.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       Metrics_VisibilityChange_Upload_ReshowAfterLinkClick) {
  base::HistogramTester histogram_tester;
  tabs::TabHandle tab = tab_handle();

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
  // TODO(crbug.com/316391673): Determine if a different metric (or the re-show
  // metric) should be tracking this re-show.
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer.Upload.FirstShow",
      autofill_metrics::SaveCardPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveCreditCardPromptOffer.Upload.Reshows",
      autofill_metrics::SaveCardPromptOffer::kShown, 0);

  // Ensure that metrics are recorded on a subsequent bubble close.
  active_web_contents()->UpdateWebContentsVisibility(
      content::Visibility::HIDDEN);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.FirstShow", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.SaveCreditCardPromptResult.Upload.Reshows", 1);
}

// Test that `HideSaveCardBubble()` hides save card offer and confirmation
// bubble.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       HideSaveCardBubble) {
  ShowUploadBubble();
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  controller()->HideSaveCardBubble();
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);

  ShowConfirmationBubbleView(/*card_saved=*/true);
  EXPECT_NE(controller()->GetPaymentBubbleView(), nullptr);

  controller()->HideSaveCardBubble();
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
}

// Test that `OnConfirmationClosedCallback` runs when confirmation prompt
// is closed by user.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       OnConfirmationPromptClosedByUser_RunCallback) {
  ShowConfirmationBubbleView(/*card_saved=*/true);
  CloseBubble();
  EXPECT_TRUE(did_on_confirmation_closed_callback_run_);
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
}

// Test that `OnConfirmationClosedCallback` runs when confirmation prompt is
// auto-closed in 3 sec.
TEST_F(SaveCardBubbleControllerImplTestWithLoadingAndConfirmation,
       OnConfirmationPromptAutoClosed_RunCallback) {
  ShowConfirmationBubbleView(/*card_saved=*/true);
  task_environment()->FastForwardBy(
      SaveCardBubbleControllerImpl::kAutoCloseConfirmationBubbleWaitSec);
  EXPECT_TRUE(did_on_confirmation_closed_callback_run_);
  EXPECT_EQ(controller()->GetPaymentBubbleView(), nullptr);
}

using UploadCardUpdatedDesktopUiTestData =
    std::tuple<UpdatedDesktopUiTreatmentArm, int, int>;

// Ensures that the AutofillUpstreamUpdatedUi feature displays the correct UI
// based on which treatment arm the user is in.
// Param of the UploadCardUpdatedDesktopUiTest:
// -- UploadCardUpdatedDesktopUiTestData: A trio of 1) which arm of the
//    experiment is active, 2) the expected bubble title text message ID for
//    that arm, and 3) the expected bubble explanatory message ID for that arm.
class UploadCardUpdatedDesktopUiTest
    : public SaveCardBubbleControllerImplTest,
      public testing::WithParamInterface<UploadCardUpdatedDesktopUiTestData> {
 public:
  UploadCardUpdatedDesktopUiTest()
      : treatment_arm_(std::get<0>(GetParam())),
        expected_title_id_(std::get<1>(GetParam())),
        expected_explanatory_message_id_(std::get<2>(GetParam())) {
    std::string treatment_arm_number;
    switch (treatment_arm_) {
      case UpdatedDesktopUiTreatmentArm::kSecurityFocus:
        treatment_arm_number = "1";
        break;
      case UpdatedDesktopUiTreatmentArm::kConvenienceFocus:
        treatment_arm_number = "2";
        break;
      case UpdatedDesktopUiTreatmentArm::kEducationFocus:
        treatment_arm_number = "3";
        break;
      case UpdatedDesktopUiTreatmentArm::kDefault:
        // For the default arm, disable the experiment flag.
        scoped_feature_list_.InitAndDisableFeature(
            features::kAutofillUpstreamUpdatedUi);
        return;
    }
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillUpstreamUpdatedUi,
        {{"autofill_upstream_updated_ui_treatment", treatment_arm_number}});
  }

  ~UploadCardUpdatedDesktopUiTest() override = default;

 protected:
  const UpdatedDesktopUiTreatmentArm treatment_arm_;
  const int expected_title_id_;
  const int expected_explanatory_message_id_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    UploadCardUpdatedDesktopUiTest,
    testing::Values(
        UploadCardUpdatedDesktopUiTestData(
            UpdatedDesktopUiTreatmentArm::kDefault,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V4,
#else
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_V3,
#endif
            IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_V3),
        UploadCardUpdatedDesktopUiTestData(
            UpdatedDesktopUiTreatmentArm::kSecurityFocus,
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_SECURITY,
            IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_SECURITY),
        UploadCardUpdatedDesktopUiTestData(
            UpdatedDesktopUiTreatmentArm::kConvenienceFocus,
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_CONVENIENCE,
            IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_CONVENIENCE),
        UploadCardUpdatedDesktopUiTestData(
            UpdatedDesktopUiTreatmentArm::kEducationFocus,
            IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD_EDUCATION,
            IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION_EDUCATION)));

TEST_P(UploadCardUpdatedDesktopUiTest, ReturnsApplicableWindowTitle) {
  ShowUploadBubble();
  EXPECT_EQ(l10n_util::GetStringUTF16(expected_title_id_),
            controller()->GetWindowTitle());
}

TEST_P(UploadCardUpdatedDesktopUiTest, ReturnsApplicableExplanatoryMessage) {
  ShowUploadBubble();
  EXPECT_EQ(l10n_util::GetStringUTF16(expected_explanatory_message_id_),
            controller()->GetExplanatoryMessage());
}

}  // namespace
}  // namespace autofill
