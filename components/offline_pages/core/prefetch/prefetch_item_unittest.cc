// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_item.h"

#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_schema.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

class PrefetchItemTest : public testing::Test {
 public:
  void CheckFieldAndResetItem(PrefetchItem& item, const char* tested_field);
  void CheckAllFieldsWereTested();

  std::size_t GetTableColumnsCount();

 private:
  std::size_t checked_fields_counter_ = 0;
};

// Checks behavior of some PrefetchItem methods for an item with one single
// member updated to a non-default value. After testing the item is reset and
// |checked_fields_counter_| is incremented.
void PrefetchItemTest::CheckFieldAndResetItem(PrefetchItem& item,
                                              const char* tested_field) {
  EXPECT_NE(item, PrefetchItem())
      << "Item with updated \"" << tested_field
      << "\" should not equal a default constructed item";
  EXPECT_EQ(item, PrefetchItem(item))
      << "Item with updated \"" << tested_field
      << "\" should equal a copy constructed based off of it";
  EXPECT_NE(item.ToString(), PrefetchItem().ToString())
      << "Result of ToString() from an item with updated \"" << tested_field
      << "\" should not equal the ToString() result from a default constructed"
         " item";
  item = PrefetchItem();
  ++checked_fields_counter_;
}

// Compares the |checked_fields_counter_| value with the number of columns in
// the SQLite table to make sure they match. This should be run after all
// PrefetchItem fields were verified with CheckFieldAndResetItem and it helps in
// confirming PrefetchItem implementation and tests are coping with changes in
// the table.
void PrefetchItemTest::CheckAllFieldsWereTested() {
  // Note: the off-by-1 difference is due to the single client_id field, of type
  // ClientID, representing 2 columns in the database table (client_namespace
  // and client_id).
  EXPECT_EQ(GetTableColumnsCount() - 1, checked_fields_counter_)
      << "The number of tested fields mismatches the number of columns in the "
         "database table.";
}

// Computes the number of columns the SQL table has.
std::size_t PrefetchItemTest::GetTableColumnsCount() {
  std::string tableCreationSql =
      PrefetchStoreSchema::GetItemTableCreationSqlForTesting();
  std::vector<std::string> create_statement_split = base::SplitString(
      tableCreationSql, "()", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  EXPECT_EQ(3U, create_statement_split.size());

  std::string& columns_list = create_statement_split[1];
  std::vector<std::string> columns_split = base::SplitString(
      columns_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return columns_split.size();
}

TEST_F(PrefetchItemTest, OperatorEqualsCopyConstructorAndToString) {
  PrefetchItem item1;
  EXPECT_EQ(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  EXPECT_EQ(item1.ToString(), PrefetchItem().ToString());

  item1.offline_id = 77L;
  CheckFieldAndResetItem(item1, "offline_id");

  item1.guid = "A";
  CheckFieldAndResetItem(item1, "guid");

  item1.client_id = ClientId("B", "C");
  CheckFieldAndResetItem(item1, "client_id");

  item1.state = PrefetchItemState::AWAITING_GCM;
  CheckFieldAndResetItem(item1, "state");

  item1.url = GURL("http://test.com");
  CheckFieldAndResetItem(item1, "url");

  item1.final_archived_url = GURL("http://test.com/final");
  CheckFieldAndResetItem(item1, "final_archived_url");

  item1.thumbnail_url = GURL("http://thumbnail");
  CheckFieldAndResetItem(item1, "thumbnail_url");

  item1.favicon_url = GURL("http://favicon");
  CheckFieldAndResetItem(item1, "favicon_url");

  item1.generate_bundle_attempts = 10;
  CheckFieldAndResetItem(item1, "generate_bundle_attempts");

  item1.get_operation_attempts = 11;
  CheckFieldAndResetItem(item1, "get_operation_attempts");

  item1.download_initiation_attempts = 12;
  CheckFieldAndResetItem(item1, "download_initiation_attempts");

  item1.operation_name = "D";
  CheckFieldAndResetItem(item1, "operation_name");

  item1.archive_body_name = "E";
  CheckFieldAndResetItem(item1, "archive_body_name");

  item1.archive_body_length = 20;
  CheckFieldAndResetItem(item1, "archive_body_length");

  item1.creation_time = base::Time::FromJavaTime(1000L);
  CheckFieldAndResetItem(item1, "creation_time");

  item1.freshness_time = base::Time::FromJavaTime(2000L);
  CheckFieldAndResetItem(item1, "freshness_time");

  item1.error_code = PrefetchItemErrorCode::TOO_MANY_NEW_URLS;
  CheckFieldAndResetItem(item1, "error_code");

  item1.title = base::UTF8ToUTF16("F");
  CheckFieldAndResetItem(item1, "title");

  item1.file_path = base::FilePath(FILE_PATH_LITERAL("G"));
  CheckFieldAndResetItem(item1, "file_path");

  item1.file_size = 30;
  CheckFieldAndResetItem(item1, "file_size");

  item1.snippet = "G";
  CheckFieldAndResetItem(item1, "snippet");

  item1.attribution = "H";
  CheckFieldAndResetItem(item1, "attribution");

  CheckAllFieldsWereTested();
}

}  // namespace offline_pages
