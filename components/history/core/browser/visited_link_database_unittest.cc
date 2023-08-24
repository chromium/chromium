// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visited_link_database.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/url_database.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace history {

namespace {

bool IsVisitedLinkRowEqual(const VisitedLinkRow& a, const VisitedLinkRow& b) {
  return a.link_url_id == b.link_url_id && a.top_level_url == b.top_level_url &&
         a.frame_url == b.frame_url && a.visit_count == b.visit_count;
}

}  // namespace

class VisitedLinkDatabaseTest : public testing::Test,
                                public URLDatabase,
                                public VisitedLinkDatabase {
 public:
  VisitedLinkDatabaseTest() = default;

 protected:
  URLID GetLinkURLID() { return link_url_id_; }
  // Provided for URL/Visit/VisitedLinksDatabase.
  sql::Database& GetDB() override { return db_; }

 private:
  // Create and store the test tables.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file =
        temp_dir_.GetPath().AppendASCII("VisitedLinkTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the URL tables.
    CreateURLTable(false);
    CreateMainURLIndex();

    // Initialize the visited link table.
    CreateVisitedLinkTable();

    // Add a link URL to the URLDatabase.
    link_url_id_ = PopulateURLTable();
    EXPECT_TRUE(link_url_id_);
  }

  URLID PopulateURLTable() {
    // Add a row to the URLDatabase to reference in the
    // VisitedLinkDatabase.
    const GURL url("http://www.google.com/");
    URLRow url_row(url);
    url_row.set_title(u"Google");
    url_row.set_visit_count(4);
    url_row.set_typed_count(2);
    url_row.set_last_visit(Time::Now() - base::Days(1));
    url_row.set_hidden(false);
    URLID link_url_id = AddURL(url_row);
    return link_url_id;
  }

  void TearDown() override { db_.Close(); }

  URLID link_url_id_;
  base::ScopedTempDir temp_dir_;
  sql::Database db_;
};

// Test add, update, and delete operations for the
// VisitedLinkDatabase.
TEST_F(VisitedLinkDatabaseTest, AddVisitedLink) {
  // Add two rows to the VisitedLinkDatabase.
  const GURL top_level_url1("http://docs.google.com/");
  const GURL frame_url1("http://meet.google.com/");
  VisitedLinkID row1_id = AddVisitedLink(GetLinkURLID(), top_level_url1,
                                         frame_url1, /*visit_count=*/1);
  EXPECT_TRUE(row1_id);

  const GURL top_level_url2("http://mail.google.com/");
  const GURL frame_url2("http://mail.google.com/");
  VisitedLinkID row2_id = AddVisitedLink(GetLinkURLID(), top_level_url2,
                                         frame_url2, /*visit_count=*/1024);
  EXPECT_TRUE(row2_id);

  // Query both of them.
  VisitedLinkRow row_by_values, row_by_id;
  EXPECT_TRUE(GetRowForVisitedLink(GetLinkURLID(), top_level_url1, frame_url1,
                                   row_by_values));
  EXPECT_TRUE(GetVisitedLinkRow(row1_id, row_by_id));
  EXPECT_TRUE(IsVisitedLinkRowEqual(row_by_values, row_by_id));
  EXPECT_TRUE(GetRowForVisitedLink(GetLinkURLID(), top_level_url2, frame_url2,
                                   row_by_values));
  EXPECT_TRUE(GetVisitedLinkRow(row2_id, row_by_id));
  EXPECT_TRUE(IsVisitedLinkRowEqual(row_by_values, row_by_id));

  // Delete the rows we added.
  EXPECT_TRUE(DeleteVisitedLinkRow(row1_id));
  EXPECT_TRUE(DeleteVisitedLinkRow(row2_id));

  // Ensure they were deleted.
  VisitedLinkRow returned_row;
  EXPECT_FALSE(GetVisitedLinkRow(row1_id, returned_row));
  EXPECT_FALSE(GetVisitedLinkRow(row2_id, returned_row));
}

TEST_F(VisitedLinkDatabaseTest, UpdateVisitedLink) {
  // Add a row to the VisitedLinkDatabase.
  const GURL top_level_url("http://mail.google.com/");
  const GURL frame_url("http://maps.google.com/");
  VisitedLinkID row_id = AddVisitedLink(GetLinkURLID(), top_level_url,
                                        frame_url, /*visit_count=*/1);
  EXPECT_TRUE(row_id);

  // Ensure updating a non-existing row has no effect.
  VisitedLinkID nonexistent_id = row_id + 1024;
  EXPECT_FALSE(
      UpdateVisitedLinkRowVisitCount(nonexistent_id, /*visit_count=*/100));
  VisitedLinkRow nonexistent_row;
  EXPECT_FALSE(GetVisitedLinkRow(nonexistent_id, nonexistent_row));

  // Ensure we can update the visit count of an existing row.
  int new_visit_count = 35;
  EXPECT_TRUE(UpdateVisitedLinkRowVisitCount(row_id, new_visit_count));
  VisitedLinkRow updated_row;
  EXPECT_TRUE(GetVisitedLinkRow(row_id, updated_row));
  EXPECT_EQ(updated_row.visit_count, new_visit_count);

  // Delete the row we added.
  EXPECT_TRUE(DeleteVisitedLinkRow(row_id));
  EXPECT_FALSE(GetVisitedLinkRow(row_id, nonexistent_row));
}

}  // namespace history
