// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_table.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "components/autofill/core/browser/field_types.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PASSWORD;
using autofill::SINGLE_USERNAME;
using autofill::USERNAME;
using base::Time;
using testing::ElementsAre;

namespace password_manager {
namespace {

class FieldInfoTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");
    db_ = std::make_unique<FieldInfoTable>();
    connection_ = std::make_unique<sql::Database>();
    connection_->set_exclusive_locking();
    ASSERT_TRUE(connection_->Open(file));
    db_->Init(connection_.get());
    ASSERT_TRUE(db_->CreateTableIfNecessary());

    test_data_.push_back({101u, 1u, USERNAME, Time::FromTimeT(1)});
    test_data_.push_back({101u, 10u, PASSWORD, Time::FromTimeT(5)});
    test_data_.push_back({102u, 1u, SINGLE_USERNAME, Time::FromTimeT(10)});
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> connection_;
  std::unique_ptr<FieldInfoTable> db_;
  std::vector<FieldInfo> test_data_;
};

TEST_F(FieldInfoTableTest, AddRow) {
  EXPECT_TRUE(db_->AddRow(test_data_[0]));
  EXPECT_EQ(test_data_[0], db_->GetAllRows()[0]);
}

TEST_F(FieldInfoTableTest, Reload) {
  EXPECT_TRUE(db_->AddRow(test_data_[0]));
  ReloadDatabase();
  EXPECT_EQ(test_data_[0], db_->GetAllRows()[0]);
}

TEST_F(FieldInfoTableTest, AddManyRow) {
  for (const FieldInfo& field : test_data_)
    EXPECT_TRUE(db_->AddRow(field));
  EXPECT_EQ(test_data_, db_->GetAllRows());
}

TEST_F(FieldInfoTableTest, RemoveRowsByTime) {
  for (const FieldInfo& field : test_data_)
    EXPECT_TRUE(db_->AddRow(field));
  db_->RemoveRowsByTime(Time::FromTimeT(1), Time::FromTimeT(10));
  std::vector<FieldInfo> expected_rows = {test_data_[2]};
  EXPECT_EQ(expected_rows, db_->GetAllRows());
}

TEST_F(FieldInfoTableTest, GetAllRowsForFormSignature) {
  for (const FieldInfo& field : test_data_)
    EXPECT_TRUE(db_->AddRow(field));

  constexpr uint64_t kFirstFormSignature = 101u;
  constexpr uint64_t kSecondFormSignature = 102u;
  constexpr uint64_t kNotExistingSignature = 1001;

  std::vector<FieldInfo> expected_rows = {test_data_[0], test_data_[1]};
  EXPECT_EQ(expected_rows,
            db_->GetAllRowsForFormSignature(kFirstFormSignature));

  expected_rows = {test_data_[2]};
  EXPECT_EQ(expected_rows,
            db_->GetAllRowsForFormSignature(kSecondFormSignature));

  EXPECT_TRUE(db_->GetAllRowsForFormSignature(kNotExistingSignature).empty());
}

}  // namespace
}  // namespace password_manager
