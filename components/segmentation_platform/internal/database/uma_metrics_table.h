// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UMA_METRICS_TABLE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UMA_METRICS_TABLE_H_

#include <string>

#include "base/sequence_checker.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/public/database_client.h"
#include "sql/database.h"

namespace segmentation_platform {

// Class to handle UMA metrics table updates in SQL database.
class UmaMetricsTable {
 public:
  static constexpr char kTableName[] = "uma_metrics";

  explicit UmaMetricsTable(sql::Database* db);
  ~UmaMetricsTable();

  UmaMetricsTable(const UmaMetricsTable&) = delete;
  UmaMetricsTable& operator=(const UmaMetricsTable&) = delete;

  // Creates the metrics table if it doesn't exist.
  bool InitTable();

  // Adds the given row to the metrics table.
  bool AddUmaMetric(const std::string& profile_id, const UmaMetricEntry& row);

  // Delete all metrics older than the provided `time`;
  bool DeleteEventsBeforeTimestamp(base::Time time);

  bool CleanupItems(const std::string& profile_id,
                    const std::vector<CleanupItem>& cleanup_items);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<sql::Database> db_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UMA_METRICS_TABLE_H_
