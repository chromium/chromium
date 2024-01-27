// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_database.h"

#include <optional>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/public/browser/storage_partition.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace media_device_salt {

using ::base::Time;
using StorageKeyMatcher =
    ::content::StoragePartition::StorageKeyMatcherFunction;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

namespace {

blink::StorageKey StorageKey1() {
  return blink::StorageKey::CreateFromStringForTesting("https://example1.com");
}

blink::StorageKey StorageKey2() {
  return blink::StorageKey::CreateFromStringForTesting("https://example2.com");
}

blink::StorageKey StorageKey3() {
  return blink::StorageKey::CreateFromStringForTesting("https://example3.com");
}

}  // namespace

// Test parameter indicates if the database should be in memory (true, for
// incognito mode) or persistent (false).
class MediaDeviceSaltDatabaseTest : public testing::TestWithParam<bool> {
 protected:
  MediaDeviceSaltDatabase& db() { return *db_; }

  bool ShouldBeInMemory() const { return GetParam(); }
  bool ShouldBePersistent() const { return !ShouldBeInMemory(); }

  base::FilePath DbPath() {
    return ShouldBeInMemory()
               ? base::FilePath()
               : temp_directory_.GetPath().AppendASCII("MediaDeviceSalts");
  }

  std::vector<std::pair<blink::StorageKey, std::string>> GetAllStoredEntries() {
    std::vector<std::pair<blink::StorageKey, std::string>> entries;
    sql::Statement statement(db().DatabaseForTesting().GetUniqueStatement(
        "SELECT storage_key, salt FROM media_device_salts"));
    while (statement.Step()) {
      std::optional<blink::StorageKey> key =
          blink::StorageKey::Deserialize(statement.ColumnString(0));
      CHECK(key.has_value());
      entries.emplace_back(*key, statement.ColumnString(1));
    }
    return entries;
  }

