// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/preferences_merge_helper.h"

#include <unordered_map>

#include "base/json/json_reader.h"
#include "components/sync_preferences/pref_model_associator_client.h"
#include "components/sync_preferences/test_syncable_prefs_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {
namespace {

const char kMergeableDictPref[] = "mergeable.dict.pref";
const char kMergeableListPref[] = "mergeable.list.pref";

const TestSyncablePrefsDatabase::PrefsMap kSyncablePrefsDatabase = {
    {kMergeableListPref,
     {/*syncable_pref_id=*/1, syncer::PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kMergeableListWithRewriteOnUpdate}},
    {kMergeableDictPref,
     {/*syncable_pref_id=*/2, syncer::PREFERENCES, PrefSensitivity::kNone,
      MergeBehavior::kMergeableDict}},
};

TEST(PreferencesMergeHelperTest, MergeListValues) {
  auto local_value =
      base::Value::List().Append("local_value").Append("common_value");
  auto server_value =
      base::Value::List().Append("server_value").Append("common_value");

  auto expected_value = base::Value::List()
                            .Append("server_value")
                            .Append("common_value")
                            .Append("local_value");
  EXPECT_EQ(helper::MergeListValues(local_value, server_value), expected_value);
}

TEST(PreferencesMergeHelperTest, MergeDictionaryValues) {
  auto local_value = base::Value::Dict()
                         .Set("local_key", "local_value")
                         .Set("common_key", "local_value");
  auto server_value = base::Value::Dict()
                          .Set("server_key", "server_value")
                          .Set("common_key", "server_value");

  auto expected_value = base::Value::Dict()
                            .Set("server_key", "server_value")
                            .Set("common_key", "server_value")
                            .Set("local_key", "local_value");
  EXPECT_EQ(helper::MergeDictionaryValues(local_value, server_value),
            expected_value);
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldAddNewValueToBothUpdates) {
  base::Value::Dict local_value;
  base::Value::Dict account_value;

  base::Value::Dict new_value =
      base::Value::Dict()
          // "new_key" is not present in either, should get added to both.
          .Set("new_key", "new_value");

  auto [updated_local_value, updated_account_value] =
      helper::UnmergeDictionaryValues(new_value.Clone(), local_value,
                                      account_value);

  // Both values should get updated.
  EXPECT_EQ(updated_local_value, new_value);
  EXPECT_EQ(updated_account_value, new_value);
}

TEST(PreferencesMergeHelperTest, UnmergeDictionaryValuesShouldRemoveValues) {
  auto local_value = base::Value::Dict()
                         .Set("local_key1", "local_value")
                         // "local_key2" is not part of new value, should get
                         // removed.
                         .Set("local_key2", "local_value")
                         // "common_key" is not part of new value, should get
                         // removed.
                         .Set("common_key", "local_value");
  auto account_value = base::Value::Dict()
                           .Set("server_key1", "server_value")
                           // "server_key2" is not part of new value, should get
                           // removed.
                           .Set("server_key2", "server_value")
                           // "common_key" is not part of new value, should get
                           // removed.
                           .Set("common_key", "server_value");

  auto [updated_local_value, updated_account_value] =
      helper::UnmergeDictionaryValues(base::Value::Dict()
                                          .Set("server_key1", "server_value")
                                          .Set("local_key1", "local_value"),
                                      local_value, account_value);

  // Entries not present in new value gets removed.
  EXPECT_EQ(updated_local_value,
            base::Value::Dict().Set("local_key1", "local_value"));
  EXPECT_EQ(updated_account_value,
            base::Value::Dict().Set("server_key1", "server_value"));
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldNotAddValuesWithNoUpdate) {
  auto local_value = base::Value::Dict()
                         .Set("local_key", "local_value")
                         .Set("common_key", "local_value");
  auto account_value = base::Value::Dict()
                           .Set("server_key", "server_value")
                           .Set("common_key", "server_value");

  auto [updated_local_value, updated_account_value] =
      helper::UnmergeDictionaryValues(base::Value::Dict()
                                          // New value same as the merged value,
                                          // so no update.
                                          .Set("server_key", "server_value")
                                          .Set("common_key", "server_value")
                                          .Set("local_key", "local_value"),
                                      local_value, account_value);

  // No change.
  EXPECT_EQ(updated_local_value, local_value);
  EXPECT_EQ(updated_account_value, account_value);
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldOnlyUpdateCommonKeyIfEffectiveValueChanges) {
  auto local_value =
      base::Value::Dict()
          // Entries are overridden by the entries in the account value.
          .Set("common_key1", "local_value1")
          .Set("common_key2", "local_value2");
  auto account_value = base::Value::Dict()
                           .Set("common_key1", "server_value1")
                           .Set("common_key2", "server_value2");

  auto [updated_local_value, updated_account_value] =
      helper::UnmergeDictionaryValues(
          base::Value::Dict()
              // "common_key1" value same as merged value, hence no update.
              .Set("common_key1", "server_value1")
              // "common_key2" value is different from merged value.
              .Set("common_key2", "local_value2"),
          local_value, account_value);

  EXPECT_EQ(updated_local_value,
            base::Value::Dict()
                // No change as the effective value (overridden by account
                // value) is unchanged.
                .Set("common_key1", "local_value1")
                .Set("common_key2", "local_value2"));
  EXPECT_EQ(updated_account_value, base::Value::Dict()
                                       .Set("common_key1", "server_value1")
                                       // Value updated.
                                       .Set("common_key2", "local_value2"));
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldAddUpdatedValuesToBothUpdates) {
  auto local_value = base::Value::Dict()
                         .Set("local_key1", "local_value")
                         .Set("local_key2", "local_value")
                         .Set("common_key", "local_value");
  auto account_value = base::Value::Dict()
                           .Set("server_key1", "server_value")
                           .Set("server_key2", "server_value")
                           .Set("common_key", "server_value");

  auto [updated_local_value, updated_account_value] =
      helper::UnmergeDictionaryValues(
          base::Value::Dict()
              // Updated, should get added to both.
              .Set("server_key1", "new_server_value")
              // Updated, should get added to both.
              .Set("common_key", "new_common_value")
              // Updated, should get added to both.
              .Set("local_key1", "new_local_value")
              // Unchanged.
              .Set("local_key2", "local_value")
              // Unchanged.
              .Set("server_key2", "server_value"),
          local_value, account_value);

  // Updated entries get added to both.
  EXPECT_EQ(updated_local_value, base::Value::Dict()
                                     .Set("local_key1", "new_local_value")
                                     .Set("local_key2", "local_value")
                                     .Set("common_key", "new_common_value")
                                     .Set("server_key1", "new_server_value"));
  EXPECT_EQ(updated_account_value, base::Value::Dict()
                                       .Set("server_key1", "new_server_value")
                                       .Set("server_key2", "server_value")
                                       .Set("common_key", "new_common_value")
                                       .Set("local_key1", "new_local_value"));
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldUnmergeRecursively) {
  const char* local_dict_json = R"(
{
  "common_key" : {
    "local_key" : "local_value"
  }
}
  )";
  std::optional<base::Value> local_value =
      base::JSONReader::Read(local_dict_json);
  ASSERT_TRUE(local_value.has_value() && local_value->is_dict());

