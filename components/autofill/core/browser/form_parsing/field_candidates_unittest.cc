// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::UnorderedElementsAre;

// An empty FieldCandidates does not have any material to work with and should
// return an empty value.
TEST(FieldCandidatesTest, EmptyFieldCandidates) {
  FieldCandidates field_candidates;
  EXPECT_EQ(std::nullopt, field_candidates.BestHeuristicCandidate());
}

// A FieldCandidates with a single candidate should always return the type of
// the only candidate.
TEST(FieldCandidatesTest, SingleCandidate) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(
      COMPANY_NAME,
      MatchInfo{.matched_attribute = MatchInfo::MatchAttribute::kName},
      {/*is_name_or_high_quality_label_match=*/true,
       /*parser_type=*/HeuristicParser::kName});
  EXPECT_EQ(COMPANY_NAME, field_candidates.BestHeuristicCandidate()->type);
}

// Simple case with two candidates. The one with higher score should win.
TEST(FieldCandidatesTest, TwoCandidates) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(
      NAME_FULL,
      MatchInfo{.matched_attribute =
                    MatchInfo::MatchAttribute::kHighQualityLabel},
      {/*is_name_or_high_quality_label_match=*/true,
       /*parser_type=*/HeuristicParser::kName});
  field_candidates.AddFieldCandidate(
      MERCHANT_PROMO_CODE,
      MatchInfo{.matched_attribute = MatchInfo::MatchAttribute::kName},
      {/*is_name_or_high_quality_label_match=*/true,
       /*parser_type=*/HeuristicParser::kMerchantPromoCode});
  EXPECT_EQ(NAME_FULL, field_candidates.BestHeuristicCandidate()->type);
  EXPECT_EQ(
      MatchInfo::MatchAttribute::kHighQualityLabel,
      field_candidates.BestHeuristicCandidate()->match_info.matched_attribute);
}

// Same as TwoCandidates but added in the opposite order, which should not
// interfere with the outcome.
TEST(FieldCandidatesTest, TwoCandidatesOppositeOrder) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(
      MERCHANT_PROMO_CODE,
      MatchInfo{.matched_attribute = MatchInfo::MatchAttribute::kName},
      {/*is_name_or_high_quality_label_match=*/true,
       /*parser_type=*/HeuristicParser::kMerchantPromoCode});
  field_candidates.AddFieldCandidate(
      NAME_FULL,
      MatchInfo{.matched_attribute =
                    MatchInfo::MatchAttribute::kHighQualityLabel},
      {/*is_name_or_high_quality_label_match=*/true,
       /*parser_type=*/HeuristicParser::kName});
  EXPECT_EQ(NAME_FULL, field_candidates.BestHeuristicCandidate()->type);
  EXPECT_EQ(
      MatchInfo::MatchAttribute::kHighQualityLabel,
      field_candidates.BestHeuristicCandidate()->match_info.matched_attribute);
}

TEST(FieldCandidatesTest, BestHeuristicTypeReason) {
  FieldCandidates field_candidates;

  field_candidates.AddFieldCandidate(
      NAME_FULL,
      MatchInfo{.matched_attribute =
                    MatchInfo::MatchAttribute::kLowQualityLabel},
      {/*is_name_or_high_quality_label_match=*/false,
       /*parser_type=*/HeuristicParser::kName});
  EXPECT_THAT(field_candidates.BestHeuristicTypeReason(),
              UnorderedElementsAre(MatchAttribute::kLabel));

  field_candidates.AddFieldCandidate(
      IBAN_VALUE,
      MatchInfo{.matched_attribute =
                    MatchInfo::MatchAttribute::kLowQualityLabel},
      {/*is_name_or_high_quality_label_match=*/false,
       /*parser_type=*/HeuristicParser::kIban});
  // The best type becomes IBAN_VALUE due to a higher parser priority.
  EXPECT_THAT(field_candidates.BestHeuristicTypeReason(),
              UnorderedElementsAre(MatchAttribute::kLabel));

  field_candidates.AddFieldCandidate(
      IBAN_VALUE,
      MatchInfo{.matched_attribute = MatchInfo::MatchAttribute::kName},
      {/*is_name_or_high_quality_label_match=*/true,
       /*parser_type=*/HeuristicParser::kIban});
  // The best type remains, but the reason now includes the kName match.
  EXPECT_THAT(
      field_candidates.BestHeuristicTypeReason(),
      UnorderedElementsAre(MatchAttribute::kName, MatchAttribute::kLabel));
}

}  // namespace
}  // namespace autofill
