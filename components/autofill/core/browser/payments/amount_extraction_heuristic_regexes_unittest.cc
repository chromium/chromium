// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::autofill::core::browser::payments::HeuristicRegexes;

namespace {
const char* kAmountExtractionComponentInstallationResult =
    "Autofill.AmountExtraction.HeuristicRegexesComponentInstallationResult";
}  // namespace

namespace autofill::payments {

class AmountExtractionHeuristicRegexesTest : public testing::Test {
 protected:
  AmountExtractionHeuristicRegexesTest() = default;
  ~AmountExtractionHeuristicRegexesTest() override = default;

 protected:
  AmountExtractionHeuristicRegexes heuristic_regexes_;
};

TEST_F(AmountExtractionHeuristicRegexesTest, EmptyProto) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(heuristic_regexes_.PopulateStringFromComponent(std::string()));
  histogram_tester.ExpectBucketCount(
      kAmountExtractionComponentInstallationResult,
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kEmptyGenericDetails,
      1);
}

TEST_F(AmountExtractionHeuristicRegexesTest, BadProto) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(heuristic_regexes_.PopulateStringFromComponent("rrr"));
  histogram_tester.ExpectBucketCount(
      kAmountExtractionComponentInstallationResult,
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kParsingToProtoFailed,
      1);
}

TEST_F(AmountExtractionHeuristicRegexesTest, ParsingSuccessful) {
  base::HistogramTester histogram_tester;
  HeuristicRegexes regexes_proto;
  regexes_proto.mutable_generic_details()->set_keyword_pattern("Total Amount");
  regexes_proto.mutable_generic_details()->set_amount_pattern("$123.00");
  regexes_proto.mutable_generic_details()
      ->set_number_of_ancestor_levels_to_search(4);

  ASSERT_TRUE(heuristic_regexes_.PopulateStringFromComponent(
      regexes_proto.SerializeAsString()));

  EXPECT_EQ(heuristic_regexes_.keyword_pattern(), "Total Amount");
  EXPECT_EQ(heuristic_regexes_.amount_pattern(), "$123.00");
  EXPECT_EQ(heuristic_regexes_.number_of_ancestor_levels_to_search(),
            static_cast<unsigned int>(4));
  histogram_tester.ExpectBucketCount(
      kAmountExtractionComponentInstallationResult,
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kSuccessful,
      1);
}

TEST_F(AmountExtractionHeuristicRegexesTest,
       ParsingSuccessful_WithDefaultValues) {
  heuristic_regexes_.ResetRegexStringPatternsForTesting();

  EXPECT_EQ(heuristic_regexes_.keyword_pattern(), "^(Order Total|Total):?$");
  EXPECT_EQ(heuristic_regexes_.amount_pattern(),
            R"regexp((?:\$)\s*\d{1,3}(?:[.,]\d{3})*(?:[.,]\d{2})?)regexp");
  EXPECT_EQ(heuristic_regexes_.number_of_ancestor_levels_to_search(), 6u);
}

}  // namespace autofill::payments
