// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_store_backend.h"

#include <stddef.h>

#include <memory>
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

void FilterStoreBackend::ClearData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  filter_annotation_table_.Shutdown();
  if (db_) {
    db_->Close();
  }
  Init();
}

}  // namespace multistep_filter