  const char* account_dict_json = R"(
{
  "common_key" : {
    "server_key1" : "server_value1",
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> account_value =
      base::JSONReader::Read(account_dict_json);
  ASSERT_TRUE(account_value.has_value() && account_value->is_dict());

  // Changes:
  // - "new_key" was added, should get added to both - local and account values.
  // - "server_key1" value was updated, should get added to both.
  const char* new_dict_json = R"(
{
  "common_key" : {
    "new_key"     : "new_value",
    "server_key1" : "new_server_value1",
    "server_key2" : "server_value2",
    "local_key"   : "local_value"
  }
}
  )";
  std::optional<base::Value> new_value = base::JSONReader::Read(new_dict_json);
  ASSERT_TRUE(new_value.has_value() && new_value->is_dict());

  // "local_key" is unchanged, "new_key" was added and "server_key1" was
  // updated, so should've been added to the local value.
  // "server_key2" is unchanged and should not be added to the local value.
  const char* expected_local_dict_json = R"(
{
  "common_key" : {
    "local_key"   : "local_value",
    "new_key"     : "new_value",
    "server_key1" : "new_server_value1"
  }
}
  )";
  std::optional<base::Value> expected_local_value =
      base::JSONReader::Read(expected_local_dict_json);
  ASSERT_TRUE(expected_local_value.has_value() &&
              expected_local_value->is_dict());

