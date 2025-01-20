// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"

#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::autofill::core::browser::payments::HeuristicRegexes;

namespace autofill::payments {

class AmountExtractionHeuristicRegexesTest : public testing::Test {
 protected:
  AmountExtractionHeuristicRegexesTest() = default;
  ~AmountExtractionHeuristicRegexesTest() override = default;

 protected:
  AmountExtractionHeuristicRegexes heuristic_regexes_;
};

TEST_F(AmountExtractionHeuristicRegexesTest, EmptyProto) {
  EXPECT_FALSE(heuristic_regexes_.PopulateStringFromComponent(std::string()));
}

TEST_F(AmountExtractionHeuristicRegexesTest, BadProto) {
  EXPECT_FALSE(heuristic_regexes_.PopulateStringFromComponent("rrr"));
}

TEST_F(AmountExtractionHeuristicRegexesTest, ParsingSuccessful) {
  HeuristicRegexes regexes_proto;
  regexes_proto.mutable_generic_details()->set_keyword_pattern("Total Amount");
  regexes_proto.mutable_generic_details()->set_amount_pattern("$123.00");
  ASSERT_TRUE(heuristic_regexes_.PopulateStringFromComponent(
      regexes_proto.SerializeAsString()));

  EXPECT_EQ(heuristic_regexes_.keyword_pattern(), "Total Amount");
  EXPECT_EQ(heuristic_regexes_.amount_pattern(), "$123.00");
}

}  // namespace autofill::payments