  StorageKeyMatcher CreateMatcherForKeys(std::vector<blink::StorageKey> keys) {
    return base::BindLambdaForTesting([keys](const blink::StorageKey& key) {
      return base::Contains(keys, key);
    });
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  void SetUp() override {
    if (ShouldBePersistent()) {
      ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    }
    db_.emplace(DbPath());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_directory_;
  std::optional<MediaDeviceSaltDatabase> db_;
};

TEST_P(MediaDeviceSaltDatabaseTest, DatabasePathExists) {
  // Issue an operation to ensure the dababase is open.
  std::ignore = db().GetOrInsertSalt(StorageKey1());
  ASSERT_EQ(base::PathExists(DbPath()), ShouldBePersistent());
}

TEST_P(MediaDeviceSaltDatabaseTest, NoStorageForOpaqueStorageKey) {
  EXPECT_FALSE(db().GetOrInsertSalt(blink::StorageKey()).has_value());
}

TEST_P(MediaDeviceSaltDatabaseTest, InsertExplicitSalt) {
  const std::string salt = CreateRandomSalt();
  EXPECT_THAT(db().GetOrInsertSalt(StorageKey1(), salt), Optional(salt));
  EXPECT_THAT(db().GetOrInsertSalt(StorageKey1()), Optional(salt));
  EXPECT_THAT(GetAllStoredEntries(),
              ElementsAre(std::pair(StorageKey1(), salt)));
}

TEST_P(MediaDeviceSaltDatabaseTest, InsertRandomSalts) {
  std::optional<std::string> salt1 = db().GetOrInsertSalt(StorageKey1());
  std::optional<std::string> salt2 = db().GetOrInsertSalt(StorageKey2());
  ASSERT_TRUE(salt1.has_value());
  ASSERT_TRUE(salt2.has_value());
  EXPECT_NE(*salt1, *salt2);
  EXPECT_THAT(GetAllStoredEntries(),
              UnorderedElementsAre(std::pair(StorageKey1(), *salt1),
                                   std::pair(StorageKey2(), *salt2)));
}

TEST_P(MediaDeviceSaltDatabaseTest, ReplaceSalt) {
  EXPECT_TRUE(db().GetOrInsertSalt(StorageKey1(), "salt1").has_value());
  EXPECT_TRUE(db().GetOrInsertSalt(StorageKey1(), "salt2").has_value());
  EXPECT_THAT(db().GetOrInsertSalt(StorageKey1()),
              Optional(std::string("salt1")));
  // There should be only one entry in the database.
  EXPECT_THAT(GetAllStoredEntries(),
              UnorderedElementsAre(std::pair(StorageKey1(), "salt1")));
}

TEST_P(MediaDeviceSaltDatabaseTest, DeleteEntriesByTime) {
  base::Time now = base::Time::Now();
  db().GetOrInsertSalt(StorageKey1(), "salt1");
  task_environment().AdvanceClock(base::Days(1000));
  db().GetOrInsertSalt(StorageKey2(), "salt2");
  task_environment().AdvanceClock(base::Days(1000));
  db().GetOrInsertSalt(StorageKey3(), "salt3");
  db().DeleteEntries(now + base::Days(500), now + base::Days(1500));
  EXPECT_THAT(GetAllStoredEntries(),
              UnorderedElementsAre(std::pair(StorageKey1(), "salt1"),
                                   std::pair(StorageKey3(), "salt3")));
}

TEST_P(MediaDeviceSaltDatabaseTest, DeleteAllEntries) {
  db().GetOrInsertSalt(StorageKey1(), "salt1");
  task_environment().AdvanceClock(base::Seconds(1000));
  db().GetOrInsertSalt(StorageKey2(), "salt2");
  task_environment().AdvanceClock(base::Days(1000));
  db().GetOrInsertSalt(StorageKey3(), "salt3");
  EXPECT_EQ(GetAllStoredEntries().size(), 3u);

  db().DeleteEntries(Time::Min(), Time::Max());
  EXPECT_TRUE(GetAllStoredEntries().empty());
}

TEST_P(MediaDeviceSaltDatabaseTest, DeleteSingleStorageKeyUsingMatcher) {
  db().GetOrInsertSalt(StorageKey1(), "salt1");
  db().GetOrInsertSalt(StorageKey2(), "salt2");
  db().GetOrInsertSalt(StorageKey3(), "salt3");
  db().DeleteEntries(base::Time(), base::Time::Max(),
                     CreateMatcherForKeys({StorageKey1()}));
  EXPECT_THAT(GetAllStoredEntries(),
              UnorderedElementsAre(std::pair(StorageKey2(), "salt2"),
                                   std::pair(StorageKey3(), "salt3")));
}

TEST_P(MediaDeviceSaltDatabaseTest, DeleteSomeStorageKeysUsingMatcher) {
  db().GetOrInsertSalt(StorageKey1(), "salt1");
  db().GetOrInsertSalt(StorageKey2(), "salt2");
  db().GetOrInsertSalt(StorageKey3(), "salt3");
  db().DeleteEntries(base::Time(), base::Time::Max(),
                     CreateMatcherForKeys({StorageKey1(), StorageKey3()}));
  EXPECT_THAT(GetAllStoredEntries(),
              ElementsAre(std::pair(StorageKey2(), "salt2")));
}

TEST_P(MediaDeviceSaltDatabaseTest, DeleteSingleStorageKey) {
  db().GetOrInsertSalt(StorageKey1(), "salt1");
  db().GetOrInsertSalt(StorageKey2(), "salt2");
  db().GetOrInsertSalt(StorageKey3(), "salt3");

  db().DeleteEntry(StorageKey1());
  EXPECT_THAT(GetAllStoredEntries(),
              UnorderedElementsAre(std::pair(StorageKey2(), "salt2"),
                                   std::pair(StorageKey3(), "salt3")));

  db().DeleteEntry(StorageKey3());
  EXPECT_THAT(GetAllStoredEntries(),
              ElementsAre(std::pair(StorageKey2(), "salt2")));
}

TEST_P(MediaDeviceSaltDatabaseTest, DatabaseErrors) {
  db().ForceErrorForTesting();
  EXPECT_FALSE(db().GetOrInsertSalt(StorageKey1()).has_value());
}

TEST_P(MediaDeviceSaltDatabaseTest, GetAllStorageKeys) {
  db().GetOrInsertSalt(StorageKey1(), "salt1");
  db().GetOrInsertSalt(StorageKey2(), "salt2");
  db().GetOrInsertSalt(StorageKey3(), "salt3");

  EXPECT_THAT(
      db().GetAllStorageKeys(),
      UnorderedElementsAre(StorageKey1(), StorageKey2(), StorageKey3()));

  db().DeleteEntry(StorageKey3());
  EXPECT_THAT(db().GetAllStorageKeys(),
              UnorderedElementsAre(StorageKey1(), StorageKey2()));

  db().DeleteEntries(base::Time::Min(), base::Time::Max());
  EXPECT_THAT(db().GetAllStorageKeys(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All, MediaDeviceSaltDatabaseTest, testing::Bool());

}  // namespace media_device_salt