  // "new_key" was added, "server_key1" was updated and "server_key2" is
  // unchanged, so should be part of the account value.
  // "local_key" is unchanged and thus, shouldn't be added to the account value.
  const char* expected_account_dict_json = R"(
{
  "common_key" : {
    "new_key"     : "new_value",
    "server_key1" : "new_server_value1",
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> expected_account_value =
      base::JSONReader::Read(expected_account_dict_json);
  ASSERT_TRUE(expected_account_value.has_value() &&
              expected_account_value->is_dict());

  auto [new_local_value, new_account_value] = helper::UnmergeDictionaryValues(
      std::move(*new_value).TakeDict(), local_value->GetDict(),
      account_value->GetDict());

  EXPECT_EQ(new_local_value, expected_local_value->GetDict());
  EXPECT_EQ(new_account_value, expected_account_value->GetDict());
}

TEST(
    PreferencesMergeHelperTest,
    UnmergeDictionaryValuesShouldUnmergeRecursivelyButShouldNotAddUnchangedValuesToOther) {
  const char* local_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "local_value1"
  },
  "common_key" : {
    "local_key2" : "local_value2"
  }
}
  )";
  std::optional<base::Value> local_value =
      base::JSONReader::Read(local_dict_json);
  ASSERT_TRUE(local_value.has_value() && local_value->is_dict());

  const char* account_dict_json = R"(
{
  "server_key" : {
    "server_key1" : "server_value1"
  },
  "common_key" : {
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> account_value =
      base::JSONReader::Read(account_dict_json);
  ASSERT_TRUE(account_value.has_value() && account_value->is_dict());

  // Unchanged, this is the same as the merged value. Hence, both the local
  // value and the account value should remain unchanged.
  const char* new_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "local_value1"
  },
  "server_key" : {
    "server_key1" : "server_value1"
  },
  "common_key" : {
    "local_key2" : "local_value2",
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> new_value = base::JSONReader::Read(new_dict_json);
  ASSERT_TRUE(new_value.has_value() && new_value->is_dict());
  // The new value is the same as the merged value.
  ASSERT_EQ(new_value->GetDict(),
            helper::MergeDictionaryValues(local_value->GetDict(),
                                          account_value->GetDict()));

  auto [new_local_value, new_account_value] = helper::UnmergeDictionaryValues(
      std::move(*new_value).TakeDict(), local_value->GetDict(),
      account_value->GetDict());

  EXPECT_EQ(new_local_value, local_value->GetDict());
  EXPECT_EQ(new_account_value, account_value->GetDict());
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldUnmergeRecursivelyAndAddUpdatedValuesToBoth) {
  const char* local_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "local_value1"
  }
}
  )";
  std::optional<base::Value> local_value =
      base::JSONReader::Read(local_dict_json);
  ASSERT_TRUE(local_value.has_value() && local_value->is_dict());

  const char* account_dict_json = R"(
{
  "server_key" : {
    "server_key1" : "server_value1"
  }
}
  )";
  std::optional<base::Value> account_value =
      base::JSONReader::Read(account_dict_json);
  ASSERT_TRUE(account_value.has_value() && account_value->is_dict());

  // Values for "local_key1" and "server_key1" were updated. They should get
  // added to both the values.
  const char* new_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "new_local_value1"
  },
  "server_key" : {
    "server_key1" : "new_server_value1"
  }
}
  )";
  std::optional<base::Value> new_value = base::JSONReader::Read(new_dict_json);
  ASSERT_TRUE(new_value.has_value() && new_value->is_dict());

  auto [new_local_value, new_account_value] = helper::UnmergeDictionaryValues(
      new_value->GetDict().Clone(), local_value->GetDict(),
      account_value->GetDict());

  EXPECT_EQ(new_local_value, new_value->GetDict());
  EXPECT_EQ(new_account_value, new_value->GetDict());
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldUnmergeRecursivelyAndAddNewKeysToBoth) {
  const char* local_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "local_value1"
  }
}
  )";
  std::optional<base::Value> local_value =
      base::JSONReader::Read(local_dict_json);
  ASSERT_TRUE(local_value.has_value() && local_value->is_dict());

  const char* account_dict_json = R"(
{
  "server_key" : {
    "server_key1" : "server_value1"
  }
}
  )";
  std::optional<base::Value> account_value =
      base::JSONReader::Read(account_dict_json);
  ASSERT_TRUE(account_value.has_value() && account_value->is_dict());

  // "local_key2" and "server_key2" are newly-added keys. They should get added
  // to both the local and the account values. "local_key1" and
  // "server_key1" are unchanged.
  const char* new_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "local_value1",
    "local_key2" : "local_value2"
  },
  "server_key" : {
    "server_key1" : "server_value1",
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> new_value = base::JSONReader::Read(new_dict_json);
  ASSERT_TRUE(new_value.has_value() && new_value->is_dict());

  // "local_key2" and "server_key2" were added. Since, "server_key1" was
  // unchanged, it was not added to the local value.
  const char* expected_local_dict_json = R"(
{
  "local_key" : {
    "local_key1" : "local_value1",
    "local_key2" : "local_value2"
  },
  "server_key" : {
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> expected_local_value =
      base::JSONReader::Read(expected_local_dict_json);
  ASSERT_TRUE(expected_local_value.has_value() &&
              expected_local_value->is_dict());

  // "local_key2" and "server_key2" were added. Since, "local_key1" was
  // unchanged, it was not added to the account/server value.
  const char* expected_account_dict_json = R"(
{
  "local_key" : {
    "local_key2" : "local_value2"
  },
  "server_key" : {
    "server_key1" : "server_value1",
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> expected_account_value =
      base::JSONReader::Read(expected_account_dict_json);
  ASSERT_TRUE(expected_account_value.has_value() &&
              expected_account_value->is_dict());

  auto [new_local_value, new_account_value] = helper::UnmergeDictionaryValues(
      std::move(*new_value).TakeDict(), local_value->GetDict(),
      account_value->GetDict());

  EXPECT_EQ(new_local_value, expected_local_value->GetDict());
  EXPECT_EQ(new_account_value, expected_account_value->GetDict());
}

TEST(PreferencesMergeHelperTest,
     UnmergeDictionaryValuesShouldUnmergeRecursivelyAndRemoveMissingKeys) {
  const char* local_dict_json = R"(
{
  "common_key1" : {
    "local_key1" : "local_value1"
  },
  "common_key2" : {
    "local_key2" : "local_value2"
  }
}
  )";
  std::optional<base::Value> local_value =
      base::JSONReader::Read(local_dict_json);
  ASSERT_TRUE(local_value.has_value() && local_value->is_dict());

  const char* account_dict_json = R"(
{
  "common_key1" : {
    "server_key1" : "server_value1"
  },
  "common_key2" : {
    "server_key2" : "server_value2"
  }
}
  )";
  std::optional<base::Value> account_value =
      base::JSONReader::Read(account_dict_json);
  ASSERT_TRUE(account_value.has_value() && account_value->is_dict());

  // "local_key1" and "server_key2" were removed.
  const char* new_dict_json = R"(
{
  "common_key1" : {
    "server_key1" : "server_value1"
  },
  "common_key2" : {
    "local_key2" : "local_value2"
  }
}
  )";
  std::optional<base::Value> new_value = base::JSONReader::Read(new_dict_json);
  ASSERT_TRUE(new_value.has_value() && new_value->is_dict());

  // "local_key1" and "server_key2" were removed. So, "local_key1" got removed
  // while nothing new was added/updated.
  const char* expected_local_dict_json = R"(
{
  "common_key2" : {
    "local_key2" : "local_value2"
  }
}
  )";
  std::optional<base::Value> expected_local_value =
      base::JSONReader::Read(expected_local_dict_json);
  ASSERT_TRUE(expected_local_value.has_value() &&
              expected_local_value->is_dict());

  // "local_key1" and "server_key2" were removed. So, "server_key2" got removed
  // while nothing new was added/updated.
  const char* expected_account_dict_json = R"(
{
  "common_key1" : {
    "server_key1" : "server_value1"
  }
}
  )";
  std::optional<base::Value> expected_account_value =
      base::JSONReader::Read(expected_account_dict_json);
  ASSERT_TRUE(expected_account_value.has_value() &&
              expected_account_value->is_dict());

  auto [new_local_value, new_account_value] = helper::UnmergeDictionaryValues(
      std::move(*new_value).TakeDict(), local_value->GetDict(),
      account_value->GetDict());

  EXPECT_EQ(new_local_value, expected_local_value->GetDict());
  EXPECT_EQ(new_account_value, expected_account_value->GetDict());
}

