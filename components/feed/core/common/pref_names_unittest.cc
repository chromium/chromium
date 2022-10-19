// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/common/pref_names.h"

#include <string>

#include "base/values.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

class PrefNamesTest : public testing::Test {
 protected:
  PrefNamesTest() { feed::RegisterProfilePrefs(prefs_.registry()); }

  TestingPrefServiceSimple prefs_;
};

TEST_F(PrefNamesTest, MigrateExperiments) {
  base::Value::Dict dict;
  dict.Set("Trial1", "Group1");
  base::Value::Dict expected;
  base::Value::List group_list;
  group_list.Append("Group1");
  expected.Set("Trial1", std::move(group_list));

  // Set the old prefs dictionary.
  prefs_.SetDict(prefs::kExperimentsDeprecated, std::move(dict));

  // Migrate the prefs.
  MigrateObsoleteProfilePrefsOct_2022(&prefs_);

  ASSERT_TRUE(prefs_.HasPrefPath(prefs::kExperimentsV2));
  EXPECT_EQ(expected, prefs_.GetDict(prefs::kExperimentsV2));
}

}  // namespace feed
