// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/migrator_util.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::standalone_browser::migrator_util {

class MigratorUtilTest : public testing::Test {
 public:
  MigratorUtilTest() = default;
  ~MigratorUtilTest() override = default;

  void SetUp() override { RegisterLocalStatePrefs(pref_service_.registry()); }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(MigratorUtilTest, ManipulateMigrationAttemptCount) {
  const std::string user_id_hash = "user";

  EXPECT_EQ(GetMigrationAttemptCountForUser(&pref_service_, user_id_hash), 0);
  UpdateMigrationAttemptCountForUser(&pref_service_, user_id_hash);
  EXPECT_EQ(GetMigrationAttemptCountForUser(&pref_service_, user_id_hash), 1);

  UpdateMigrationAttemptCountForUser(&pref_service_, user_id_hash);
  EXPECT_EQ(GetMigrationAttemptCountForUser(&pref_service_, user_id_hash), 2);

  ClearMigrationAttemptCountForUser(&pref_service_, user_id_hash);
  EXPECT_EQ(GetMigrationAttemptCountForUser(&pref_service_, user_id_hash), 0);
}

}  // namespace ash::standalone_browser::migrator_util
