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
    database_ = std::make_unique<UserAnnotationsDatabase>(temp_dir_.GetPath());
  }

  void TearDown() override {
    database_.reset();
    CHECK(temp_dir_.Delete());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<UserAnnotationsDatabase> database_;
};

TEST_F(UserAnnotationsDatabaseTest, StoreAndRetrieve) {
  EXPECT_TRUE(database_->RetrieveAllEntries().empty());

  std::vector<UserAnnotationsEntry> entries;
  entries.push_back(CreateUserAnnotationsEntry(1, "foo", "foo_value"));
  entries.push_back(CreateUserAnnotationsEntry(2, "bar", "bar_value"));

  EXPECT_TRUE(database_->UpdateEntries(entries));
  EXPECT_THAT(
      database_->RetrieveAllEntries(),
      UnorderedElementsAre(EqualsProto(entries[0]), EqualsProto(entries[1])));

  // Reopen the database, and it should have the entries.
  database_ = std::make_unique<UserAnnotationsDatabase>(temp_dir_.GetPath());
  EXPECT_THAT(
      database_->RetrieveAllEntries(),
      UnorderedElementsAre(EqualsProto(entries[0]), EqualsProto(entries[1])));
}

}  // namespace user_annotations
