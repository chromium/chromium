// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_prefs.h"

#include <string>
#include <vector>

#include "components/feed/core/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

class IOSSharedPrefsTest : public testing::Test {
 protected:
  IOSSharedPrefsTest() { feed::RegisterProfilePrefs(prefs_.registry()); }

  TestingPrefServiceSimple prefs_;
};

TEST_F(IOSSharedPrefsTest, TestSetAndGetExperiments) {
  Experiments e;
  std::vector<ExperimentGroup> group_list1{{"Group1", 123}, {"Group2", 9999}};
  e["Trial1"] = group_list1;
  std::vector<ExperimentGroup> group_list2{{"Hello", 12345}};
  e["Trial2"] = group_list2;

  prefs::SetExperiments(e, prefs_);

  ASSERT_TRUE(prefs_.HasPrefPath(prefs::kExperimentsV3));
  EXPECT_EQ(e, prefs::GetExperiments(prefs_));
}

TEST_F(IOSSharedPrefsTest, MigrateExperimentsV2) {
  // Save the experiments in the old format.
  base::Value::Dict dict;
  base::Value::List list1;
  list1.Append("Group1");
  list1.Append("Group2");
  dict.Set("Trial1", std::move(list1));
  base::Value::List list2;
  list2.Append("Hello");
  dict.Set("Trial2", std::move(list2));
  prefs_.SetDict(prefs::kExperimentsV2Deprecated, std::move(dict));

  prefs::MigrateObsoleteFeedExperimentPref_Jun_2024(&prefs_);

  // Validate the migration.
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kExperimentsV2Deprecated));
  ASSERT_TRUE(prefs_.HasPrefPath(prefs::kExperimentsV3));

  Experiments e;
  std::vector<ExperimentGroup> group_list1{{"Group1", 0}, {"Group2", 0}};
  e["Trial1"] = group_list1;
  std::vector<ExperimentGroup> group_list2{{"Hello", 0}};
  e["Trial2"] = group_list2;
  EXPECT_EQ(e, prefs::GetExperiments(prefs_));
}

}  // namespace

}  // namespace feed
