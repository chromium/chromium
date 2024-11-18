// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::UnorderedElementsAre;

// An empty FieldCandidates does not have any material to work with and should
// return UNKNOWN_TYPE.
TEST(FieldCandidatesTest, EmptyFieldCandidates) {
  FieldCandidates field_candidates;
  EXPECT_EQ(UNKNOWN_TYPE, field_candidates.BestHeuristicType());
}

// A FieldCandidates with a single candidate should always return the type of
// the only candidate.
TEST(FieldCandidatesTest, SingleCandidate) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(COMPANY_NAME, MatchAttribute::kName, 1.0);
  EXPECT_EQ(COMPANY_NAME, field_candidates.BestHeuristicType());
}

// Simple case with two candidates. The one with higher score should win.
TEST(FieldCandidatesTest, TwoCandidates) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(NAME_LAST, MatchAttribute::kName, 1.01);
  field_candidates.AddFieldCandidate(NAME_FIRST, MatchAttribute::kName, 0.99);
  EXPECT_EQ(NAME_LAST, field_candidates.BestHeuristicType());
}

// Same as TwoCandidates but added in the opposite order, which should not
// interfere with the outcome.
TEST(FieldCandidatesTest, TwoCandidatesOppositeOrder) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(NAME_FIRST, MatchAttribute::kName, 0.99);
  field_candidates.AddFieldCandidate(NAME_LAST, MatchAttribute::kName, 1.01);
  EXPECT_EQ(NAME_LAST, field_candidates.BestHeuristicType());
}

TEST(FieldCandidatesTest, BestHeuristicTypeReason) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(NAME_FIRST, MatchAttribute::kName, 1);
  // The best type is NAME_FIRST due to the kName match.
  EXPECT_THAT(field_candidates.BestHeuristicTypeReason(),
              UnorderedElementsAre(MatchAttribute::kName));
  field_candidates.AddFieldCandidate(NAME_LAST, MatchAttribute::kLabel, 2);
  // The best type becomes NAME_LAST due to a higher scoring kLabel match.
  EXPECT_THAT(field_candidates.BestHeuristicTypeReason(),
              UnorderedElementsAre(MatchAttribute::kLabel));
  field_candidates.AddFieldCandidate(NAME_LAST, MatchAttribute::kName, 0.5);
  // The best type remains, but the reason now includes the kName match.
  EXPECT_THAT(
      field_candidates.BestHeuristicTypeReason(),
      UnorderedElementsAre(MatchAttribute::kName, MatchAttribute::kLabel));
}

}  // namespace
}  // namespace autofill