class TestPrefModelAssociatorClient : public PrefModelAssociatorClient {
 public:
  TestPrefModelAssociatorClient()
      : syncable_prefs_database_(kSyncablePrefsDatabase) {}

  TestPrefModelAssociatorClient(const TestPrefModelAssociatorClient&) = delete;
  TestPrefModelAssociatorClient& operator=(
      const TestPrefModelAssociatorClient&) = delete;

  // PrefModelAssociatorClient implementation.
  base::Value MaybeMergePreferenceValues(
      std::string_view pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override {
    return base::Value();
  }
  const SyncablePrefsDatabase& GetSyncablePrefsDatabase() const override {
    return syncable_prefs_database_;
  }

 private:
  ~TestPrefModelAssociatorClient() override = default;

  TestSyncablePrefsDatabase syncable_prefs_database_;
};

TEST(PreferencesMergeHelperTest,
     ShouldHandleCorruptLocalValueForMergeableDictPref) {
  auto client = base::MakeRefCounted<TestPrefModelAssociatorClient>();

  base::Value corrupt_local_value("corrupt value");
  base::Value account_value(
      base::Value::Dict().Set("account_key", "account value"));

  base::Value merged_value = helper::MergePreference(
      client.get(), kMergeableDictPref, corrupt_local_value, account_value);
  // Since local value is corrupt and account value is not, account value wins.
  EXPECT_EQ(merged_value, account_value);
}

TEST(PreferencesMergeHelperTest,
     ShouldHandleCorruptServerValueForMergeableDictPref) {
  auto client = base::MakeRefCounted<TestPrefModelAssociatorClient>();

  base::Value local_value(base::Value::Dict().Set("local_key", "local value"));
  base::Value corrupt_account_value("corrupt value");

  base::Value merged_value = helper::MergePreference(
      client.get(), kMergeableDictPref, local_value, corrupt_account_value);
  // Since account value is corrupt but local value is not, local value wins.
  EXPECT_EQ(merged_value, local_value);
}

TEST(PreferencesMergeHelperTest,
     ShouldHandleCorruptValuesForMergeableDictPref) {
  auto client = base::MakeRefCounted<TestPrefModelAssociatorClient>();

  base::Value corrupt_local_value("corrupt value");
  base::Value corrupt_account_value(
      base::Value::List().Append("account value"));

  base::Value merged_value =
      helper::MergePreference(client.get(), kMergeableDictPref,
                              corrupt_local_value, corrupt_account_value);
  // Since both values are corrupt, local value wins to avoid updating pref at
  // all.
  EXPECT_EQ(merged_value, corrupt_local_value);
}

TEST(PreferencesMergeHelperTest,
     ShouldHandleCorruptLocalValueForMergeableListPref) {
  auto client = base::MakeRefCounted<TestPrefModelAssociatorClient>();

  base::Value corrupt_local_value("corrupt value");
  base::Value account_value(base::Value::List().Append("account value"));

  base::Value merged_value = helper::MergePreference(
      client.get(), kMergeableListPref, corrupt_local_value, account_value);
  // Since local value is corrupt and account value is not, account value wins.
  EXPECT_EQ(merged_value, account_value);
}

TEST(PreferencesMergeHelperTest,
     ShouldHandleCorruptServerValueForMergeableListPref) {
  auto client = base::MakeRefCounted<TestPrefModelAssociatorClient>();

  base::Value local_value(base::Value::List().Append("local value"));
  base::Value corrupt_account_value("corrupt value");

  base::Value merged_value = helper::MergePreference(
      client.get(), kMergeableListPref, local_value, corrupt_account_value);
  // Since account value is corrupt but local value is not, local value wins.
  EXPECT_EQ(merged_value, local_value);
}

TEST(PreferencesMergeHelperTest,
     ShouldHandleCorruptValuesForMergeableListPref) {
  auto client = base::MakeRefCounted<TestPrefModelAssociatorClient>();

  base::Value corrupt_local_value("corrupt value");
  base::Value corrupt_account_value(
      base::Value::Dict().Set("account_key", "account value"));

  base::Value merged_value =
      helper::MergePreference(client.get(), kMergeableListPref,
                              corrupt_local_value, corrupt_account_value);
  // Since both values are corrupt, local value wins to avoid updating pref at
  // all.
  EXPECT_EQ(merged_value, corrupt_local_value);
}

// Tests for MergePreference() exists in pref_model_associator_unittest.cc.
// TODO(crbug.com/40256874): Move those tests here.

}  // namespace
}  // namespace sync_preferences
