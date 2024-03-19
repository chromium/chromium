// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"

#include <memory>

#include "base/rand_util.h"
#include "components/segmentation_platform/internal/database/ukm_database_test_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

UkmMetricsTable::MetricsRow GetSampleMetricsRow() {
  static auto event_id_generator = MetricsRowEventId::Generator();
  static auto event_hash_generator = UkmEventHash::Generator();
  static auto metric_hash_generator = UkmMetricHash::Generator();
  return UkmMetricsTable::MetricsRow{
      .event_timestamp = base::Time::Now(),
      .source_id = base::RandInt(0, 1000),
      .event_id = event_id_generator.GenerateNextId(),
      .event_hash = event_hash_generator.GenerateNextId(),
      .metric_hash = metric_hash_generator.GenerateNextId(),
      .metric_value = base::RandInt(-1000, 1000)};
}

}  // namespace

class UkmMetricsTableTest : public testing::Test {
 public:
  UkmMetricsTableTest() = default;
  ~UkmMetricsTableTest() override = default;

  void SetUp() override {
    sql::DatabaseOptions options;
    db_ = std::make_unique<sql::Database>(options);
    bool opened = db_->OpenInMemory();
    ASSERT_TRUE(opened);
    metrics_table_ = std::make_unique<UkmMetricsTable>(db_.get());
  }

  void TearDown() override {
    metrics_table_.reset();
    db_.reset();
  }

  UkmMetricsTable& metrics_table() { return *metrics_table_; }

  sql::Database& db() { return *db_; }

 private:
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<UkmMetricsTable> metrics_table_;
};

TEST_F(UkmMetricsTableTest, CreateTable) {
  ASSERT_TRUE(metrics_table().InitTable());

  EXPECT_TRUE(db().DoesTableExist(UkmMetricsTable::kTableName));
  EXPECT_TRUE(db().DoesIndexExist("event_timestamp_index"));
  EXPECT_TRUE(db().DoesIndexExist("url_id_index"));
  EXPECT_TRUE(db().DoesIndexExist("ukm_source_id_index"));
  EXPECT_TRUE(db().DoesIndexExist("event_hash_index"));

  // Creating table again should be noop.
  ASSERT_TRUE(metrics_table().InitTable());

  EXPECT_TRUE(db().DoesTableExist(UkmMetricsTable::kTableName));
}

TEST_F(UkmMetricsTableTest, InsertRow) {
  ASSERT_TRUE(metrics_table().InitTable());

  EXPECT_TRUE(db().DoesTableExist(UkmMetricsTable::kTableName));

  // Invalid rows should not inserted.
  UkmMetricsTable::MetricsRow row;
  EXPECT_FALSE(metrics_table().AddUkmEvent(row));
  auto row1 = GetSampleMetricsRow();
  row1.event_timestamp = base::Time();
  EXPECT_FALSE(metrics_table().AddUkmEvent(row1));

  test_util::AssertRowsInMetricsTable(db(), {});

  auto row2 = GetSampleMetricsRow();
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  test_util::AssertRowsInMetricsTable(db(), {row2});

  auto row3 = GetSampleMetricsRow();
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));
  test_util::AssertRowsInMetricsTable(db(), {row2, row3});
}

TEST_F(UkmMetricsTableTest, DeleteForUrl) {
  auto url_id_generator = UrlId::Generator();
  const UrlId url_id1 = url_id_generator.GenerateNextId();
  const UrlId url_id2 = url_id_generator.GenerateNextId();
  const UrlId url_id3 = url_id_generator.GenerateNextId();

  ASSERT_TRUE(metrics_table().InitTable());

  // Delete on empty table does nothing.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({}));
  test_util::AssertRowsInMetricsTable(db(), {});
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1}));
  test_util::AssertRowsInMetricsTable(db(), {});

  auto row1 = GetSampleMetricsRow();
  row1.url_id = url_id1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  auto row2 = GetSampleMetricsRow();
  row2.url_id = url_id1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  auto row3 = GetSampleMetricsRow();
  row3.url_id = url_id2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));
  auto row4 = GetSampleMetricsRow();
  row4.url_id = url_id2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row4));

  test_util::AssertRowsInMetricsTable(db(), {row1, row2, row3, row4});

  // Remove empty URL list.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({}));
  test_util::AssertRowsInMetricsTable(db(), {row1, row2, row3, row4});

  // Remove matching URL ID, should remove 2 rows.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1}));
  test_util::AssertRowsInMetricsTable(db(), {row3, row4});

  // Remove non-existent URL IDs, no change to db.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1, url_id3}));
  test_util::AssertRowsInMetricsTable(db(), {row3, row4});

  auto row5 = GetSampleMetricsRow();
  row5.url_id = url_id3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row5));
  auto row6 = GetSampleMetricsRow();
  row6.url_id = url_id3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row6));

  test_util::AssertRowsInMetricsTable(db(), {row3, row4, row5, row6});

  // Remove all URL IDs, should clear the table.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1, url_id2, url_id3}));
  test_util::AssertRowsInMetricsTable(db(), {});
}

