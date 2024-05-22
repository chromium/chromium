// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/quick_answers_test_base.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

namespace {

class TestResultLoader : public ResultLoader {
 public:
  TestResultLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ResultLoaderDelegate* delegate)
      : ResultLoader(url_loader_factory, delegate) {}
  // ResultLoader:
  void BuildRequest(const PreprocessedOutput& preprocessed_output,
                    BuildRequestCallback callback) const override {
    return std::move(callback).Run(std::make_unique<network::ResourceRequest>(),
                                   std::string());
  }
  void ProcessResponse(const PreprocessedOutput& preprocessed_output,
                       std::unique_ptr<std::string> response_body,
                       ResponseParserCallback complete_callback) override {}
};

class MockResultLoader : public TestResultLoader {
 public:
  MockResultLoader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      ResultLoaderDelegate* delegate)
      : TestResultLoader(url_loader_factory, delegate) {
    ON_CALL(*this, Fetch)
        .WillByDefault([this](const PreprocessedOutput& preprocessed_output) {
          // `ResultLoader::Fetch` has a fail-safe CHECK for consent_status.
          // Delegate to `TestResultLoader` to trigger the check.
          TestResultLoader::Fetch(preprocessed_output);
        });
  }

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
      : IntentGenerator(/*spell_checker=*/nullptr,
                        std::move(complete_callback)) {}

  MockIntentGenerator(const MockIntentGenerator&) = delete;
  MockIntentGenerator& operator=(const MockIntentGenerator&) = delete;

  // IntentGenerator:
  MOCK_METHOD1(GenerateIntent, void(const QuickAnswersRequest&));
};

}  // namespace

class QuickAnswersClientTest : public QuickAnswersTestBase {
 public:
  QuickAnswersClientTest() = default;

  QuickAnswersClientTest(const QuickAnswersClientTest&) = delete;
  QuickAnswersClientTest& operator=(const QuickAnswersClientTest&) = delete;

  ~QuickAnswersClientTest() override = default;

  // QuickAnswersTestBase:
  void SetUp() override {
    QuickAnswersTestBase::SetUp();
    mock_delegate_ = std::make_unique<MockQuickAnswersDelegate>();

    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    client_ = std::make_unique<QuickAnswersClient>(test_shared_loader_factory_,
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
    QuickAnswersTestBase::TearDown();
  }

  void IntentGeneratorTestCallback(const IntentInfo& intent_info) {}

 protected:
  std::unique_ptr<ResultLoader> CreateResultLoader() {
    return std::move(mock_result_loader_);
  }

  std::unique_ptr<IntentGenerator> CreateIntentGenerator() {
    return std::move(mock_intent_generator_);
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<QuickAnswersClient> client_;
  std::unique_ptr<MockQuickAnswersDelegate> mock_delegate_;
  std::unique_ptr<MockResultLoader> mock_result_loader_;
  std::unique_ptr<MockIntentGenerator> mock_intent_generator_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  QuickAnswersClient::ResultLoaderFactoryCallback
      result_loader_factory_callback_;
  QuickAnswersClient::IntentGeneratorFactoryCallback
      intent_generator_factory_callback_;
  IntentGenerator::IntentGeneratorCallback intent_generator_callback_;
};

TEST_F(QuickAnswersClientTest, NetworkError) {
  // Verify that OnNetworkError is called.
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(::testing::_)).Times(0);

  client_->OnNetworkError();
}

TEST_F(QuickAnswersClientTest, SendRequest) {
  fake_quick_answers_state()->AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);

  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  // Verify that |GenerateIntent| is called.
  EXPECT_CALL(*mock_intent_generator_,
              GenerateIntent(QuickAnswersRequestEqual(*quick_answers_request)));
  QuickAnswersClient::SetIntentGeneratorFactoryForTesting(
      &intent_generator_factory_callback_);

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(test_shared_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_,
              Fetch(PreprocessedOutputEqual(PreprocessRequest(
                  IntentInfo("sel", IntentType::kDictionary)))));
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->SendRequest(*quick_answers_request);
  client_->IntentGeneratorCallback(*quick_answers_request, /*skip_fetch=*/false,
                                   IntentInfo("sel", IntentType::kDictionary));

