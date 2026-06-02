// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_store_backend.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/storage/filter_annotation_table.h"
#include "sql/database.h"

namespace multistep_filter {

FilterStoreBackend::FilterStoreBackend() = default;

FilterStoreBackend::~FilterStoreBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  filter_annotation_table_.Shutdown();
}

bool FilterStoreBackend::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::make_unique<sql::Database>(sql::Database::Tag("MultistepFilter"));

  if (!db_->OpenInMemory()) {
    return false;
  }
  return filter_annotation_table_.Init(db_.get());
}

bool FilterStoreBackend::StoreAnnotation(const FilterAnnotation& annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseInitialized()) {
    return false;
  }
  return filter_annotation_table_.StoreAnnotation(annotation);
}

std::vector<FilterAnnotation>
FilterStoreBackend::GetAnnotationsForTaskSortedByCreationTimestamp(
    std::string_view task_type,
    size_t max_count,
    base::Time min_creation_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseInitialized()) {
    return {};
  }
  return filter_annotation_table_
      .GetAnnotationsForTaskSortedByCreationTimestamp(task_type, max_count,
                                                      min_creation_time);
}

std::optional<int64_t> FilterStoreBackend::DeleteAnnotationsForTask(
    std::string_view task_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseInitialized()) {
    return std::nullopt;
  }
  return filter_annotation_table_.DeleteAnnotationsForTask(task_type);
}

std::optional<int64_t> FilterStoreBackend::DeleteAnnotationsForDomains(
    std::vector<std::string> domains,
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseInitialized()) {
    return std::nullopt;
  }
  return filter_annotation_table_.DeleteAnnotationsForDomains(
      domains, delete_begin, delete_end);
}

void FilterStoreBackend::ClearData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  filter_annotation_table_.Shutdown();
  if (db_) {
    db_->Close();
  }
  Init();
}

}  // namespace multistep_filter
