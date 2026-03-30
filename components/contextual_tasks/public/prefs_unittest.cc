// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/prefs.h"

#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksPrefsTest : public testing::Test {
 public:
  ContextualTasksPrefsTest() {
    pref_service_.registry()->RegisterDictionaryPref(
        kContextualTasksSiteExclusions);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ContextualTasksPrefsTest, SaveAndReadSiteExclusions) {
  // Initially empty.
  const base::DictValue& initial_exclusions =
      ReadSiteExclusionsFromPrefs(&pref_service_);
  EXPECT_TRUE(initial_exclusions.empty());

  base::Time time1 = base::Time::FromMillisecondsSinceUnixEpoch(1234567890123);
  base::Time time2 = base::Time::FromMillisecondsSinceUnixEpoch(1234560000000);

  // Save some exclusions.
  base::DictValue exclusions_to_save;
  exclusions_to_save.Set(
      "example.com", static_cast<double>(time1.InMillisecondsSinceUnixEpoch()));
  exclusions_to_save.Set(
      "test.org", static_cast<double>(time2.InMillisecondsSinceUnixEpoch()));
  SaveSiteExclusionsToPrefs(&pref_service_, exclusions_to_save);

  // Read them back.
  const base::DictValue& read_exclusions =
      ReadSiteExclusionsFromPrefs(&pref_service_);

  EXPECT_EQ(2u, read_exclusions.size());

  const base::Value* time1_val = read_exclusions.Find("example.com");
  ASSERT_TRUE(time1_val);
  EXPECT_EQ(time1,
            base::Time::FromMillisecondsSinceUnixEpoch(time1_val->GetDouble()));

  const base::Value* time2_val = read_exclusions.Find("test.org");
  ASSERT_TRUE(time2_val);
  EXPECT_EQ(time2,
            base::Time::FromMillisecondsSinceUnixEpoch(time2_val->GetDouble()));
}

}  // namespace contextual_tasks
