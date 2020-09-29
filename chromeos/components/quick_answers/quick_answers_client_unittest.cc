// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_client.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {

namespace {

class TestResultLoader : public ResultLoader {
 public:
  TestResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                   ResultLoaderDelegate* delegate)
      : ResultLoader(url_loader_factory, delegate) {}
  // ResultLoader:
  void BuildRequest(const PreprocessedOutput& preprocessed_output,
                    BuildRequestCallback callback) const override {
    return std::move(callback).Run(
        std::make_unique<network::ResourceRequest>());
  }
  void ProcessResponse(std::unique_ptr<std::string> response_body,
                       ResponseParserCallback complete_callback) override {}
};

class MockResultLoader : public TestResultLoader {
 public:
  MockResultLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                   ResultLoaderDelegate* delegate)
      : TestResultLoader(url_loader_factory, delegate) {}

  MockResultLoader(const MockResultLoader&) = delete;
  MockResultLoader& operator=(const MockResultLoader&) = delete;

  // TestResultLoader:
  MOCK_METHOD1(Fetch, void(const PreprocessedOutput&));
};

MATCHER_P(QuickAnswersRequestWithOutputEqual, quick_answers_request, "") {
  return (
      arg.selected_text == quick_answers_request.selected_text &&
      arg.preprocessed_output.intent_info.intent_type ==
          quick_answers_request.preprocessed_output.intent_info.intent_type &&
      arg.preprocessed_output.intent_info.intent_text ==
          quick_answers_request.preprocessed_output.intent_info.intent_text &&
      arg.preprocessed_output.query ==
          quick_answers_request.preprocessed_output.query);
}

class MockIntentGenerator : public IntentGenerator {
 public:
  explicit MockIntentGenerator(IntentGeneratorCallback complete_callback)
      : IntentGenerator(std::move(complete_callback)) {}

  MockIntentGenerator(const MockIntentGenerator&) = delete;
  MockIntentGenerator& operator=(const MockIntentGenerator&) = delete;

  // IntentGenerator:
  MOCK_METHOD1(GenerateIntent, void(const QuickAnswersRequest&));
};

}  // namespace

class QuickAnswersClientTest : public testing::Test {
 public:
  QuickAnswersClientTest() = default;

  QuickAnswersClientTest(const QuickAnswersClientTest&) = delete;
  QuickAnswersClientTest& operator=(const QuickAnswersClientTest&) = delete;

  // Testing::Test:
  void SetUp() override {
    assistant_state_ = std::make_unique<ash::AssistantState>();
    mock_delegate_ = std::make_unique<MockQuickAnswersDelegate>();

    client_ = std::make_unique<QuickAnswersClient>(&test_url_loader_factory_,
                                                   assistant_state_.get(),
                                                   mock_delegate_.get());

    result_loader_factory_callback_ = base::BindRepeating(
        &QuickAnswersClientTest::CreateResultLoader, base::Unretained(this));

    intent_generator_factory_callback_ = base::BindRepeating(
        &QuickAnswersClientTest::CreateIntentGenerator, base::Unretained(this));

    mock_intent_generator_ = std::make_unique<MockIntentGenerator>(
        base::BindOnce(&QuickAnswersClientTest::IntentGeneratorTestCallback,
                       base::Unretained(this)));
  }

  void TearDown() override {
    QuickAnswersClient::SetResultLoaderFactoryForTesting(nullptr);
    QuickAnswersClient::SetIntentGeneratorFactoryForTesting(nullptr);
    client_.reset();
  }

  void IntentGeneratorTestCallback(const IntentInfo& intent_info) {}

 protected:
  void NotifyAssistantStateChange(
      bool setting_enabled,
      bool context_enabled,
      bool quick_answers_enabled,
      chromeos::assistant::AssistantAllowedState assistant_state,
      const std::string& locale) {
    client_->OnAssistantSettingsEnabled(setting_enabled);
    client_->OnAssistantContextEnabled(context_enabled);
    client_->OnAssistantQuickAnswersEnabled(quick_answers_enabled);
    client_->OnAssistantFeatureAllowedChanged(assistant_state);
    client_->OnLocaleChanged(locale);
  }

  std::unique_ptr<ResultLoader> CreateResultLoader() {
    return std::move(mock_result_loader_);
  }

  std::unique_ptr<IntentGenerator> CreateIntentGenerator() {
    return std::move(mock_intent_generator_);
  }

  std::unique_ptr<QuickAnswersClient> client_;
  std::unique_ptr<MockQuickAnswersDelegate> mock_delegate_;
  std::unique_ptr<MockResultLoader> mock_result_loader_;
  std::unique_ptr<ash::AssistantState> assistant_state_;
  std::unique_ptr<MockIntentGenerator> mock_intent_generator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  QuickAnswersClient::ResultLoaderFactoryCallback
      result_loader_factory_callback_;
  QuickAnswersClient::IntentGeneratorFactoryCallback
      intent_generator_factory_callback_;
  IntentGenerator::IntentGeneratorCallback intent_generator_callback_;
};

TEST_F(QuickAnswersClientTest, FeatureEligible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kQuickAnswers,
       chromeos::features::kQuickAnswersSubToggle},
      {});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(1);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, FeatureIneligibleAfterContextDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({chromeos::features::kQuickAnswers}, {});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true));
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false));

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/false,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantSettingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/false,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantContextDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/false,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, AssistantNotAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {chromeos::features::kQuickAnswers});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/
      chromeos::assistant::AssistantAllowedState::DISALLOWED_BY_POLICY,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, UnsupportedLocale) {
  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/true,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-GB");
}

