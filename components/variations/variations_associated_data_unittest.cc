// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

const VariationID TEST_VALUE_A = 3300200;
const VariationID TEST_VALUE_B = 3300201;

// Convenience helper to retrieve the variations::VariationID for a FieldTrial.
// Note that this will do the group assignment in |trial| if not already done.
VariationID GetIDForTrial(IDCollectionKey key, base::FieldTrial* trial) {
  return GetGoogleVariationID(key, trial->trial_name(), trial->group_name());
}

// Call FieldTrialList::FactoryGetFieldTrial().
scoped_refptr<base::FieldTrial> CreateFieldTrial(
    const std::string& trial_name,
    int total_probability,
    const std::string& default_group_name) {
  base::MockEntropyProvider entropy_provider(0.9);
  return base::FieldTrialList::FactoryGetFieldTrial(
      trial_name, total_probability, default_group_name, entropy_provider);
}

}  // namespace

class VariationsAssociatedDataTest : public ::testing::Test {
 public:
  VariationsAssociatedDataTest() = default;

  VariationsAssociatedDataTest(const VariationsAssociatedDataTest&) = delete;
  VariationsAssociatedDataTest& operator=(const VariationsAssociatedDataTest&) =
      delete;

  ~VariationsAssociatedDataTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    testing::ClearAllVariationIDs();
  }
};

// Test that if the trial is immediately disabled, GetGoogleVariationID just
// returns the empty ID.
TEST_F(VariationsAssociatedDataTest, DisableImmediately) {
  scoped_refptr<base::FieldTrial> trial(
      CreateFieldTrial("trial", 100, "default"));

  for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
    ASSERT_EQ(EMPTY_ID,
              GetIDForTrial(static_cast<IDCollectionKey>(i), trial.get()));
  }
}

// Test various successful association cases.
TEST_F(VariationsAssociatedDataTest, AssociateGoogleVariationID) {
  const std::string default_name1 = "default";
  scoped_refptr<base::FieldTrial> trial_true(
      CreateFieldTrial("d1", 10, default_name1));
  const std::string winner = "TheWinner";
  trial_true->AppendGroup(winner, 10);

  // Set GoogleVariationIDs so we can verify that they were chosen correctly.
  AssociateGoogleVariationID(GOOGLE_APP, trial_true->trial_name(),
                             default_name1, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_APP, trial_true->trial_name(), winner,
                             TEST_VALUE_B);

  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(TEST_VALUE_B, GetIDForTrial(GOOGLE_APP, trial_true.get()));

  const std::string default_name2 = "default2";
  scoped_refptr<base::FieldTrial> trial_false(
      CreateFieldTrial("d2", 10, default_name2));
  const std::string loser = "ALoser";
  trial_false->AppendGroup(loser, 0);

  AssociateGoogleVariationID(GOOGLE_APP, trial_false->trial_name(),
                             default_name2, TEST_VALUE_A);
  AssociateGoogleVariationID(GOOGLE_APP, trial_false->trial_name(), loser,
                             TEST_VALUE_B);

  EXPECT_NE(loser, trial_false->group_name());
  EXPECT_EQ(TEST_VALUE_A, GetIDForTrial(GOOGLE_APP, trial_false.get()));
}

// Test that not associating a FieldTrial with any IDs ensure that the empty ID
// will be returned.
TEST_F(VariationsAssociatedDataTest, NoAssociation) {
  const std::string default_name = "default";
  scoped_refptr<base::FieldTrial> no_id_trial(
      CreateFieldTrial("d3", 10, default_name));

  const std::string winner = "TheWinner";
  no_id_trial->AppendGroup(winner, 10);

  // Ensure that despite the fact that a normal winner is elected, it does not
  // have a valid VariationID associated with it.
  EXPECT_EQ(winner, no_id_trial->group_name());
  for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
    ASSERT_EQ(EMPTY_ID, GetIDForTrial(static_cast<IDCollectionKey>(i),
                                      no_id_trial.get()));
  }
}

// Ensure that the AssociateGoogleVariationIDForce works as expected.
TEST_F(VariationsAssociatedDataTest, ForceAssociation) {
  EXPECT_EQ(EMPTY_ID, GetGoogleVariationID(GOOGLE_APP, "trial", "group"));

  AssociateGoogleVariationID(GOOGLE_APP, "trial", "group", TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A, GetGoogleVariationID(GOOGLE_APP, "trial", "group"));
  AssociateGoogleVariationID(GOOGLE_APP, "trial", "group", TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_A, GetGoogleVariationID(GOOGLE_APP, "trial", "group"));

  AssociateGoogleVariationIDForce(GOOGLE_APP, "trial", "group", TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_B, GetGoogleVariationID(GOOGLE_APP, "trial", "group"));
}

}  // namespace variations
