// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/key_value_table.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/protobuf_matchers.h"
#include "components/sqlite_proto/test_proto.pb.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_proto {

namespace {

using base::test::EqualsProto;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

bool CreateProtoTable(sql::Database* db) {
  const char kCreateProtoTableStatementTemplate[] =
      "CREATE TABLE my_table ( "
      "key TEXT, "
      "proto BLOB, "
      "PRIMARY KEY(key))";
  return db->Execute(kCreateProtoTableStatementTemplate);
}

}  // namespace

class KeyValueTableTest : public ::testing::Test {
 public:
  KeyValueTableTest() {
    CHECK(db_.OpenInMemory());
    CHECK(CreateProtoTable(&db_));
  }
  ~KeyValueTableTest() override = default;

 protected:
  sql::Database db_{sql::test::kTestTag};
  KeyValueTable<TestProto> table_{"my_table"};
};

TEST_F(KeyValueTableTest, Empty) {
  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_TRUE(my_data.empty());
}

TEST_F(KeyValueTableTest, PutAndGet) {
  TestProto element;
  element.set_value(1);
  table_.UpdateData("a", element, &db_);

  TestProto second_element;
  second_element.set_value(2);
  table_.UpdateData("b", second_element, &db_);

  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_THAT(my_data,
              UnorderedElementsAre(Pair("a", EqualsProto(element)),
                                   Pair("b", EqualsProto(second_element))));
}

TEST_F(KeyValueTableTest, Update) {
  TestProto element;
  element.set_value(1);

  TestProto superseding_element;
  superseding_element.set_value(2);

  table_.UpdateData("a", element, &db_);
  table_.UpdateData("a", superseding_element, &db_);

  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_THAT(my_data,
              ElementsAre(Pair("a", EqualsProto(superseding_element))));
}

TEST_F(KeyValueTableTest, DeleteNonexistent) {
  TestProto element;
  element.set_value(1);
  table_.UpdateData("a", element, &db_);

  // Deleting a nonexistent key should no-op.
  table_.DeleteData(std::vector<std::string>{"b"}, &db_);

  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_THAT(my_data, ElementsAre(Pair("a", EqualsProto(element))));
}

TEST_F(KeyValueTableTest, Delete) {
  TestProto element;
  element.set_value(1);
  table_.UpdateData("a", element, &db_);

  table_.DeleteData(std::vector<std::string>{"a"}, &db_);

  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_TRUE(my_data.empty());
}

TEST_F(KeyValueTableTest, DeleteAll) {
  TestProto element;
  element.set_value(1);

  table_.UpdateData("a", element, &db_);
  table_.UpdateData("b", element, &db_);

  table_.DeleteAllData(&db_);

  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_TRUE(my_data.empty());
}

// Storing an element with a default proto value (zero byte size) should work;
// this can be useful since a default value can have different semantics from a
// missing value.
TEST_F(KeyValueTableTest, PutGetDefaultValue) {
  TestProto element;
  table_.UpdateData("a", element, &db_);
  ASSERT_EQ(element.ByteSizeLong(), 0u);

  std::map<std::string, TestProto> my_data;
  table_.GetAllData(&my_data, &db_);
  EXPECT_THAT(my_data, ElementsAre(Pair("a", EqualsProto(element))));
}

}  // namespace sqlite_proto
