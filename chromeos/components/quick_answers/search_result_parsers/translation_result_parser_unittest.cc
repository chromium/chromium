// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/translation_result_parser.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;

}

class TranslationResultParserTest : public testing::Test {
 public:
  TranslationResultParserTest()
      : parser_(std::make_unique<TranslationResultParser>()) {}

  TranslationResultParserTest(const TranslationResultParserTest&) = delete;
  TranslationResultParserTest& operator=(const TranslationResultParserTest&) =
      delete;

 protected:
  std::unique_ptr<TranslationResultParser> parser_;
};

TEST_F(TranslationResultParserTest, Success) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("translateResult.sourceText", "oxygen");
  result.SetStringPath("translateResult.sourceTextLanguageLocalizedName",
                       "English");
  result.SetStringPath("translateResult.translatedText", "ox\\xC3\\xADgeno");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));
  EXPECT_EQ(ResultType::kTranslationResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.title.size());
  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  auto* title = static_cast<QuickAnswerText*>(quick_answer.title[0].get());
  EXPECT_EQ("oxygen · English",
            GetQuickAnswerTextForTesting(quick_answer.title));
  EXPECT_EQ(gfx::kGoogleGrey900, title->color);
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ("ox\\xC3\\xADgeno",
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(gfx::kGoogleGrey700, answer->color);
}

TEST_F(TranslationResultParserTest, MissingSourceText) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("translateResult.sourceTextLanguageLocalizedName",
                       "English");
  result.SetStringPath("translateResult.translatedText", "ox\\xC3\\xADgeno");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(TranslationResultParserTest, MissingTranslatedText) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("translateResult.sourceText", "oxygen");
  result.SetStringPath("translateResult.sourceTextLanguageLocalizedName",
                       "English");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(TranslationResultParserTest, MissingSourceTextLanguageLocalizedName) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("translateResult.sourceText", "oxygen");
  result.SetStringPath("translateResult.translatedText", "ox\\xC3\\xADgeno");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(TranslationResultParserTest, EmptyValue) {
  Value result(Value::Type::DICTIONARY);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(TranslationResultParserTest, IncorrectType) {
  Value result(Value::Type::DICTIONARY);
  result.SetIntPath("translateResult.sourceText", 1);
  result.SetStringPath("translateResult.sourceTextLanguageLocalizedName",
                       "English");
  result.SetStringPath("translateResult.translatedText", "ox\\xC3\\xADgeno");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(TranslationResultParserTest, IncorrectPath) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("translateResults.sourceText", "oxygen");
  result.SetStringPath("translateResult.sourceTextLanguageLocalizedName",
                       "English");
  result.SetStringPath("translateResult.translatedText", "ox\\xC3\\xADgeno");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
