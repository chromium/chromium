// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_pedal.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

OmniboxPedal::TokenSequence make_sequence(std::vector<int> token_ids) {
  return OmniboxPedal::TokenSequence(token_ids);
}

}  // namespace

class OmniboxPedalTest : public testing::Test {
 protected:
  OmniboxPedalTest() = default;
};

TEST_F(OmniboxPedalTest, SynonymGroupRespectsSingle) {
  {
    // Test |match_once| = true:
    // Only one instance of first found representative should be removed.
    auto group = OmniboxPedal::SynonymGroup(true, true, 2);
    group.AddSynonym(make_sequence({1, 2}));
    group.AddSynonym(make_sequence({3}));
    OmniboxPedal::TokenSequence sequence({1, 2, 3, 1, 2});
    EXPECT_EQ(sequence.CountUnconsumed(), size_t{5});
    const bool found = group.EraseMatchesIn(sequence, false);
    EXPECT_TRUE(found);
    EXPECT_EQ(sequence.CountUnconsumed(), size_t{3});
  }
  {
    // Test |match_once| = false:
    // All matches should be removed.
    auto group = OmniboxPedal::SynonymGroup(true, false, 2);
    group.AddSynonym(make_sequence({1, 2}));
    group.AddSynonym(make_sequence({3}));
    OmniboxPedal::TokenSequence sequence({1, 2, 3, 5, 1, 2});
    EXPECT_EQ(sequence.CountUnconsumed(), size_t{6});
    const bool found = group.EraseMatchesIn(sequence, false);
    EXPECT_TRUE(found);
    EXPECT_EQ(sequence.CountUnconsumed(), size_t{1});
  }
}

TEST_F(OmniboxPedalTest, SynonymGroupsDriveConceptMatches) {
  constexpr int optional = 1;
  constexpr int required_a = 2;
  constexpr int required_b = 3;
  constexpr int nonsense = 4;
  scoped_refptr<OmniboxPedal> test_pedal = base::MakeRefCounted<OmniboxPedal>(
      OmniboxPedalId::CLEAR_BROWSING_DATA,
      OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
          IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
          IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
      GURL());
  const auto add_group = [&](bool required, int token) {
    OmniboxPedal::SynonymGroup group(required, true, 1);
    group.AddSynonym(make_sequence({token}));
    test_pedal->AddSynonymGroup(std::move(group));
  };
  add_group(false, optional);
  add_group(true, required_a);
  add_group(true, required_b);

  const auto is_concept_match = [&](OmniboxPedal::TokenSequence sequence) {
    return test_pedal->IsConceptMatch(sequence);
  };

  // As long as required synonym groups are present, order shouldn't matter.
  EXPECT_TRUE(is_concept_match(make_sequence({required_a, required_b})));
  EXPECT_TRUE(is_concept_match(make_sequence({required_b, required_a})));

  // Optional groups may be added without stopping trigger.
  EXPECT_TRUE(
      is_concept_match(make_sequence({required_a, required_b, optional})));
  EXPECT_TRUE(
      is_concept_match(make_sequence({required_a, optional, required_b})));
  EXPECT_TRUE(
      is_concept_match(make_sequence({optional, required_b, required_a})));

  // Any required group's absence will stop trigger.
  EXPECT_FALSE(is_concept_match(make_sequence({required_a, optional})));
  EXPECT_FALSE(is_concept_match(make_sequence({nonsense})));
  EXPECT_FALSE(is_concept_match(make_sequence({nonsense, optional})));

  // Presence of extra text will stop trigger even with all required present.
  EXPECT_FALSE(is_concept_match(
      make_sequence({required_a, required_b, nonsense, optional})));
  EXPECT_FALSE(
      is_concept_match(make_sequence({required_b, required_a, nonsense})));

  // This includes extra instances of optional groups, since it is match_once.
  EXPECT_FALSE(is_concept_match(
      make_sequence({required_b, required_a, optional, optional})));
}

TEST_F(OmniboxPedalTest, VerbatimSynonymGroupDrivesConceptMatches) {
  constexpr int required_a = 1;
  constexpr int required_b = 2;
  constexpr int nonsense = 3;
  scoped_refptr<OmniboxPedal> test_pedal = base::MakeRefCounted<OmniboxPedal>(
      OmniboxPedalId::CLEAR_BROWSING_DATA,
      OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS,
          IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUFFIX,
          IDS_ACC_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA),
      GURL());
  const auto is_concept_match = [&](OmniboxPedal::TokenSequence sequence) {
    return test_pedal->IsConceptMatch(sequence);
  };

  test_pedal->AddVerbatimSequence(make_sequence({required_a, required_b}));

  EXPECT_TRUE(is_concept_match(make_sequence({required_a, required_b})));
  EXPECT_FALSE(
      is_concept_match(make_sequence({required_a, required_b, nonsense})));
  EXPECT_FALSE(is_concept_match(make_sequence({required_b, required_a})));
}
