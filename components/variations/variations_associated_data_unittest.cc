// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_associated_data.h"

#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

constexpr VariationID TEST_VALUE_A = 3300200;
constexpr VariationID TEST_VALUE_B = 3300201;
constexpr IDCollectionKey APP = GOOGLE_APP;
constexpr std::string_view TRIAL = "trial";
constexpr std::string_view GROUP = "group";

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
    test::ClearAllVariationIDs();
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
  AssociateGoogleVariationIDForTesting(GOOGLE_APP, trial_true->trial_name(),
                                       default_name1, TEST_VALUE_A);
  AssociateGoogleVariationIDForTesting(GOOGLE_APP, trial_true->trial_name(),
                                       winner, TEST_VALUE_B);

  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(TEST_VALUE_B, GetIDForTrial(GOOGLE_APP, trial_true.get()));

  const std::string default_name2 = "default2";
  scoped_refptr<base::FieldTrial> trial_false(
      CreateFieldTrial("d2", 10, default_name2));
  const std::string loser = "ALoser";
  trial_false->AppendGroup(loser, 0);

  AssociateGoogleVariationIDForTesting(GOOGLE_APP, trial_false->trial_name(),
                                       default_name2, TEST_VALUE_A);
  AssociateGoogleVariationIDForTesting(GOOGLE_APP, trial_false->trial_name(),
                                       loser, TEST_VALUE_B);

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

// Ensure that the overwrite behavior of AssociateGoogleVariationID works as
// expected.
TEST_F(VariationsAssociatedDataTest, ForceAssociation) {
  EXPECT_EQ(EMPTY_ID, GetGoogleVariationID(APP, TRIAL, GROUP));

  AssociateGoogleVariationIDForTesting(APP, TRIAL, GROUP, TEST_VALUE_A);
  EXPECT_EQ(TEST_VALUE_A, GetGoogleVariationID(APP, TRIAL, GROUP));
  AssociateGoogleVariationIDForTesting(APP, TRIAL, GROUP, TEST_VALUE_B);
  EXPECT_EQ(TEST_VALUE_B, GetGoogleVariationID(APP, TRIAL, GROUP));
}

TEST_F(VariationsAssociatedDataTest, TimeWindow) {
  const base::Time start = base::Time::Now();
  const base::Time end = start + base::Hours(1);
  const TimeWindow time_window(start, end);

  EXPECT_TRUE(time_window.IsValid());
  EXPECT_EQ(start, time_window.start());
  EXPECT_EQ(end, time_window.end());
  EXPECT_FALSE(time_window.Contains(start - base::Seconds(1)));
  EXPECT_TRUE(time_window.Contains(start));
  EXPECT_TRUE(time_window.Contains(start + base::Seconds(1)));
  EXPECT_TRUE(time_window.Contains(end - base::Seconds(1)));
  EXPECT_TRUE(time_window.Contains(end));
  EXPECT_FALSE(time_window.Contains(end + base::Seconds(1)));
}

TEST_F(VariationsAssociatedDataTest, InvalidTimeWindow_StartEqualsEnd) {
  const base::Time start = base::Time::Now();
  const base::Time end = start;
  const TimeWindow time_window(start, end);

  EXPECT_FALSE(time_window.IsValid());
  EXPECT_EQ(start, time_window.start());
  EXPECT_EQ(end, time_window.end());
  EXPECT_FALSE(time_window.Contains(start - base::Seconds(1)));
  EXPECT_FALSE(time_window.Contains(start));
  EXPECT_FALSE(time_window.Contains(start + base::Seconds(1)));
  EXPECT_FALSE(time_window.Contains(end - base::Seconds(1)));
  EXPECT_FALSE(time_window.Contains(end));
  EXPECT_FALSE(time_window.Contains(end + base::Seconds(1)));
}

TEST_F(VariationsAssociatedDataTest, InvalidTimeWindow_StartGreaterThanEnd) {
  const base::Time start = base::Time::Now();
  const base::Time end = start - base::Minutes(1);
  const TimeWindow time_window(start, end);

  EXPECT_FALSE(time_window.IsValid());
  EXPECT_EQ(start, time_window.start());
  EXPECT_EQ(end, time_window.end());
  EXPECT_FALSE(time_window.Contains(start - base::Seconds(1)));
  EXPECT_FALSE(time_window.Contains(start));
  EXPECT_FALSE(time_window.Contains(start + base::Seconds(1)));
  EXPECT_FALSE(time_window.Contains(end - base::Seconds(1)));
  EXPECT_FALSE(time_window.Contains(end));
  EXPECT_FALSE(time_window.Contains(end + base::Seconds(1)));
}

