// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_result_loader.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/fake_quick_answers_state.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {
namespace {

constexpr char kCloudTranslationApiRequest[] =
    "https://translation.googleapis.com/language/translate/v2";
constexpr char kApiKeyName[] = "key";

constexpr char kValidResponse[] = R"(
  {
    "data": {
      "translations": [
        {
          "translatedText": "prueba"
        }
      ]
    }
  }
)";

constexpr char kTestTranslationTitle[] = "test · inglés";
constexpr char kTestTranslationResult[] = "prueba";

const auto kTestTranslationIntent =
    IntentInfo("test", IntentType::kTranslation, "es", "en");

GURL CreateTranslationRequest() {
  return net::AppendQueryParameter(GURL(kCloudTranslationApiRequest),
                                   kApiKeyName, google_apis::GetAPIKey());
}

}  // namespace

class TranslationResultLoaderTest : public testing::Test {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(const std::string& access_token)>;

  TranslationResultLoaderTest() = default;

  TranslationResultLoaderTest(const TranslationResultLoaderTest&) = delete;
  TranslationResultLoaderTest& operator=(const TranslationResultLoaderTest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    mock_delegate_ = std::make_unique<MockResultLoaderDelegate>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    loader_ = std::make_unique<TranslationResultLoader>(
        test_shared_loader_factory_, mock_delegate_.get());
  }

  void TearDown() override { loader_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TranslationResultLoader> loader_;
  std::unique_ptr<MockResultLoaderDelegate> mock_delegate_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  FakeQuickAnswersState fake_quick_answers_state_;
};

TEST_F(TranslationResultLoaderTest, Success) {
  test_url_loader_factory_.AddResponse(CreateTranslationRequest().spec(),
                                       kValidResponse);

  base::RunLoop run_loop;
  std::unique_ptr<QuickAnswersSession> session;
  ON_CALL(*mock_delegate_, OnQuickAnswerReceived)
      .WillByDefault(
          [&session, &run_loop](
              std::unique_ptr<QuickAnswersSession> quick_answers_session) {
            session = std::move(quick_answers_session);
            run_loop.Quit();
          });

  EXPECT_CALL(*mock_delegate_, OnNetworkError()).Times(0);

  fake_quick_answers_state_.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);
  loader_->Fetch(PreprocessRequest(kTestTranslationIntent));
  run_loop.Run();

  ASSERT_TRUE(session);
  ASSERT_TRUE(session->quick_answer);
  EXPECT_EQ(ResultType::kTranslationResult, session->quick_answer->result_type);
  EXPECT_EQ(
      kTestTranslationResult,
      GetQuickAnswerTextForTesting(session->quick_answer->first_answer_row));
  EXPECT_EQ(kTestTranslationTitle,
            GetQuickAnswerTextForTesting(session->quick_answer->title));

  ASSERT_TRUE(session->structured_result);
  ASSERT_TRUE(session->structured_result->translation_result);
  raw_ptr<TranslationResult> translation_result =
      session->structured_result->translation_result.get();
  EXPECT_EQ(kTestTranslationIntent.intent_text,
            translation_result->text_to_translate);
  EXPECT_EQ(kTestTranslationResult, translation_result->translated_text);
  EXPECT_EQ(kTestTranslationIntent.device_language,
            translation_result->target_locale);
  EXPECT_EQ(kTestTranslationIntent.source_language,
            translation_result->source_locale);
}

TEST_F(TranslationResultLoaderTest, NetworkError) {
  test_url_loader_factory_.AddResponse(
      CreateTranslationRequest(), network::mojom::URLResponseHead::New(),
      std::string(), network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::_)).Times(0);

  fake_quick_answers_state_.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);
  loader_->Fetch(PreprocessRequest(kTestTranslationIntent));
  base::RunLoop().RunUntilIdle();
}

TEST_F(TranslationResultLoaderTest, EmptyResponse) {
  test_url_loader_factory_.AddResponse(CreateTranslationRequest().spec(),
                                       std::string());
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::Eq(nullptr)))
      .WillOnce([&]() { run_loop.Quit(); });
  EXPECT_CALL(*mock_delegate_, OnNetworkError()).Times(0);

  fake_quick_answers_state_.AsyncSetConsentStatus(
      quick_answers::prefs::ConsentStatus::kAccepted);
  loader_->Fetch(PreprocessRequest(kTestTranslationIntent));
  run_loop.Run();
}

}  // namespace quick_answers
