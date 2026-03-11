// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_annotation_table.h"

#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "sql/database.h"

namespace multistep_filter {

FilterAnnotationTable::FilterAnnotationTable() = default;
FilterAnnotationTable::~FilterAnnotationTable() = default;

bool FilterAnnotationTable::Init(sql::Database* db) {
  CHECK(db);
  db_ = db;

  // TODO(crbug.com/483670345): Create the "filter_annotations" and
  // "filter_annotation_attributes" tables.
  NOTIMPLEMENTED();
  return true;
}

bool FilterAnnotationTable::StoreAnnotation(
    const FilterAnnotation& annotation) {
  // TODO(crbug.com/483670345): Insert a new record into the tables.
  NOTIMPLEMENTED();
  return true;
}

std::vector<FilterAnnotation>
FilterAnnotationTable::GetAnnotationsForTaskSortedByCreationTimestamp(
    std::string_view task_type,
    size_t max_count) {
  // TODO(crbug.com/483670345): Retrieve up to `max_count` records from the
  // tables that match the given `task_type`.
  NOTIMPLEMENTED();
  return {};
}

void FilterAnnotationTable::Shutdown() {
  db_ = nullptr;
}

}  // namespace multistep_filter