// Ensure that timeboxing works as expected.
TEST_F(VariationsAssociatedDataTest, Timeboxing) {
  // Associate a variation id that becomes visible in 7 days, for 7 days.
  const base::Time timestamp = base::Time::Now();
  const base::Time start = timestamp + base::Days(7);
  const base::Time end = timestamp + base::Days(14);
  AssociateGoogleVariationIDForTesting(APP, TRIAL, GROUP, TEST_VALUE_A,
                                       {start, end});

  // The associated variation id is not visible before the time window starts.
  EXPECT_EQ(EMPTY_ID,
            GetGoogleVariationID(APP, TRIAL, GROUP, start - base::Days(3)));
  EXPECT_EQ(EMPTY_ID,
            GetGoogleVariationID(APP, TRIAL, GROUP, start - base::Seconds(1)));

  // The associated variation id is visible between 7 days and 14 days.
  EXPECT_EQ(TEST_VALUE_A, GetGoogleVariationID(APP, TRIAL, GROUP, start));
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(APP, TRIAL, GROUP, start + base::Seconds(1)));
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(APP, TRIAL, GROUP, start + base::Days(2)));
  EXPECT_EQ(TEST_VALUE_A,
            GetGoogleVariationID(APP, TRIAL, GROUP, end - base::Seconds(1)));
  EXPECT_EQ(TEST_VALUE_A, GetGoogleVariationID(APP, TRIAL, GROUP, end));

  // The associated variation id is not visible after 14 days.
  EXPECT_EQ(EMPTY_ID,
            GetGoogleVariationID(APP, TRIAL, GROUP, end + base::Seconds(1)));
  EXPECT_EQ(EMPTY_ID,
            GetGoogleVariationID(APP, TRIAL, GROUP, end + base::Days(15)));
}

TEST_F(VariationsAssociatedDataTest, GetNextTimeWindowEvent_Basic) {
  const base::Time timestamp = base::Time::Now();
  const base::Time start = timestamp + base::Days(7);
  const base::Time end = timestamp + base::Days(14);

  EXPECT_EQ(base::Time::Max(), GetNextTimeWindowEvent(start));
  EXPECT_EQ(base::Time::Max(), GetNextTimeWindowEvent(end));

  // Associate a variation id that becomes visible in 7 days, for 7 days.
  AssociateGoogleVariationIDForTesting(APP, TRIAL, GROUP, TEST_VALUE_A,
                                       {start, end});

  // Validate the next time window event as 'current_time' moves forward.
  EXPECT_EQ(start, GetNextTimeWindowEvent(start - base::Days(1)));
  EXPECT_EQ(start, GetNextTimeWindowEvent(start - base::Seconds(1)));
  EXPECT_EQ(end, GetNextTimeWindowEvent(start));
  EXPECT_EQ(end, GetNextTimeWindowEvent(end - base::Days(1)));
  EXPECT_EQ(end, GetNextTimeWindowEvent(end - base::Seconds(1)));
  EXPECT_EQ(base::Time::Max(), GetNextTimeWindowEvent(end));
  EXPECT_EQ(base::Time::Max(), GetNextTimeWindowEvent(end + base::Seconds(1)));
}

TEST_F(VariationsAssociatedDataTest,
       GetNextTimeWindowEvent_DisjointAndOverlapping) {
  const base::Time timestamp = base::Time::Now();

  const TimeWindow windows[] = {
      {timestamp + base::Days(3), timestamp + base::Days(6)},    // Disjoint.
      {timestamp + base::Days(7), timestamp + base::Days(14)},   // Overlapped.
      {timestamp + base::Days(9), timestamp + base::Days(11)},   // Contained.
      {timestamp + base::Days(10), timestamp + base::Days(16)},  // Partial.
  };

  // Associate a variation id for each time window.
  int i = 0;
  for (const auto& window : windows) {
    AssociateGoogleVariationIDForTesting(
        APP, base::StrCat({TRIAL, "_", base::NumberToString(i)}), GROUP,
        TEST_VALUE_A + i, window);
    ++i;
  }

  // Put all of the time window events into vector, then sort that vector. This
  // is the order that GetNextTimeWindowEvent() should return them.
  std::vector<base::Time> times;
  for (const auto& window : windows) {
    times.push_back(window.start());
    times.push_back(window.end());
  }
  std::sort(times.begin(), times.end());

  // Validate that the next time window event is always the next event in the
  // sorted event list.
  base::Time prev = base::Time::Min();
  for (const auto& time : times) {
    EXPECT_EQ(time, GetNextTimeWindowEvent(prev));
    prev = time;
  }
  EXPECT_EQ(base::Time::Max(), GetNextTimeWindowEvent(prev));
}

}  // namespace variations
