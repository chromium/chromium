// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/active_field_trials.h"

#include <stddef.h>

#include "base/strings/string_piece.h"
#include "components/variations/hashing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(ActiveFieldTrialsTest, GetFieldTrialActiveGroups) {
  typedef std::set<ActiveGroupId, ActiveGroupIdCompare> ActiveGroupIdSet;
  std::string trial_one("trial one");
  std::string group_one("group one");
  std::string trial_two("trial two");
  std::string group_two("group two");

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrial::ActiveGroup active_group;
  active_group.trial_name = trial_one;
  active_group.group_name = group_one;
  active_groups.push_back(active_group);

  active_group.trial_name = trial_two;
  active_group.group_name = group_two;
  active_groups.push_back(active_group);

  // Create our expected groups of IDs.
  ActiveGroupIdSet expected_groups;
  ActiveGroupId name_group_id;
  name_group_id.name = HashName(trial_one);
  name_group_id.group = HashName(group_one);
  expected_groups.insert(name_group_id);
  name_group_id.name = HashName(trial_two);
  name_group_id.group = HashName(group_two);
  expected_groups.insert(name_group_id);

  std::vector<ActiveGroupId> active_group_ids;
  testing::TestGetFieldTrialActiveGroupIds(base::StringPiece(), active_groups,
                                           &active_group_ids);
  EXPECT_EQ(2U, active_group_ids.size());
  for (size_t i = 0; i < active_group_ids.size(); ++i) {
    auto expected_group = expected_groups.find(active_group_ids[i]);
    EXPECT_FALSE(expected_group == expected_groups.end());
    expected_groups.erase(expected_group);
  }
  EXPECT_EQ(0U, expected_groups.size());
}

TEST(ActiveFieldTrialsTest, GetFieldTrialActiveGroupsWithSuffix) {
  std::string trial_one("trial one");
  std::string group_one("group one");
  std::string suffix("some_suffix");

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrial::ActiveGroup active_group;
  active_group.trial_name = trial_one;
  active_group.group_name = group_one;
  active_groups.push_back(active_group);

  std::vector<ActiveGroupId> active_group_ids;
  testing::TestGetFieldTrialActiveGroupIds(suffix, active_groups,
                                           &active_group_ids);
  EXPECT_EQ(1U, active_group_ids.size());

  uint32_t expected_name = HashName("trial onesome_suffix");
  uint32_t expected_group = HashName("group onesome_suffix");
  EXPECT_EQ(expected_name, active_group_ids[0].name);
  EXPECT_EQ(expected_group, active_group_ids[0].group);
}

}  // namespace variations
