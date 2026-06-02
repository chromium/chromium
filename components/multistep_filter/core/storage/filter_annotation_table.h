// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_ANNOTATION_TABLE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_ANNOTATION_TABLE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace sql {
class Database;
}

namespace multistep_filter {

struct FilterAnnotation;

// This class manages the in-memory tables to store `FilterAnnotation`
// objects and their `FilterAttribute`s within the SQLite database passed
// to the constructor. It expects the following schemas:
//
// -----------------------------------------------------------------------------
// filter_annotations        Contains filter annotations.
//
//   id                      Uniquely identifies the annotation record (UUID,
//                           primary key).
//   task_type               An identifier classifying the purpose of the
//                           annotation.
//   source_domain           The eTLD+1 domain of the source URL.
//   creation_timestamp      The timestamp when the annotation was generated in
//                           base::Time format.
// -----------------------------------------------------------------------------
// filter_annotation_attributes  Contains normalized key-value attributes for
//                               filter annotations.
//
//   annotation_id           The ID of the annotation this attribute belongs to
//                           (foreign key to filter_annotations table).
//   key                     The standardized, human-readable name for this
//                           attribute.
//   value                   The processed and cleaned value.
// -----------------------------------------------------------------------------
class FilterAnnotationTable {
 public:
  FilterAnnotationTable();

  FilterAnnotationTable(const FilterAnnotationTable&) = delete;
  FilterAnnotationTable& operator=(const FilterAnnotationTable&) = delete;

  ~FilterAnnotationTable();

  bool Init(sql::Database* db);

  // Stores a new filter annotation in the database.
  // Returns true if the operation was successful, false otherwise.
  bool StoreAnnotation(const FilterAnnotation& annotation);

  // Retrieves up to `max_count` stored filter annotations for the given
  // `task_type` created at or after `min_creation_time`.
  // The results are sorted by their creation timestamp in descending order,
  // allowing efficient access to the most recent annotation. This is used by
  // `FilterSuggestionGenerator` to provide filter recommendations for a
  // specific task type.
  std::vector<FilterAnnotation> GetAnnotationsForTaskSortedByCreationTimestamp(
      std::string_view task_type,
      size_t max_count,
      base::Time min_creation_time);

  // Deletes all annotations for the given `task_type`.
  // Returns the number of annotations deleted, or std::nullopt on failure.
  std::optional<int64_t> DeleteAnnotationsForTask(std::string_view task_type);

  // Deletes annotations for specific domains and time range.
  // If `domains` is empty, deletes data for all domains in the time range.
  // Returns the number of annotations deleted, or std::nullopt on failure.
  std::optional<int64_t> DeleteAnnotationsForDomains(
      const std::vector<std::string>& domains,
      base::Time delete_begin,
      base::Time delete_end);

  void Shutdown();

 private:
  sql::Database* db() { return db_; }

  // Helper methods for DeleteAnnotationsForDomains.
  std::optional<int64_t> DeleteAnnotationsForTimeRange(base::Time begin,
                                                       base::Time end);
  std::optional<int64_t> DeleteAnnotationsForDomain(std::string_view domain,
                                                    base::Time begin,
                                                    base::Time end);

  // Non-null, except before `Init()` and after `Shutdown()`. Effectively, this
  // means that it is non-null except during the constructor and destructor.
  // It points to the `sql::Database` instance owned by `FilterStoreBackend`.
  raw_ptr<sql::Database> db_ = nullptr;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_ANNOTATION_TABLE_H_
