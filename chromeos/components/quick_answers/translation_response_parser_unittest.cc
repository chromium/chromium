// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_response_parser.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

class TranslationResponseParserTest : public testing::Test {
 public:
  TranslationResponseParserTest() = default;

  TranslationResponseParserTest(const TranslationResponseParserTest&) = delete;
  TranslationResponseParserTest& operator=(
      const TranslationResponseParserTest&) = delete;

  // testing::Test:
  void SetUp() override {
    translation_response_parser_ =
        std::make_unique<TranslationResponseParser>(base::BindOnce(
            &TranslationResponseParserTest::TranslationResponseParserCallback,
            base::Unretained(this)));
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TranslationResponseParserCallback(
      std::unique_ptr<TranslationResult> translation_result) {
    translation_result_ = std::move(translation_result);
    run_loop_->Quit();
  }

  void WaitForResponse() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TranslationResponseParser> translation_response_parser_;
  std::unique_ptr<TranslationResult> translation_result_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(TranslationResponseParserTest, ProcessResponseSuccess) {
  constexpr char kTranslationResponse[] = R"(
    {
      "data": {
        "translations": [
          {
            "translatedText": "translated text"
          }
        ]
      }
    }
  )";
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>(kTranslationResponse));
  WaitForResponse();
  ASSERT_TRUE(translation_result_);
  EXPECT_EQ("translated text", translation_result_->translated_text);
}

TEST_F(TranslationResponseParserTest,
       ProcessResponseWithAmpersandCharacterCodes) {
  constexpr char kTranslationResponse[] = R"(
    {
      "data": {
        "translations": [
          {
            "translatedText": "don&#39;t mess with me"
          }
        ]
      }
    }
  )";
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>(kTranslationResponse));
  WaitForResponse();
  ASSERT_TRUE(translation_result_);
  // Should correctly unescape ampersand character codes.
  EXPECT_EQ("don't mess with me", translation_result_->translated_text);
}

TEST_F(TranslationResponseParserTest, ProcessResponseNoResults) {
  constexpr char kTranslationResponse[] = R"(
    {}
  )";
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>(kTranslationResponse));
  WaitForResponse();
  EXPECT_FALSE(translation_result_);
}

TEST_F(TranslationResponseParserTest, ProcessResponseInvalidResponse) {
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>("results {}"));
  WaitForResponse();
  EXPECT_FALSE(translation_result_);
}

}  // namespace quick_answers
