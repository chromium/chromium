// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_store_migration_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/android/tab_group_sync_conversions_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

class TabGroupStoreMigrationUtilsTest : public testing::Test {
 public:
  TabGroupStoreMigrationUtilsTest() = default;
  ~TabGroupStoreMigrationUtilsTest() override = default;

  void SetUp() override {
    // Start with clean shared prefs.
    ClearSharedPrefsForTesting();
  }

  base::test::TaskEnvironment task_environment_;
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

TEST_F(TabGroupStoreMigrationUtilsTest, BasicMigrationTeset) {
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  base::Token token = base::Token::CreateRandom();

  // Initialize with one entry in the shared prefs.
  WriteMappingToSharedPrefsForTesting(uuid, token);

  // Call migration. Expect one entry in the shared prefs.
  std::map<base::Uuid, LocalTabGroupID> map =
      ReadAndClearIdMappingsForMigrationFromSharedPrefs();

  ASSERT_EQ(1u, map.size());
  EXPECT_EQ(uuid, map.begin()->first);
  EXPECT_EQ(token, map.begin()->second);

  // Call migration again. Expect the entry to be removed from the shared prefs
  // and the map to be empty.
  map = ReadAndClearIdMappingsForMigrationFromSharedPrefs();
  ASSERT_EQ(0u, map.size());
}

}  // namespace tab_groups
