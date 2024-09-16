// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/uma_metrics_table.h"

#include <memory>

#include "base/rand_util.h"
#include "components/segmentation_platform/internal/database/ukm_database_test_utils.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

constexpr char kProfileId[] = "123";
constexpr char kProfileIdOther[] = "1234";
constexpr char kInvalidProfileId[] = "";

UmaMetricEntry GetSampleMetricsRow() {
  return UmaMetricEntry{.type = proto::SignalType::HISTOGRAM_VALUE,
                        .name_hash = 10,
                        .time = base::Time::Now(),
                        .value = 100};
}

}  // namespace

class UmaMetricsTableTest : public testing::Test {
 public:
  UmaMetricsTableTest() = default;
  ~UmaMetricsTableTest() override = default;

  void SetUp() override {
    sql::DatabaseOptions options;
    db_ = std::make_unique<sql::Database>(options);
    bool opened = db_->OpenInMemory();
    ASSERT_TRUE(opened);
    metrics_table_ = std::make_unique<UmaMetricsTable>(db_.get());
  }

  void TearDown() override {
    metrics_table_.reset();
    db_.reset();
  }

  UmaMetricsTable& metrics_table() { return *metrics_table_; }

  sql::Database& db() { return *db_; }

 private:
  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<UmaMetricsTable> metrics_table_;
};

TEST_F(UmaMetricsTableTest, CreateTable) {
  EXPECT_FALSE(db().DoesTableExist(UmaMetricsTable::kTableName));
  ASSERT_TRUE(metrics_table().InitTable());

  EXPECT_TRUE(db().DoesTableExist(UmaMetricsTable::kTableName));
  EXPECT_TRUE(db().DoesIndexExist("uma_event_timestamp_index"));
  EXPECT_TRUE(db().DoesIndexExist("uma_type_index"));
  EXPECT_TRUE(db().DoesIndexExist("uma_profile_id_index"));
  EXPECT_TRUE(db().DoesIndexExist("uma_metric_hash_index"));

  // Creating table again should be noop.
  ASSERT_TRUE(metrics_table().InitTable());

  EXPECT_TRUE(db().DoesTableExist(UmaMetricsTable::kTableName));
}

TEST_F(UmaMetricsTableTest, InsertRow) {
  ASSERT_TRUE(metrics_table().InitTable());

  EXPECT_TRUE(db().DoesTableExist(UmaMetricsTable::kTableName));

  // Invalid rows should not inserted.
  // Empty row:
  UmaMetricEntry row;
  EXPECT_FALSE(metrics_table().AddUmaMetric(kProfileId, row));
  // Invalid profile ID:
  auto row1 = GetSampleMetricsRow();
  EXPECT_FALSE(metrics_table().AddUmaMetric(kInvalidProfileId, row1));
  // Empty timestamp:
  row1.time = base::Time();
  EXPECT_FALSE(metrics_table().AddUmaMetric(kProfileId, row1));

  test_util::AssertRowsInUmaMetricsTable(db(), {});

  auto row2 = GetSampleMetricsRow();
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row2));
  test_util::AssertRowsInUmaMetricsTable(db(), {row2});

  auto row3 = GetSampleMetricsRow();
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row3));
  test_util::AssertRowsInUmaMetricsTable(db(), {row2, row3});
}

