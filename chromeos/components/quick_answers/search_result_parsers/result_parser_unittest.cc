// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/result_parser.h"

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

class ResultParserFactoryTest : public testing::Test {
 public:
  ResultParserFactoryTest()
      : parser_factory_(std::make_unique<ResultParserFactory>()) {}

  ResultParserFactoryTest(const ResultParserFactoryTest&) = delete;
  ResultParserFactoryTest& operator=(const ResultParserFactoryTest&) = delete;

 protected:
  std::unique_ptr<ResultParserFactory> parser_factory_;
};

TEST_F(ResultParserFactoryTest, SupportedType) {
  EXPECT_NE(nullptr, parser_factory_->Create(
                         static_cast<int>(ResultType::kUnitConversionResult)));
}

TEST_F(ResultParserFactoryTest, UnsupportedType) {
  // 1 is a unsupported types.
  int kUnsupportedType = 1;
  EXPECT_EQ(nullptr, parser_factory_->Create(kUnsupportedType));
}

}  // namespace quick_answers
