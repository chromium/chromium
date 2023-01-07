// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/field_candidates.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

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
  field_candidates.AddFieldCandidate(COMPANY_NAME, 1.0f);
  EXPECT_EQ(COMPANY_NAME, field_candidates.BestHeuristicType());
}

// Simple case with two candidates. The one with higher score should win.
TEST(FieldCandidatesTest, TwoCandidates) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(NAME_LAST, 1.01f);
  field_candidates.AddFieldCandidate(NAME_FIRST, 0.99f);
  EXPECT_EQ(NAME_LAST, field_candidates.BestHeuristicType());
}

// Same as TwoCandidates but added in the opposite order, which should not
// interfere with the outcome.
TEST(FieldCandidatesTest, TwoCandidatesOppositeOrder) {
  FieldCandidates field_candidates;
  field_candidates.AddFieldCandidate(NAME_FIRST, 0.99f);
  field_candidates.AddFieldCandidate(NAME_LAST, 1.01f);
  EXPECT_EQ(NAME_LAST, field_candidates.BestHeuristicType());
}

}  // namespace
}  // namespace autofill