TEST_F(UmaMetricsTableTest, CleanupItems) {
  const base::Time kTimestamp1 = base::Time::Now();
  const base::Time kTimestamp2 = kTimestamp1 + base::Seconds(1);
  const base::Time kTimestamp3 = kTimestamp1 + base::Seconds(2);
  const base::Time kTimestamp4 = kTimestamp1 + base::Seconds(3);
  const base::Time kTimestamp5 = kTimestamp1 + base::Seconds(4);
  const int64_t kHash1 = 201;
  const int64_t kHash2 = 202;
  const int64_t kHash3 = 203;
  const int64_t kHash4 = 204;

  ASSERT_TRUE(metrics_table().InitTable());

  std::vector<CleanupItem> empty_items;
  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, empty_items));
  test_util::AssertRowsInUmaMetricsTable(db(), {});

  std::vector<CleanupItem> items{
      CleanupItem(kHash1, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp3),
      CleanupItem(kHash2, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp3),
      CleanupItem(10, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp3),
      CleanupItem(kHash4, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp3),
  };
  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, items));
  test_util::AssertRowsInUmaMetricsTable(db(), {});

  auto row1 = GetSampleMetricsRow();
  row1.time = kTimestamp1;
  row1.name_hash = kHash1;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row1));
  auto row2 = GetSampleMetricsRow();
  row2.time = kTimestamp2;
  row2.name_hash = kHash2;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row2));
  auto row3 = GetSampleMetricsRow();
  row3.time = kTimestamp3;
  row3.name_hash = kHash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row3));
  auto row4 = GetSampleMetricsRow();
  row4.time = kTimestamp4;
  row4.name_hash = kHash4;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row4));

  test_util::AssertRowsInUmaMetricsTable(db(), {row1, row2, row3, row4});

  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, empty_items));
  test_util::AssertRowsInUmaMetricsTable(db(), {row1, row2, row3, row4});

  EXPECT_TRUE(metrics_table().CleanupItems("bad-profile", items));
  test_util::AssertRowsInUmaMetricsTable(db(), {row1, row2, row3, row4});

  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, items));
  test_util::AssertRowsInUmaMetricsTable(db(), {row3, row4});

  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, items));
  test_util::AssertRowsInUmaMetricsTable(db(), {row3, row4});

  // Add more rows.
  auto row5 = GetSampleMetricsRow();
  row5.time = kTimestamp5;
  row5.name_hash = kHash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row5));
  auto row6 = GetSampleMetricsRow();
  row6.time = kTimestamp5;
  row6.name_hash = kHash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row6));
  test_util::AssertRowsInUmaMetricsTable(db(), {row3, row4, row5, row6});

  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, items));
  test_util::AssertRowsInUmaMetricsTable(db(), {row3, row4, row5, row6});

  std::vector<CleanupItem> items2{
      CleanupItem(kHash3, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp5),
      CleanupItem(10, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp3),
      CleanupItem(kHash4, 0, proto::SignalType::HISTOGRAM_VALUE, kTimestamp3),
  };
  EXPECT_TRUE(metrics_table().CleanupItems("bad-profile", items2));
  test_util::AssertRowsInUmaMetricsTable(db(), {row3, row4, row5, row6});

  EXPECT_TRUE(metrics_table().CleanupItems(kProfileId, items2));
  test_util::AssertRowsInUmaMetricsTable(db(), {row4});
}

