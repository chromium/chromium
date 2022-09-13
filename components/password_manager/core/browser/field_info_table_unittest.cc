// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_table.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
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
    field_info_table_ = std::make_unique<FieldInfoTable>();
    db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
        .exclusive_locking = true, .page_size = 4096, .cache_size = 500});
    ASSERT_TRUE(db_->Open(file));
    field_info_table_->Init(db_.get());
    ASSERT_TRUE(field_info_table_->CreateTableIfNecessary());

    test_data_.push_back({autofill::FormSignature(101u),
                          autofill::FieldSignature(1u), USERNAME,
                          Time::FromTimeT(1)});
    test_data_.push_back({autofill::FormSignature(101u),
                          autofill::FieldSignature(10u), PASSWORD,
                          Time::FromTimeT(5)});
    test_data_.push_back({autofill::FormSignature(102u),
                          autofill::FieldSignature(1u), SINGLE_USERNAME,
                          Time::FromTimeT(10)});
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<FieldInfoTable> field_info_table_;
  std::vector<FieldInfo> test_data_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(FieldInfoTableTest, AddRow) {
  EXPECT_TRUE(field_info_table_->AddRow(test_data_[0]));
  EXPECT_EQ(test_data_[0], field_info_table_->GetAllRows()[0]);
}

TEST_F(FieldInfoTableTest, Reload) {
  EXPECT_TRUE(field_info_table_->AddRow(test_data_[0]));
  ReloadDatabase();
  EXPECT_EQ(test_data_[0], field_info_table_->GetAllRows()[0]);
}

TEST_F(FieldInfoTableTest, AddManyRow) {
  for (const FieldInfo& field : test_data_)
    EXPECT_TRUE(field_info_table_->AddRow(field));
  EXPECT_EQ(test_data_, field_info_table_->GetAllRows());
}

TEST_F(FieldInfoTableTest, RemoveRowsByTime) {
  for (const FieldInfo& field : test_data_)
    EXPECT_TRUE(field_info_table_->AddRow(field));
  field_info_table_->RemoveRowsByTime(Time::FromTimeT(1), Time::FromTimeT(10));
  std::vector<FieldInfo> expected_rows = {test_data_[2]};
  EXPECT_EQ(expected_rows, field_info_table_->GetAllRows());
}

TEST_F(FieldInfoTableTest, GetAllRowsForFormSignature) {
  for (const FieldInfo& field : test_data_)
    EXPECT_TRUE(field_info_table_->AddRow(field));

  constexpr uint64_t kFirstFormSignature = 101u;
  constexpr uint64_t kSecondFormSignature = 102u;
  constexpr uint64_t kNotExistingSignature = 1001;

  std::vector<FieldInfo> expected_rows = {test_data_[0], test_data_[1]};
  EXPECT_EQ(expected_rows,
            field_info_table_->GetAllRowsForFormSignature(kFirstFormSignature));

  expected_rows = {test_data_[2]};
  EXPECT_EQ(expected_rows, field_info_table_->GetAllRowsForFormSignature(
                               kSecondFormSignature));

  EXPECT_TRUE(
      field_info_table_->GetAllRowsForFormSignature(kNotExistingSignature)
          .empty());
}

TEST_F(FieldInfoTableTest, DropTableIfExists) {
  ASSERT_TRUE(db_->DoesTableExist("field_info"));
  EXPECT_TRUE(field_info_table_->DropTableIfExists());
  EXPECT_FALSE(db_->DoesTableExist("field_info"));
  EXPECT_FALSE(field_info_table_->DropTableIfExists());
}

#else
TEST_F(FieldInfoTableTest, NoTable) {
  EXPECT_FALSE(db_->DoesTableExist("field_info"));
}

TEST_F(FieldInfoTableTest, AddRowNoOp) {
  EXPECT_FALSE(field_info_table_->AddRow(test_data_[0]));
  EXPECT_TRUE(field_info_table_->GetAllRows().empty());
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace password_manager