  std::unique_ptr<QuickAnswer> quick_answer = std::make_unique<QuickAnswer>();
  quick_answer->first_answer_row.push_back(
      std::make_unique<QuickAnswerResultText>("answer"));

  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      std::make_unique<QuickAnswersSession>();
  quick_answers_session->quick_answer = std::move(quick_answer);

  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::Pointer(
                                   testing::Eq(quick_answers_session.get()))));
  client_->OnQuickAnswerReceived(std::move(quick_answers_session));
}

TEST_F(QuickAnswersClientTest, SendRequestForPreprocessing) {
  // Make the status to kUnknown as this test case is for pre-process.
  fake_quick_answers_state()->AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kUnknown);

  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "sel";

  // Verify that |GenerateIntent| is called.
  EXPECT_CALL(*mock_intent_generator_,
              GenerateIntent(QuickAnswersRequestEqual(*quick_answers_request)));
  QuickAnswersClient::SetIntentGeneratorFactoryForTesting(
      &intent_generator_factory_callback_);

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(test_shared_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_, Fetch(::testing::_)).Times(0);
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->SendRequestForPreprocessing(*quick_answers_request);
}

TEST_F(QuickAnswersClientTest, FetchQuickAnswers) {
  fake_quick_answers_state()->AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);

  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->preprocessed_output.query = "Define sel";

  mock_result_loader_ =
      std::make_unique<MockResultLoader>(test_shared_loader_factory_, nullptr);
  EXPECT_CALL(*mock_result_loader_,
              Fetch(PreprocessedOutputEqual(
                  quick_answers_request->preprocessed_output)));
  QuickAnswersClient::SetResultLoaderFactoryForTesting(
      &result_loader_factory_callback_);

  client_->FetchQuickAnswers(*quick_answers_request);
}

TEST_F(QuickAnswersClientTest, PreprocessDefinitionIntent) {
  // Make the status to kUnknown as this test case is for pre-process.
  fake_quick_answers_state()->AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kUnknown);

  std::unique_ptr<QuickAnswersRequest> quick_answers_request =
      std::make_unique<QuickAnswersRequest>();
  quick_answers_request->selected_text = "unfathomable";

  // Verify that |OnRequestPreprocessFinished| is called.
  std::unique_ptr<QuickAnswersRequest> processed_request =
      std::make_unique<QuickAnswersRequest>();
  processed_request->selected_text = "unfathomable";
  PreprocessedOutput expected_processed_output;
  expected_processed_output.intent_info.intent_text = "unfathomable";
  expected_processed_output.query = "Define unfathomable";
  expected_processed_output.intent_info.intent_type = IntentType::kDictionary;
  processed_request->preprocessed_output = expected_processed_output;
  EXPECT_CALL(*mock_delegate_,
              OnRequestPreprocessFinished(
                  QuickAnswersRequestWithOutputEqual(*processed_request)));

  // Simulate pre-process callback, i.e. skip_fetch=true.
  client_->IntentGeneratorCallback(
      *quick_answers_request, /*skip_fetch=*/true,
      IntentInfo("unfathomable", IntentType::kDictionary));
}

TEST_F(QuickAnswersClientTest, PreprocessUnitConversionIntent) {
  // Make the status to kUnknown as this test case is for pre-process.
  fake_quick_answers_state()->AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kUnknown);

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

  // Simulate pre-process callback, i.e. skip_fetch=true.
  client_->IntentGeneratorCallback(*quick_answers_request, /*skip_fetch=*/true,
                                   IntentInfo("20ft", IntentType::kUnit));
}

}  // namespace quick_answers
