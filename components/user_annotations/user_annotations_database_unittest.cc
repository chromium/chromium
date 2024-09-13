// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/user_annotations/user_annotations_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_annotations {

using ::base::test::EqualsProto;
using ::optimization_guide::proto::UserAnnotationsEntry;
using ::testing::UnorderedElementsAre;

UserAnnotationsEntry CreateUserAnnotationsEntry(int id,
                                                const std::string& key,
                                                const std::string& value) {
  UserAnnotationsEntry entry;
  entry.set_entry_id(id);
  entry.set_key(key);
  entry.set_value(value);
  return entry;
}

class UserAnnotationsDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    os_crypt_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    CreateDatabase();
  }

  void TearDown() override {
    database_.reset();
    CHECK(temp_dir_.Delete());
  }

  void CreateDatabase() {
    base::RunLoop run_loop;
    on_database_created_closure_ = run_loop.QuitClosure();
    encryptor_ready_subscription_ = os_crypt_->GetInstance(
        base::BindOnce(&UserAnnotationsDatabaseTest::CreateDatabaseOnCryptReady,
                       base::Unretained(this)));
    run_loop.Run();
  }

 protected:
  void CreateDatabaseOnCryptReady(os_crypt_async::Encryptor encryptor,
                                  bool success) {
    ASSERT_TRUE(success);
    database_ = std::make_unique<UserAnnotationsDatabase>(temp_dir_.GetPath(),
                                                          std::move(encryptor));
    std::move(on_database_created_closure_).Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  base::CallbackListSubscription encryptor_ready_subscription_;
  std::unique_ptr<UserAnnotationsDatabase> database_;
  base::OnceClosure on_database_created_closure_;
};

TEST_F(UserAnnotationsDatabaseTest, StoreAndRetrieve) {
  EXPECT_TRUE(database_->RetrieveAllEntries()->empty());

  std::vector<UserAnnotationsEntry> entries;
  entries.push_back(CreateUserAnnotationsEntry(1, "foo", "foo_value"));
  entries.push_back(CreateUserAnnotationsEntry(2, "bar", "bar_value"));

  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries(entries));
  EXPECT_THAT(
      *database_->RetrieveAllEntries(),
      UnorderedElementsAre(EqualsProto(entries[0]), EqualsProto(entries[1])));

  // Reopen the database, and it should have the entries.
  database_.reset();
  CreateDatabase();
  EXPECT_THAT(
      *database_->RetrieveAllEntries(),
      UnorderedElementsAre(EqualsProto(entries[0]), EqualsProto(entries[1])));
}

TEST_F(UserAnnotationsDatabaseTest, RemoveEntry) {
  std::vector<UserAnnotationsEntry> entries;
  entries.push_back(CreateUserAnnotationsEntry(1, "foo", "foo_value"));
  entries.push_back(CreateUserAnnotationsEntry(2, "bar", "bar_value"));
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries(entries));

  auto db_entries = *database_->RetrieveAllEntries();
  EXPECT_EQ(2U, db_entries.size());
  EXPECT_TRUE(database_->RemoveEntry(db_entries[0].entry_id()));
  EXPECT_TRUE(database_->RemoveEntry(db_entries[1].entry_id()));
  EXPECT_TRUE(database_->RetrieveAllEntries()->empty());
}

TEST_F(UserAnnotationsDatabaseTest, RemoveAllEntries) {
  std::vector<UserAnnotationsEntry> entries;
  entries.push_back(CreateUserAnnotationsEntry(1, "foo", "foo_value"));
  entries.push_back(CreateUserAnnotationsEntry(2, "bar", "bar_value"));
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries(entries));
  EXPECT_TRUE(database_->RemoveAllEntries());
  EXPECT_TRUE(database_->RemoveAllEntries());
  EXPECT_TRUE(database_->RetrieveAllEntries()->empty());
}

TEST_F(UserAnnotationsDatabaseTest, RemoveAllAnnotationsInRange) {
  auto foo_entry = CreateUserAnnotationsEntry(1, "foo", "foo_value");
  auto bar_entry = CreateUserAnnotationsEntry(2, "bar", "bar_value");
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries({foo_entry}));
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries({bar_entry}));
  EXPECT_EQ(2u, database_->RetrieveAllEntries()->size());

  // Delete all.
  database_->RemoveAnnotationsInRange(base::Time::Min(), base::Time::Max());
  EXPECT_TRUE(database_->RetrieveAllEntries()->empty());
}

TEST_F(UserAnnotationsDatabaseTest, RemoveAnnotationsInRange) {
  auto foo_entry = CreateUserAnnotationsEntry(1, "foo", "foo_value");
  auto bar_entry = CreateUserAnnotationsEntry(2, "bar", "bar_value");
  auto foo_create_time = base::Time::Now();
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries({foo_entry}));
  task_environment_.FastForwardBy(base::Hours(1));
  auto bar_create_time = base::Time::Now();
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries({bar_entry}));
  EXPECT_EQ(2u, database_->RetrieveAllEntries()->size());

  // Delete foo.
  database_->RemoveAnnotationsInRange(foo_create_time - base::Seconds(1),
                                      foo_create_time + base::Seconds(1));
  EXPECT_THAT(*database_->RetrieveAllEntries(),
              UnorderedElementsAre(EqualsProto(bar_entry)));

  // Delete bar.
  database_->RemoveAnnotationsInRange(bar_create_time - base::Seconds(1),
                                      bar_create_time + base::Seconds(1));
  EXPECT_TRUE(database_->RetrieveAllEntries()->empty());
}

TEST_F(UserAnnotationsDatabaseTest, RemoveAnnotationsInRangeBackward) {
  auto foo_entry = CreateUserAnnotationsEntry(1, "foo", "foo_value");
  auto bar_entry = CreateUserAnnotationsEntry(2, "bar", "bar_value");
  auto foo_create_time = base::Time::Now();
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries({foo_entry}));
  task_environment_.FastForwardBy(base::Hours(1));
  auto bar_create_time = base::Time::Now();
  EXPECT_EQ(UserAnnotationsExecutionResult::kSuccess,
            database_->UpdateEntries({bar_entry}));
  EXPECT_EQ(2u, database_->RetrieveAllEntries()->size());

  // Delete bar.
  database_->RemoveAnnotationsInRange(bar_create_time - base::Seconds(1),
                                      bar_create_time + base::Seconds(1));
  EXPECT_THAT(*database_->RetrieveAllEntries(),
              UnorderedElementsAre(EqualsProto(foo_entry)));

  // Delete foo.
  database_->RemoveAnnotationsInRange(foo_create_time - base::Seconds(1),
                                      foo_create_time + base::Seconds(1));
  EXPECT_TRUE(database_->RetrieveAllEntries()->empty());
}

}  // namespace user_annotations