TEST_F(UkmMetricsTableTest, DeleteBeforeTimestamp) {
  const base::Time kTimestamp1 = base::Time::Now();
  const base::Time kTimestamp2 = kTimestamp1 + base::Seconds(1);
  const base::Time kTimestamp3 = kTimestamp1 + base::Seconds(2);
  const base::Time kTimestamp4 = kTimestamp1 + base::Seconds(3);
  const base::Time kTimestamp5 = kTimestamp1 + base::Seconds(4);
  const UrlId kUrl1 = UrlId::FromUnsafeValue(1);
  const UrlId kUrl2 = UrlId::FromUnsafeValue(2);
  const UrlId kUrl3 = UrlId::FromUnsafeValue(3);
  const UrlId kUrl4 = UrlId::FromUnsafeValue(4);

  ASSERT_TRUE(metrics_table().InitTable());

  // Delete on empty table does nothing.
  std::vector<UrlId> empty_set;
  EXPECT_EQ(empty_set,
            metrics_table().DeleteEventsBeforeTimestamp(base::Time()));
  test_util::AssertRowsInMetricsTable(db(), {});
  EXPECT_EQ(empty_set,
            metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1));
  test_util::AssertRowsInMetricsTable(db(), {});

  auto row1 = GetSampleMetricsRow();
  row1.event_timestamp = kTimestamp1;
  row1.url_id = kUrl1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  auto row2 = GetSampleMetricsRow();
  row2.event_timestamp = kTimestamp2;
  row2.url_id = kUrl2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  auto row3 = GetSampleMetricsRow();
  row3.event_timestamp = kTimestamp3;
  row3.url_id = kUrl3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));
  auto row4 = GetSampleMetricsRow();
  row4.event_timestamp = kTimestamp4;
  row4.url_id = kUrl4;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row4));

  test_util::AssertRowsInMetricsTable(db(), {row1, row2, row3, row4});

  // Delete with time before all rows does nothing.
  EXPECT_EQ(empty_set,
            metrics_table().DeleteEventsBeforeTimestamp(base::Time()));
  test_util::AssertRowsInMetricsTable(db(), {row1, row2, row3, row4});
  EXPECT_EQ(empty_set, metrics_table().DeleteEventsBeforeTimestamp(
                           kTimestamp1 - base::Seconds(1)));
  test_util::AssertRowsInMetricsTable(db(), {row1, row2, row3, row4});

  // Remove single row.
  EXPECT_EQ(std::vector<UrlId>({kUrl1}),
            metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1));
  test_util::AssertRowsInMetricsTable(db(), {row2, row3, row4});

  // Add more rows with UrlId3.
  auto row5 = GetSampleMetricsRow();
  row5.event_timestamp = kTimestamp5;
  row5.url_id = kUrl3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row5));
  auto row6 = GetSampleMetricsRow();
  row6.event_timestamp = kTimestamp5;
  row6.url_id = kUrl3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row6));
  test_util::AssertRowsInMetricsTable(db(), {row2, row3, row4, row5, row6});

  // Remove bunch of rows. UrlId3 should not be part of removed list since 2
  // other metrics reference it.
  EXPECT_EQ(std::vector<UrlId>({kUrl2, kUrl4}),
            metrics_table().DeleteEventsBeforeTimestamp(kTimestamp4));
  test_util::AssertRowsInMetricsTable(db(), {row5, row6});

  // Insert entry with an older timestamp out of order and remove old entries
  // should still work.
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  test_util::AssertRowsInMetricsTable(db(), {row5, row6, row1});
  EXPECT_EQ(std::vector<UrlId>({kUrl1}),
            metrics_table().DeleteEventsBeforeTimestamp(kTimestamp4));
  test_util::AssertRowsInMetricsTable(db(), {row5, row6});

  // Removing multiple entries with same timestamp and url should return the
  // right url to be removed.
  EXPECT_EQ(std::vector<UrlId>({kUrl3}),
            metrics_table().DeleteEventsBeforeTimestamp(kTimestamp5));
  test_util::AssertRowsInMetricsTable(db(), {});
}

TEST_F(UkmMetricsTableTest, MatchHashesTest) {
  const UkmEventHash event_hash1 = UkmEventHash::FromUnsafeValue(1);
  const UkmEventHash event_hash2 =
      UkmEventHash::FromUnsafeValue(0x9CEA8CBC362AB242);
  const UkmEventHash event_hash3 =
      UkmEventHash::FromUnsafeValue(std::numeric_limits<uint64_t>::max());
  ASSERT_TRUE(metrics_table().InitTable());

  UkmMetricsTable::MetricsRow row1 = GetSampleMetricsRow();
  row1.event_hash = event_hash1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  UkmMetricsTable::MetricsRow row2 = GetSampleMetricsRow();
  row2.event_hash = event_hash2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  UkmMetricsTable::MetricsRow row3 = GetSampleMetricsRow();
  row3.event_hash = event_hash3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));

  auto result1 = test_util::GetMetricsRowWithQuery(
      "SELECT * FROM metrics WHERE event_hash = '1'", db());
  ASSERT_EQ(1u, result1.size());
  test_util::ExpectRowIsEqual(row1, result1[0]);

  auto result2 = test_util::GetMetricsRowWithQuery(
      "SELECT * FROM metrics WHERE event_hash = '9CEA8CBC362AB242'", db());
  ASSERT_EQ(1u, result2.size());
  test_util::ExpectRowIsEqual(row2, result2[0]);

  auto result3 = test_util::GetMetricsRowWithQuery(
      "SELECT * FROM metrics WHERE event_hash = 'FFFFFFFFFFFFFFFF'", db());
  ASSERT_EQ(1u, result3.size());
  test_util::ExpectRowIsEqual(row2, result2[0]);
}

}  // namespace segmentation_platform
