// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class OmniboxPedalTest : public testing::Test {
 protected:
  OmniboxPedalTest() {}
};

TEST_F(OmniboxPedalTest, SynonymGroupRespectsSingle) {
  {
    // Test |match_once| = true:
    // Only one instance of first found representative should be removed.
    auto group = OmniboxPedal::SynonymGroup(true, true, 2);
    group.AddSynonym({1, 2});
    group.AddSynonym({3});
    OmniboxPedal::Tokens sequence = {1, 2, 3, 1, 2};
    EXPECT_EQ(sequence.size(), size_t{5});
    const bool found = group.EraseMatchesIn(&sequence);
    EXPECT_TRUE(found);
    EXPECT_EQ(sequence.size(), size_t{3});
  }
  {
    // Test |match_once| = false:
    // All matches should be removed.
    auto group = OmniboxPedal::SynonymGroup(true, false, 2);
    group.AddSynonym({1, 2});
    group.AddSynonym({3});
    OmniboxPedal::Tokens sequence = {1, 2, 3, 5, 1, 2};
    EXPECT_EQ(sequence.size(), size_t{6});
    const bool found = group.EraseMatchesIn(&sequence);
    EXPECT_TRUE(found);
    EXPECT_EQ(sequence.size(), size_t{1});
  }
}

TEST_F(OmniboxPedalTest, SynonymGroupsDriveConceptMatches) {
  constexpr int optional = 1;
  constexpr int required_a = 2;
  constexpr int required_b = 3;
  constexpr int nonsense = 4;
  OmniboxPedal test_pedal(
      OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS),
      GURL());
  const auto add_group = [&](bool required, int token) {
    OmniboxPedal::SynonymGroup group(required, true, 1);
    group.AddSynonym({token});
    test_pedal.AddSynonymGroup(std::move(group));
  };
  add_group(false, optional);
  add_group(true, required_a);
  add_group(true, required_b);

  const auto is_concept_match = [&](const OmniboxPedal::Tokens& sequence) {
    return test_pedal.IsConceptMatch(sequence);
  };

  // As long as required synonym groups are present, order shouldn't matter.
  EXPECT_TRUE(is_concept_match({required_a, required_b}));
  EXPECT_TRUE(is_concept_match({required_b, required_a}));

  // Optional groups may be added without stopping trigger.
  EXPECT_TRUE(is_concept_match({required_a, required_b, optional}));
  EXPECT_TRUE(is_concept_match({required_a, optional, required_b}));
  EXPECT_TRUE(is_concept_match({optional, required_b, required_a}));

  // Any required group's absence will stop trigger.
  EXPECT_FALSE(is_concept_match({required_a, optional}));
  EXPECT_FALSE(is_concept_match({nonsense}));
  EXPECT_FALSE(is_concept_match({nonsense, optional}));

  // Presence of extra text will stop trigger even with all required present.
  EXPECT_FALSE(is_concept_match({required_a, required_b, nonsense, optional}));
  EXPECT_FALSE(is_concept_match({required_b, required_a, nonsense}));

  // This includes extra instances of optional groups, since it is match_once.
  EXPECT_FALSE(is_concept_match({required_b, required_a, optional, optional}));
}
