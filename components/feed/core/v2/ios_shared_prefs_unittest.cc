// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_prefs.h"

#include <string>
#include <vector>

#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/types.h"
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
  std::vector<std::string> group_list{"Group1"};
  e["Trial1"] = group_list;

  prefs::SetExperiments(e, prefs_);

  ASSERT_TRUE(prefs_.HasPrefPath(prefs::kExperimentsV2));
  EXPECT_EQ(e, prefs::GetExperiments(prefs_));
}

}  // namespace

}  // namespace feed
