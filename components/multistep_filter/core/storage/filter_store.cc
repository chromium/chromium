// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_store.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/storage/filter_store_backend.h"

namespace multistep_filter {

FilterStore::FilterStore() {
  scoped_refptr<base::SequencedTaskRunner> db_sequence =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  backend_ = base::SequenceBound<FilterStoreBackend>(std::move(db_sequence));
  backend_.AsyncCall(base::IgnoreResult(&FilterStoreBackend::Init));
}

FilterStore::~FilterStore() = default;

void FilterStore::StoreAnnotation(const FilterAnnotation& annotation,
                                  base::OnceCallback<void(bool)> callback) {
  backend_.AsyncCall(&FilterStoreBackend::StoreAnnotation)
      .WithArgs(annotation)
      .Then(std::move(callback));
}

void FilterStore::GetAnnotationsForTaskSortedByCreationTimestamp(
    std::string task_type,
    base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
    size_t max_count,
    base::Time min_creation_time) {
  backend_
      .AsyncCall(
          &FilterStoreBackend::GetAnnotationsForTaskSortedByCreationTimestamp)
      .WithArgs(std::move(task_type), max_count, min_creation_time)
      .Then(std::move(callback));
}

void FilterStore::ClearData() {
  backend_.AsyncCall(&FilterStoreBackend::ClearData);
}

}  // namespace multistep_filter
