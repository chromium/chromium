// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_url_table.h"

#include "components/segmentation_platform/internal/database/ukm_database_test_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using ::segmentation_platform::test_util::UrlMatcher;

}  // namespace

class UkmUrlTableTest : public testing::Test {
 public:
  UkmUrlTableTest() = default;
  ~UkmUrlTableTest() override = default;

  void SetUp() override {
    sql::DatabaseOptions options;
    db_ = std::make_unique<sql::Database>(options);
    bool opened = db_->OpenInMemory();
    ASSERT_TRUE(opened);
    url_table_ = std::make_unique<UkmUrlTable>(db_.get());
  }

  void TearDown() override {
    url_table_.reset();
    db_.reset();
  }

 protected:
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<UkmUrlTable> url_table_;
};

TEST_F(UkmUrlTableTest, CreateTable) {
  ASSERT_TRUE(url_table_->InitTable());

  EXPECT_TRUE(db_->DoesTableExist(UkmUrlTable::kTableName));

  // Creating table again should be noop.
  ASSERT_TRUE(url_table_->InitTable());

  EXPECT_TRUE(db_->DoesTableExist(UkmUrlTable::kTableName));
}

TEST_F(UkmUrlTableTest, InsertUrl) {
  const GURL kUrl("https://www.url1.com");
  auto url_id_generator = UrlId::Generator();
  const UrlId kUrlId1 = url_id_generator.GenerateNextId();
  const UrlId kUrlId2 = url_id_generator.GenerateNextId();
  const UrlId kUrlId3 = url_id_generator.GenerateNextId();

  ASSERT_TRUE(url_table_->InitTable());
  EXPECT_FALSE(url_table_->IsUrlInTable(kUrlId1));

  EXPECT_TRUE(url_table_->WriteUrl(kUrl, kUrlId1));
  EXPECT_TRUE(url_table_->IsUrlInTable(kUrlId1));
  {
    sql::test::ScopedErrorExpecter error_expector;
    error_expector.ExpectError(SQLITE_CONSTRAINT_PRIMARYKEY);
    EXPECT_FALSE(url_table_->WriteUrl(kUrl, kUrlId1));
    ASSERT_TRUE(error_expector.SawExpectedErrors());
  }
  EXPECT_TRUE(url_table_->IsUrlInTable(kUrlId1));

  test_util::AssertUrlsInTable(*db_, {UrlMatcher{kUrlId1, kUrl}});
  EXPECT_FALSE(url_table_->IsUrlInTable(kUrlId2));

  EXPECT_TRUE(url_table_->WriteUrl(kUrl, kUrlId2));
  EXPECT_TRUE(url_table_->WriteUrl(kUrl, kUrlId3));

  test_util::AssertUrlsInTable(
      *db_, {UrlMatcher{kUrlId1, kUrl}, UrlMatcher{kUrlId2, kUrl},
             UrlMatcher{kUrlId3, kUrl}});
  EXPECT_TRUE(url_table_->IsUrlInTable(kUrlId1));
  EXPECT_TRUE(url_table_->IsUrlInTable(kUrlId2));
  EXPECT_TRUE(url_table_->IsUrlInTable(kUrlId3));
}

TEST_F(UkmUrlTableTest, GenerateUrlId) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const UrlId kUrlId3 = UkmUrlTable::GenerateUrlId(kUrl3);
  ASSERT_NE(kUrlId1, kUrlId2);
  ASSERT_NE(kUrlId1, kUrlId3);
  ASSERT_NE(kUrlId2, kUrlId3);
}

TEST_F(UkmUrlTableTest, RemoveUrls) {
  const GURL kUrl1("https://www.url1.com");
  const GURL kUrl2("https://www.url2.com");
  const GURL kUrl3("https://www.url3.com");
  const UrlId kUrlId1 = UkmUrlTable::GenerateUrlId(kUrl1);
  const UrlId kUrlId2 = UkmUrlTable::GenerateUrlId(kUrl2);
  const UrlId kUrlId3 = UkmUrlTable::GenerateUrlId(kUrl3);

  ASSERT_TRUE(url_table_->InitTable());
  EXPECT_TRUE(url_table_->RemoveUrls({kUrlId1}));

  EXPECT_TRUE(url_table_->WriteUrl(kUrl1, kUrlId1));
  EXPECT_TRUE(url_table_->WriteUrl(kUrl2, kUrlId2));
  EXPECT_TRUE(url_table_->WriteUrl(kUrl3, kUrlId3));

  test_util::AssertUrlsInTable(
      *db_, {UrlMatcher{kUrlId1, kUrl1}, UrlMatcher{kUrlId2, kUrl2},
             UrlMatcher{kUrlId3, kUrl3}});

  EXPECT_TRUE(url_table_->RemoveUrls({kUrlId2}));
  test_util::AssertUrlsInTable(
      *db_, {UrlMatcher{kUrlId1, kUrl1}, UrlMatcher{kUrlId3, kUrl3}});

  EXPECT_TRUE(url_table_->RemoveUrls({kUrlId1, kUrlId3}));
  test_util::AssertUrlsInTable(*db_, {});
}

}  // namespace segmentation_platform
