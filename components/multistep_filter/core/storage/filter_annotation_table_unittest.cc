// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_annotation_table.h"

#include "base/test/task_environment.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

class FilterAnnotationTableTest : public testing::Test {
 public:
  FilterAnnotationTableTest() = default;
  ~FilterAnnotationTableTest() override = default;

  FilterAnnotationTable* table() { return &table_; }
  sql::Database* db() { return &db_; }

  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(table_.Init(&db_));
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  sql::Database db_{sql::DatabaseOptions{},
                    sql::Database::Tag("FilterAnnotationTableTest")};
  FilterAnnotationTable table_;
};

}  // namespace multistep_filter