TEST_F(QuickAnswersClientTest, SettingToggleDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {chromeos::features::kQuickAnswersSubToggle}, {});

  // Verify that OnEligibilityChanged is called.
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(false)).Times(0);
  EXPECT_CALL(*mock_delegate_, OnEligibilityChanged(true)).Times(0);

  NotifyAssistantStateChange(
      /*setting_enabled=*/true,
      /*context_enabled=*/true,
      /*quick_answers_enabled=*/false,
      /*assistant_state=*/chromeos::assistant::AssistantAllowedState::ALLOWED,
      /*locale=*/"en-US");
}

TEST_F(QuickAnswersClientTest, NetworkError) {
  // Verify that OnNetworkError is called.
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(::testing::_)).Times(0);

  client_->OnNetworkError();
}

TEST_F(QuickAnswersClientTest, SendRequest) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  // Verify that |GenerateIntent| is called.
  EXPECT_CALL(*mock_intent_generator_,
              GenerateIntent(QuickAnswersRequestEqual(*quick_answers_request)));
  QuickAnswersClient::SetIntentGeneratorFactoryForTesting(
      &intent_generator_factory_callback_);

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(&test_url_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_,
              Fetch(PreprocessedOutputEqual(PreprocessRequest(
                  IntentInfo("sel", IntentType::kDictionary)))));
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->SendRequest(*quick_answers_request);
  client_->IntentGeneratorCallback(*quick_answers_request, /*skip_fetch=*/false,
                                   IntentInfo("sel", IntentType::kDictionary));

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->primary_answer = "answer";
  EXPECT_CALL(*mock_delegate_,
              OnQuickAnswerReceived(QuickAnswerEqual(&(*quick_answer))));
  client_->OnQuickAnswerReceived(std::move(quick_answer));
}

TEST_F(QuickAnswersClientTest, SendRequestForPreprocessing) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  // Verify that |GenerateIntent| is called.
  EXPECT_CALL(*mock_intent_generator_,
              GenerateIntent(QuickAnswersRequestEqual(*quick_answers_request)));
  QuickAnswersClient::SetIntentGeneratorFactoryForTesting(
      &intent_generator_factory_callback_);

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(&test_url_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_, Fetch(::testing::_)).Times(0);
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->SendRequestForPreprocessing(*quick_answers_request);
}

TEST_F(QuickAnswersClientTest, FetchQuickAnswers) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->preprocessed_output.query = "Define:sel";

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(&test_url_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_,
              Fetch(PreprocessedOutputEqual(
                  quick_answers_request->preprocessed_output)));
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->FetchQuickAnswers(*quick_answers_request);
}

TEST_F(QuickAnswersClientTest, NotSendRequestForUnknownIntent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kQuickAnswersTextAnnotator);

  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(&test_url_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_, Fetch(::testing::_)).Times(0);
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->IntentGeneratorCallback(*quick_answers_request, /*skip_fetch=*/false,
                                   IntentInfo("sel", IntentType::kUnknown));
}

TEST_F(QuickAnswersClientTest, PreprocessDefinitionIntent) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "unfathomable";

  // Verify that |OnRequestPreprocessFinished| is called.
  std::unique_ptr<QuickAnswersRequest> processed_request =
      std::make_unique<QuickAnswersRequest>();
  processed_request->selected_text = "unfathomable";
  PreprocessedOutput expected_processed_output;
  expected_processed_output.intent_info.intent_text = "unfathomable";
  expected_processed_output.query = "Define:unfathomable";
  expected_processed_output.intent_info.intent_type = IntentType::kDictionary;
  processed_request->preprocessed_output = expected_processed_output;
  EXPECT_CALL(*mock_delegate_,
              OnRequestPreprocessFinished(
                  QuickAnswersRequestWithOutputEqual(*processed_request)));

  client_->IntentGeneratorCallback(
      *quick_answers_request, /*skip_fetch=*/false,
      IntentInfo("unfathomable", IntentType::kDictionary));
}

TEST_F(QuickAnswersClientTest, PreprocessTranslationIntent) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  // Verify that |OnRequestPreprocessFinished| is called.
  std::unique_ptr<QuickAnswersRequest> processed_request =
      std::make_unique<QuickAnswersRequest>();
  processed_request->selected_text = "sel";
  PreprocessedOutput expected_processed_output;
  expected_processed_output.intent_info.intent_text = "intent text";
  expected_processed_output.query = "Translate:intent text";
  expected_processed_output.intent_info.intent_type = IntentType::kTranslation;
  processed_request->preprocessed_output = expected_processed_output;
  EXPECT_CALL(*mock_delegate_,
              OnRequestPreprocessFinished(
                  QuickAnswersRequestWithOutputEqual(*processed_request)));

  client_->IntentGeneratorCallback(
      *quick_answers_request, /*skip_fetch=*/false,
      IntentInfo("intent text", IntentType::kTranslation));
}

TEST_F(QuickAnswersClientTest, PreprocessUnitConversionIntent) {
  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "20ft";

  // Verify that |OnRequestPreprocessFinished| is called.
  std::unique_ptr<QuickAnswersRequest> processed_request =
      std::make_unique<QuickAnswersRequest>();
  processed_request->selected_text = "20ft";
  PreprocessedOutput expected_processed_output;
  expected_processed_output.intent_info.intent_text = "20ft";
  expected_processed_output.query = "Convert:20ft";
  expected_processed_output.intent_info.intent_type = IntentType::kUnit;
  processed_request->preprocessed_output = expected_processed_output;
  EXPECT_CALL(*mock_delegate_,
              OnRequestPreprocessFinished(
                  QuickAnswersRequestWithOutputEqual(*processed_request)));

  client_->IntentGeneratorCallback(*quick_answers_request, /*skip_fetch=*/false,
                                   IntentInfo("20ft", IntentType::kUnit));
}

}  // namespace quick_answers
}  // namespace chromeos
