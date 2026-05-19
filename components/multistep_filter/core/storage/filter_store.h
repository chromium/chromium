// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_STORE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_STORE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"

namespace multistep_filter {

struct FilterAnnotation;
class FilterStoreBackend;

// The UI-thread bridge to the `FilterStoreBackend`.
// This store provides asynchronous methods to store and retrieve
// annotations. It owns the background backend via base::SequenceBound,
// ensuring all SQLite operations happen off the main UI thread.
class FilterStore {
 public:
  FilterStore();

  FilterStore(const FilterStore&) = delete;
  FilterStore& operator=(const FilterStore&) = delete;

  // Virtual for testing.
  virtual ~FilterStore();

  // Asynchronously stores a new annotation in the background database.
  // The callback is called with true if the operation was successful, false
  // otherwise. Virtual for testing.
  virtual void StoreAnnotation(const FilterAnnotation& annotation,
                               base::OnceCallback<void(bool)> callback);

  // Asynchronously retrieves annotations for a specific task type.
  // Only annotations created at or after `min_creation_time` are returned.
  // The callback is guaranteed to run safely on the calling sequence (UI
  // thread).
  void GetAnnotationsForTaskSortedByCreationTimestamp(
      std::string task_type,
      base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
      size_t max_count,
      base::Time min_creation_time);

  // Wipes the in-memory database mid-session (e.g., when the user clears
  // browsing data).
  void ClearData();

 private:
  base::SequenceBound<FilterStoreBackend> backend_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_STORAGE_FILTER_STORE_H_
