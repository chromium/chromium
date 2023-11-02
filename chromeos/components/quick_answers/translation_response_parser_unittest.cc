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
      std::unique_ptr<QuickAnswer> quick_answer) {
    quick_answer_ = std::move(quick_answer);
    run_loop_->Quit();
  }

  void WaitForResponse() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TranslationResponseParser> translation_response_parser_;
  std::unique_ptr<QuickAnswer> quick_answer_;
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
  constexpr char kTranslationTitle[] = "testo tradotto · Italian";
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>(kTranslationResponse), kTranslationTitle);
  WaitForResponse();
  EXPECT_TRUE(quick_answer_);
  EXPECT_EQ("translated text",
            GetQuickAnswerTextForTesting(quick_answer_->first_answer_row));
  EXPECT_EQ(kTranslationTitle,
            GetQuickAnswerTextForTesting(quick_answer_->title));
  EXPECT_EQ(ResultType::kTranslationResult, quick_answer_->result_type);
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
  constexpr char kTranslationTitle[] = "non scherzare con me · Italian";
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>(kTranslationResponse), kTranslationTitle);
  WaitForResponse();
  EXPECT_TRUE(quick_answer_);
  // Should correctly unescape ampersand character codes.
  EXPECT_EQ("don't mess with me",
            GetQuickAnswerTextForTesting(quick_answer_->first_answer_row));
  EXPECT_EQ(kTranslationTitle,
            GetQuickAnswerTextForTesting(quick_answer_->title));
  EXPECT_EQ(ResultType::kTranslationResult, quick_answer_->result_type);
}

TEST_F(TranslationResponseParserTest, ProcessResponseNoResults) {
  constexpr char kTranslationResponse[] = R"(
    {}
  )";
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>(kTranslationResponse), std::string());
  WaitForResponse();
  EXPECT_FALSE(quick_answer_);
}

TEST_F(TranslationResponseParserTest, ProcessResponseInvalidResponse) {
  translation_response_parser_->ProcessResponse(
      std::make_unique<std::string>("results {}"), std::string());
  WaitForResponse();
  EXPECT_FALSE(quick_answer_);
}

}  // namespace quick_answers
