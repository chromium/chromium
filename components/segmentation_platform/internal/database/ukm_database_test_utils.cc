// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_database_test_utils.h"

#include "base/strings/string_number_conversions.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::test_util {

namespace {

using ::testing::UnorderedElementsAreArray;

UkmMetricsTable::MetricsRow GetMetricsRowWithQuery(sql::Statement& statement) {
  DCHECK(statement.is_valid());
  DCHECK_EQ(statement.ColumnCount(), 8);

  UkmMetricsTable::MetricsRow row;
  row.id = MetricsRowId::FromUnsafeValue(statement.ColumnInt(0));
  row.event_timestamp = statement.ColumnTime(1);
  row.source_id = statement.ColumnInt64(2);
  row.url_id = UrlId::FromUnsafeValue(statement.ColumnInt64(3));
  row.event_id = MetricsRowEventId::FromUnsafeValue(statement.ColumnInt64(4));
  uint64_t event_hash = 0;
  if (base::HexStringToUInt64(statement.ColumnString(5), &event_hash))
    row.event_hash = UkmEventHash::FromUnsafeValue(event_hash);
  uint64_t metric_hash = 0;
  if (base::HexStringToUInt64(statement.ColumnString(6), &metric_hash))
    row.metric_hash = UkmMetricHash::FromUnsafeValue(metric_hash);
  row.metric_value = statement.ColumnInt64(7);
  return row;
}

UmaMetricEntry GetUmaMetricsRowWithQuery(sql::Statement& statement) {
  DCHECK(statement.is_valid());
  DCHECK_EQ(statement.ColumnCount(), 6);

  UmaMetricEntry row;
  row.time = statement.ColumnTime(1);
  row.type = static_cast<proto::SignalType>(statement.ColumnInt64(3));
  uint64_t metric_hash = 0;
  if (base::HexStringToUInt64(statement.ColumnString(4), &metric_hash)) {
    row.name_hash = metric_hash;
  }
  row.value = statement.ColumnInt64(5);
  return row;
}

}  // namespace

bool operator==(const UrlMatcher& row1, const UrlMatcher& row2) {
  return row1.url_id == row2.url_id && row1.url == row2.url;
}

std::vector<UkmMetricsTable::MetricsRow> GetMetricsRowWithQuery(
    base::cstring_view query,
    sql::Database& db) {
  sql::Statement statement(db.GetUniqueStatement(query));
  std::vector<UkmMetricsTable::MetricsRow> rows;
  while (statement.Step()) {
    rows.emplace_back(GetMetricsRowWithQuery(statement));
  }
  return rows;
}

std::vector<UkmMetricsTable::MetricsRow> GetAllMetricsRows(sql::Database& db) {
  return GetMetricsRowWithQuery("SELECT * FROM metrics ORDER BY id", db);
}

void AssertRowsInMetricsTable(
    sql::Database& db,
    const std::vector<UkmMetricsTable::MetricsRow>& rows) {
  auto actual_rows = GetAllMetricsRows(db);
  ASSERT_EQ(actual_rows.size(), rows.size());
  auto it1 = actual_rows.begin();
  auto it2 = rows.begin();
  for (; it1 != actual_rows.end(); ++it1, ++it2) {
    ExpectRowIsEqual(*it1, *it2);
  }
}

void ExpectRowIsEqual(const UkmMetricsTable::MetricsRow& row1,
                      const UkmMetricsTable::MetricsRow& row2) {
  EXPECT_EQ(row1.url_id, row2.url_id);
  EXPECT_EQ(row1.event_timestamp, row2.event_timestamp);
  EXPECT_EQ(row1.event_id, row2.event_id);
  EXPECT_EQ(row1.source_id, row2.source_id);
  EXPECT_EQ(row1.event_hash, row2.event_hash);
  EXPECT_EQ(row1.metric_hash, row2.metric_hash);
  EXPECT_EQ(row1.metric_value, row2.metric_value);
}

void AssertUrlsInTable(sql::Database& db, const std::vector<UrlMatcher>& urls) {
  sql::Statement statement(db.GetCachedStatement(
      SQL_FROM_HERE, "SELECT url_id, url FROM urls ORDER BY url_id"));
  std::vector<UrlMatcher> actual_rows;
  while (statement.Step()) {
    actual_rows.emplace_back(
        UrlMatcher{.url_id = static_cast<UrlId>(statement.ColumnInt64(0)),
                   .url = GURL(statement.ColumnString(1))});
  }

  EXPECT_THAT(actual_rows, UnorderedElementsAreArray(urls));
}

std::vector<UmaMetricEntry> GetUmaMetricsRowWithQuery(base::cstring_view query,
                                                      sql::Database& db) {
  sql::Statement statement(db.GetUniqueStatement(query));
  std::vector<UmaMetricEntry> rows;
  while (statement.Step()) {
    rows.emplace_back(GetUmaMetricsRowWithQuery(statement));
  }
  return rows;
}

std::vector<UmaMetricEntry> GetAllUmaMetrics(sql::Database& db) {
  return GetUmaMetricsRowWithQuery("SELECT * FROM uma_metrics ORDER BY id", db);
}

void ExpectUmaRowIsEqual(const UmaMetricEntry& row1,
                         const UmaMetricEntry& row2) {
  EXPECT_EQ(row1.name_hash, row2.name_hash);
  EXPECT_EQ(row1.time, row2.time);
  EXPECT_EQ(row1.type, row2.type);
  EXPECT_EQ(row1.value, row2.value);
}

void AssertRowsInUmaMetricsTable(sql::Database& db,
                                 const std::vector<UmaMetricEntry>& rows) {
  auto actual_rows = GetAllUmaMetrics(db);
  ASSERT_EQ(actual_rows.size(), rows.size());
  auto it1 = actual_rows.begin();
  auto it2 = rows.begin();
  for (; it1 != actual_rows.end(); ++it1, ++it2) {
    ExpectUmaRowIsEqual(*it1, *it2);
  }
}

}  // namespace segmentation_platform::test_util
