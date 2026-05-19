// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_STORE_BACKEND_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_STORE_BACKEND_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/storage/filter_annotation_table.h"
#include "sql/database.h"

namespace multistep_filter {

struct FilterAnnotation;

// Backend class that runs on the DB sequence and manages the
// in-memory SQLite database.
class FilterStoreBackend {
 public:
  FilterStoreBackend();

  FilterStoreBackend(const FilterStoreBackend&) = delete;
  FilterStoreBackend& operator=(const FilterStoreBackend&) = delete;

  ~FilterStoreBackend();

  // Initializes the in-memory SQLite database and sets up the table schemas.
  // Returns true if the database was successfully opened and tables created.
  bool Init();

  // Stores a new annotation. Called by the UI thread via SequenceBound.
  bool StoreAnnotation(const FilterAnnotation& annotation);

  // Retrieves annotations for a specific task type created at or after
  // `min_creation_time`. Called by the UI thread via SequenceBound, returning
  // data via callback.
  std::vector<FilterAnnotation> GetAnnotationsForTaskSortedByCreationTimestamp(
      std::string_view task_type,
      size_t max_count,
      base::Time min_creation_time);

  // Clears all data from the database.
  void ClearData();

 private:
  bool IsDatabaseInitialized() { return db_ && db_->is_open(); }

  std::unique_ptr<sql::Database> db_;
  FilterAnnotationTable filter_annotation_table_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_STORE_BACKEND_H_
