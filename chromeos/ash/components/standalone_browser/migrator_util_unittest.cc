// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/migrator_util.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::standalone_browser::migrator_util {
namespace {
constexpr char kDataVerPref[] = "lacros.data_version";
}  // namespace

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

TEST_F(MigratorUtilTest, IsProfileMigrationCompletedForUser) {
  const std::string user_id_hash = "abcd";
  // `IsProfileMigrationCompletedForUser()` should return
  // false by default.
  EXPECT_FALSE(
      IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));

  // Calling `SetProfileMigrationCompletedForUser()` with kCopy sets profile
  // migration as completed.
  SetProfileMigrationCompletedForUser(&pref_service_, user_id_hash,
                                      MigrationMode::kCopy);
  EXPECT_EQ(GetCompletedMigrationMode(&pref_service_, user_id_hash),
            MigrationMode::kCopy);
  EXPECT_TRUE(IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));
  ClearProfileMigrationCompletedForUser(&pref_service_, user_id_hash);
  EXPECT_FALSE(
      IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));

  // Calling `SetProfileMigrationCompletedForUser()` with kMove sets profile
  // migration as completed.
  SetProfileMigrationCompletedForUser(&pref_service_, user_id_hash,
                                      MigrationMode::kMove);
  EXPECT_EQ(GetCompletedMigrationMode(&pref_service_, user_id_hash),
            MigrationMode::kMove);
  EXPECT_TRUE(IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));
  ClearProfileMigrationCompletedForUser(&pref_service_, user_id_hash);

  // Calling `SetProfileMigrationCompletedForUser()` with kSkipForNewUser sets
  // profile migration as completed.
  SetProfileMigrationCompletedForUser(&pref_service_, user_id_hash,
                                      MigrationMode::kSkipForNewUser);
  EXPECT_EQ(GetCompletedMigrationMode(&pref_service_, user_id_hash),
            MigrationMode::kSkipForNewUser);
  EXPECT_TRUE(IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));
  ClearProfileMigrationCompletedForUser(&pref_service_, user_id_hash);
  EXPECT_FALSE(
      IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));
}

TEST_F(MigratorUtilTest, GetMissingDataVer) {
  std::string user_id_hash = "1234";
  base::Version version =
      migrator_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_FALSE(version.IsValid());
}

TEST_F(MigratorUtilTest, GetCorruptDataVer) {
  base::Value::Dict dictionary_value;
  std::string user_id_hash = "1234";
  dictionary_value.Set(user_id_hash, "corrupted");
  pref_service_.Set(kDataVerPref, base::Value(std::move(dictionary_value)));
  base::Version version =
      migrator_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_FALSE(version.IsValid());
}

TEST_F(MigratorUtilTest, GetDataVer) {
  base::Value::Dict dictionary_value;
  std::string user_id_hash = "1234";
  base::Version version{"1.1.1.1"};
  dictionary_value.Set(user_id_hash, version.GetString());
  pref_service_.Set(kDataVerPref, base::Value(std::move(dictionary_value)));

  base::Version result_version =
      migrator_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_EQ(version, result_version);
}

TEST_F(MigratorUtilTest, RecordDataVer) {
  std::string user_id_hash = "1234";
  base::Version version{"1.1.1.1"};
  migrator_util::RecordDataVer(&pref_service_, user_id_hash, version);

  base::Value::Dict expected;
  expected.Set(user_id_hash, version.GetString());
  const base::Value::Dict& dict = pref_service_.GetDict(kDataVerPref);
  EXPECT_EQ(dict, expected);
}

TEST_F(MigratorUtilTest, RecordDataVerOverrides) {
  std::string user_id_hash = "1234";

  base::Version version1{"1.1.1.1"};
  base::Version version2{"1.1.1.2"};
  migrator_util::RecordDataVer(&pref_service_, user_id_hash, version1);
  migrator_util::RecordDataVer(&pref_service_, user_id_hash, version2);

  base::Value::Dict expected;
  expected.Set(user_id_hash, version2.GetString());

  const base::Value::Dict& dict = pref_service_.GetDict(kDataVerPref);
  EXPECT_EQ(dict, expected);
}

TEST_F(MigratorUtilTest, RecordDataVerWithMultipleUsers) {
  std::string user_id_hash_1 = "1234";
  std::string user_id_hash_2 = "2345";
  base::Version version1{"1.1.1.1"};
  base::Version version2{"1.1.1.2"};
  migrator_util::RecordDataVer(&pref_service_, user_id_hash_1, version1);
  migrator_util::RecordDataVer(&pref_service_, user_id_hash_2, version2);

  EXPECT_EQ(version1,
            migrator_util::GetDataVer(&pref_service_, user_id_hash_1));
  EXPECT_EQ(version2,
            migrator_util::GetDataVer(&pref_service_, user_id_hash_2));

  base::Version version3{"3.3.3.3"};
  migrator_util::RecordDataVer(&pref_service_, user_id_hash_1, version3);

  base::Value::Dict expected;
  expected.Set(user_id_hash_1, version3.GetString());
  expected.Set(user_id_hash_2, version2.GetString());

  const base::Value::Dict& dict = pref_service_.GetDict(kDataVerPref);
  EXPECT_EQ(dict, expected);
}

}  // namespace ash::standalone_browser::migrator_util
