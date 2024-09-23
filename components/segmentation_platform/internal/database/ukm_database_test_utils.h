// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_TEST_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_TEST_UTILS_H_

#include "base/strings/cstring_view.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "url/gurl.h"

namespace segmentation_platform::test_util {

// Runs a `SELECT * FROM metrics WHERE cond` type query and returns the metrics
// rows.
std::vector<UkmMetricsTable::MetricsRow> GetMetricsRowWithQuery(
    base::cstring_view query,
    sql::Database& db);

// Gets all rows from metrics table.
std::vector<UkmMetricsTable::MetricsRow> GetAllMetricsRows(sql::Database& db);

// Checks if the metrics table rows and expected rows are the same.
void AssertRowsInMetricsTable(
    sql::Database& db,
    const std::vector<UkmMetricsTable::MetricsRow>& rows);

// Checks rows are equal.
void ExpectRowIsEqual(const UkmMetricsTable::MetricsRow& row1,
                      const UkmMetricsTable::MetricsRow& row2);

// Checks if the URL table rows and the expected rows are the same.
struct UrlMatcher {
  UrlId url_id;
  GURL url;
};
void AssertUrlsInTable(sql::Database& db, const std::vector<UrlMatcher>& urls);

// Gets all rows from UMA metrics table.
std::vector<UmaMetricEntry> GetAllUmaMetrics(sql::Database& db);

// Runs a `SELECT * FROM uma_metrics WHERE cond` type query and returns the
// metrics rows.
std::vector<UmaMetricEntry> GetUmaMetricsRowWithQuery(base::cstring_view query,
                                                      sql::Database& db);

// Checks UMA rows are equal.
void ExpectUmaRowIsEqual(const UmaMetricEntry& row1,
                         const UmaMetricEntry& row2);

// Checks if the UMA metrics table rows and expected rows are the same.
void AssertRowsInUmaMetricsTable(sql::Database& db,
                                 const std::vector<UmaMetricEntry>& rows);

}  // namespace segmentation_platform::test_util

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_TEST_UTILS_H_
