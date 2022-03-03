// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"

#include <memory>

#include "base/rand_util.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

namespace {

void ExpectRowIsEqual(const UkmMetricsTable::MetricsRow& row1,
                      const UkmMetricsTable::MetricsRow& row2) {
  // Skip checking ID.
  EXPECT_EQ(row1.url_id, row2.url_id);
  EXPECT_EQ(row1.event_timestamp, row2.event_timestamp);
  EXPECT_EQ(row1.source_id, row2.source_id);
  EXPECT_EQ(row1.event_id, row2.event_id);
  EXPECT_EQ(row1.event_hash, row2.event_hash);
  EXPECT_EQ(row1.metric_hash, row2.metric_hash);
  EXPECT_EQ(row1.metric_value, row2.metric_value);
}

UkmMetricsTable::MetricsRow GetSampleRow() {
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

  absl::optional<UkmMetricsTable::MetricsRow> GetSingleRow(const char* query) {
    sql::Statement statement(db_->GetUniqueStatement(query));
    if (statement.Step()) {
      return UkmMetricsTable::FillRowFromStatementForTesting(statement);
    }
    return absl::nullopt;
  }

  void AssertRowsInTable(const std::vector<UkmMetricsTable::MetricsRow>& rows) {
    sql::Statement statement(db_->GetCachedStatement(
        SQL_FROM_HERE, "SELECT * FROM metrics ORDER BY id"));
    std::vector<UkmMetricsTable::MetricsRow> actual_rows;
    while (statement.Step()) {
      actual_rows.emplace_back(
          UkmMetricsTable::FillRowFromStatementForTesting(statement));
    }
    ASSERT_EQ(actual_rows.size(), rows.size());
    auto it1 = actual_rows.begin();
    auto it2 = rows.begin();
    for (; it1 != actual_rows.end(); ++it1, ++it2) {
      ExpectRowIsEqual(*it1, *it2);
    }
  }

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
  auto row1 = GetSampleRow();
  row1.event_timestamp = base::Time();
  EXPECT_FALSE(metrics_table().AddUkmEvent(row1));

  AssertRowsInTable({});

  auto row2 = GetSampleRow();
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  AssertRowsInTable({row2});

  auto row3 = GetSampleRow();
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));
  AssertRowsInTable({row2, row3});
}

TEST_F(UkmMetricsTableTest, DeleteForUrl) {
  auto url_id_generator = UrlId::Generator();
  const UrlId url_id1 = url_id_generator.GenerateNextId();
  const UrlId url_id2 = url_id_generator.GenerateNextId();
  const UrlId url_id3 = url_id_generator.GenerateNextId();

  ASSERT_TRUE(metrics_table().InitTable());

  // Delete on empty table does nothing.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({}));
  AssertRowsInTable({});
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1}));
  AssertRowsInTable({});

  auto row1 = GetSampleRow();
  row1.url_id = url_id1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  auto row2 = GetSampleRow();
  row2.url_id = url_id1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  auto row3 = GetSampleRow();
  row3.url_id = url_id2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));
  auto row4 = GetSampleRow();
  row4.url_id = url_id2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row4));

  AssertRowsInTable({row1, row2, row3, row4});

  // Remove empty URL list.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({}));
  AssertRowsInTable({row1, row2, row3, row4});

  // Remove matching URL ID, should remove 2 rows.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1}));
  AssertRowsInTable({row3, row4});

  // Remove non-existent URL IDs, no change to db.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1, url_id3}));
  AssertRowsInTable({row3, row4});

  auto row5 = GetSampleRow();
  row5.url_id = url_id3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row5));
  auto row6 = GetSampleRow();
  row6.url_id = url_id3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row6));

  AssertRowsInTable({row3, row4, row5, row6});

  // Remove all URL IDs, should clear the table.
  EXPECT_TRUE(metrics_table().DeleteEventsForUrls({url_id1, url_id2, url_id3}));
  AssertRowsInTable({});
}

TEST_F(UkmMetricsTableTest, DeleteBeforeTimestamp) {
  const base::Time kTimestamp1 = base::Time::Now();
  const base::Time kTimestamp2 = kTimestamp1 + base::Seconds(1);
  const base::Time kTimestamp3 = kTimestamp1 + base::Seconds(2);
  const base::Time kTimestamp4 = kTimestamp1 + base::Seconds(3);
  const base::Time kTimestamp5 = kTimestamp1 + base::Seconds(4);

  ASSERT_TRUE(metrics_table().InitTable());

  // Delete on empty table does nothing.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(base::Time()));
  AssertRowsInTable({});
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1));
  AssertRowsInTable({});

  auto row1 = GetSampleRow();
  row1.event_timestamp = kTimestamp1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  auto row2 = GetSampleRow();
  row2.event_timestamp = kTimestamp2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  auto row3 = GetSampleRow();
  row3.event_timestamp = kTimestamp3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));
  auto row4 = GetSampleRow();
  row4.event_timestamp = kTimestamp4;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row4));

  AssertRowsInTable({row1, row2, row3, row4});

  // Delete with time before all rows does nothing.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(base::Time()));
  AssertRowsInTable({row1, row2, row3, row4});
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1 -
                                                          base::Seconds(1)));
  AssertRowsInTable({row1, row2, row3, row4});

  // Remove single row.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1));
  AssertRowsInTable({row2, row3, row4});

  auto row5 = GetSampleRow();
  row5.event_timestamp = kTimestamp5;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row5));
  AssertRowsInTable({row2, row3, row4, row5});

  // Remove bunch of rows.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp3));
  AssertRowsInTable({row4, row5});

  // Insert entry with an older timestamp out of order and remove old entries
  // should still work.
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  AssertRowsInTable({row4, row5, row1});
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp3));
  AssertRowsInTable({row4, row5});
}

TEST_F(UkmMetricsTableTest, MatchHashesTest) {
  const UkmEventHash event_hash1 = UkmEventHash::FromUnsafeValue(1);
  const UkmEventHash event_hash2 =
      UkmEventHash::FromUnsafeValue(0x9CEA8CBC362AB242);
  const UkmEventHash event_hash3 =
      UkmEventHash::FromUnsafeValue(std::numeric_limits<uint64_t>::max());
  ASSERT_TRUE(metrics_table().InitTable());

  UkmMetricsTable::MetricsRow row1 = GetSampleRow();
  row1.event_hash = event_hash1;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row1));
  UkmMetricsTable::MetricsRow row2 = GetSampleRow();
  row2.event_hash = event_hash2;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row2));
  UkmMetricsTable::MetricsRow row3 = GetSampleRow();
  row3.event_hash = event_hash3;
  EXPECT_TRUE(metrics_table().AddUkmEvent(row3));

  auto result1 = GetSingleRow("SELECT * FROM metrics WHERE event_hash = '1'");
  ASSERT_TRUE(result1);
  ExpectRowIsEqual(row1, *result1);

  auto result2 = GetSingleRow(
      "SELECT * FROM metrics WHERE event_hash = '9CEA8CBC362AB242'");
  ASSERT_TRUE(result2);
  ExpectRowIsEqual(row2, *result2);

  auto result3 = GetSingleRow(
      "SELECT * FROM metrics WHERE event_hash = 'FFFFFFFFFFFFFFFF'");
  ASSERT_TRUE(result3);
  ExpectRowIsEqual(row3, *result3);
}

}  // namespace segmentation_platform
