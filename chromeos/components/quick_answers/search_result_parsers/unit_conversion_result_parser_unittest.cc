// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/unit_conversion_result_parser.h"

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

class UnitConversionResultParserTest : public testing::Test {
 public:
  UnitConversionResultParserTest()
      : parser_(std::make_unique<UnitConversionResultParser>()) {}

  UnitConversionResultParserTest(const UnitConversionResultParserTest&) =
      delete;
  UnitConversionResultParserTest& operator=(
      const UnitConversionResultParserTest&) = delete;

 protected:
  std::unique_ptr<UnitConversionResultParser> parser_;
};

TEST_F(UnitConversionResultParserTest, Success) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("unitConversionResult.destination.valueAndUnit.rawText",
                       "9.84252 inch");

  QuickAnswer quick_answer;

  EXPECT_TRUE(parser_->Parse(&result, &quick_answer));
  EXPECT_EQ(ResultType::kUnitConversionResult, quick_answer.result_type);

  EXPECT_EQ(1u, quick_answer.first_answer_row.size());
  EXPECT_EQ(0u, quick_answer.title.size());
  auto* answer =
      static_cast<QuickAnswerText*>(quick_answer.first_answer_row[0].get());
  EXPECT_EQ("9.84252 inch",
            GetQuickAnswerTextForTesting(quick_answer.first_answer_row));
  EXPECT_EQ(gfx::kGoogleGrey700, answer->color);
}

TEST_F(UnitConversionResultParserTest, EmptyValue) {
  Value result(Value::Type::DICTIONARY);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(UnitConversionResultParserTest, IncorrectType) {
  Value result(Value::Type::DICTIONARY);
  result.SetIntPath("unitConversionResult.destination.valueAndUnit.rawText", 1);
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

TEST_F(UnitConversionResultParserTest, IncorrectPath) {
  Value result(Value::Type::DICTIONARY);
  result.SetStringPath("unitConversionResult.destination.valueAndUnit.text",
                       "9.84252 inch");
  QuickAnswer quick_answer;
  EXPECT_FALSE(parser_->Parse(&result, &quick_answer));
}

}  // namespace quick_answers
}  // namespace chromeos