TEST_F(UmaMetricsTableTest, DeleteBeforeTimestamp) {
  const base::Time kTimestamp1 = base::Time::Now();
  const base::Time kTimestamp2 = kTimestamp1 + base::Seconds(1);
  const base::Time kTimestamp3 = kTimestamp1 + base::Seconds(2);
  const base::Time kTimestamp4 = kTimestamp1 + base::Seconds(3);
  const base::Time kTimestamp5 = kTimestamp1 + base::Seconds(4);
  const int64_t kHash1 = 201;
  const int64_t kHash2 = 202;
  const int64_t kHash3 = 203;
  const int64_t kHash4 = 204;

  ASSERT_TRUE(metrics_table().InitTable());

  // Delete on empty table does nothing.
  std::vector<std::string> empty_set;
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(base::Time()));
  test_util::AssertRowsInUmaMetricsTable(db(), {});
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1));
  test_util::AssertRowsInUmaMetricsTable(db(), {});

  auto row1 = GetSampleMetricsRow();
  row1.time = kTimestamp1;
  row1.name_hash = kHash1;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row1));
  auto row2 = GetSampleMetricsRow();
  row2.time = kTimestamp2;
  row2.name_hash = kHash2;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row2));
  auto row3 = GetSampleMetricsRow();
  row3.time = kTimestamp3;
  row3.name_hash = kHash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row3));
  auto row4 = GetSampleMetricsRow();
  row4.time = kTimestamp4;
  row4.name_hash = kHash4;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row4));

  test_util::AssertRowsInUmaMetricsTable(db(), {row1, row2, row3, row4});

  // Delete with a time before all rows, does nothing.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(base::Time()));
  test_util::AssertRowsInUmaMetricsTable(db(), {row1, row2, row3, row4});
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1 -
                                                          base::Seconds(1)));
  test_util::AssertRowsInUmaMetricsTable(db(), {row1, row2, row3, row4});

  // Remove single row.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp1));
  test_util::AssertRowsInUmaMetricsTable(db(), {row2, row3, row4});

  // Add more rows.
  auto row5 = GetSampleMetricsRow();
  row5.time = kTimestamp5;
  row5.name_hash = kHash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row5));
  auto row6 = GetSampleMetricsRow();
  row6.time = kTimestamp5;
  row6.name_hash = kHash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row6));
  test_util::AssertRowsInUmaMetricsTable(db(), {row2, row3, row4, row5, row6});

  // Remove a bunch of rows.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp4));
  test_util::AssertRowsInUmaMetricsTable(db(), {row5, row6});

  // Insert entry with an older timestamp out of order and remove old entries
  // should still work.
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row1));
  test_util::AssertRowsInUmaMetricsTable(db(), {row5, row6, row1});
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp4));
  test_util::AssertRowsInUmaMetricsTable(db(), {row5, row6});

  // Removing all metrics.
  EXPECT_TRUE(metrics_table().DeleteEventsBeforeTimestamp(kTimestamp5));
  test_util::AssertRowsInUmaMetricsTable(db(), {});
}

TEST_F(UmaMetricsTableTest, MatchHashesTest) {
  const int64_t metric_hash1 = (1);
  const int64_t metric_hash2 = (0x9CEA8CBC362AB242);
  const int64_t metric_hash3 = (std::numeric_limits<uint64_t>::max());
  ASSERT_TRUE(metrics_table().InitTable());

  UmaMetricEntry row1 = GetSampleMetricsRow();
  row1.name_hash = metric_hash1;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row1));
  UmaMetricEntry row2 = GetSampleMetricsRow();
  row2.name_hash = metric_hash2;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row2));
  UmaMetricEntry row3 = GetSampleMetricsRow();
  row3.name_hash = metric_hash3;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileId, row3));
  UmaMetricEntry row4 = GetSampleMetricsRow();
  row4.name_hash = metric_hash1;
  EXPECT_TRUE(metrics_table().AddUmaMetric(kProfileIdOther, row4));

  auto result1 = test_util::GetUmaMetricsRowWithQuery(
      "SELECT * FROM uma_metrics WHERE metric_hash = '1' AND profile_id='123'",
      db());
  ASSERT_EQ(1u, result1.size());
  test_util::ExpectUmaRowIsEqual(row1, result1[0]);

  auto result2 = test_util::GetUmaMetricsRowWithQuery(
      "SELECT * FROM uma_metrics WHERE metric_hash = '9CEA8CBC362AB242' AND "
      "profile_id='123'",
      db());
  ASSERT_EQ(1u, result2.size());
  test_util::ExpectUmaRowIsEqual(row2, result2[0]);

  auto result3 = test_util::GetUmaMetricsRowWithQuery(
      "SELECT * FROM uma_metrics WHERE metric_hash = 'FFFFFFFFFFFFFFFF' AND "
      "profile_id='123'",
      db());
  ASSERT_EQ(1u, result3.size());
  test_util::ExpectUmaRowIsEqual(row3, result3[0]);

  auto result4 = test_util::GetUmaMetricsRowWithQuery(
      "SELECT * FROM uma_metrics WHERE metric_hash = '1' ORDER BY id", db());
  ASSERT_EQ(2u, result4.size());
  test_util::ExpectUmaRowIsEqual(row1, result4[0]);
  test_util::ExpectUmaRowIsEqual(row4, result4[1]);
}

}  // namespace segmentation_platform
