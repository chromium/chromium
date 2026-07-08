// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/fake_dom_storage_database_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_histogram_helper.h"
#include "components/services/storage/dom_storage/test_support/fake_dom_storage_database.h"

namespace storage {

FakeDomStorageDatabaseFactory::FakeDomStorageDatabaseFactory(
    int num_open_failures,
    int num_destroy_failures)
    : num_open_failures_(num_open_failures),
      num_destroy_failures_(num_destroy_failures),
      // base::Unretained is safe because `this` owns `scoped_factory_` and
      // destructs it first.
      scoped_factory_(
          base::BindRepeating(&FakeDomStorageDatabaseFactory::Open,
                              base::Unretained(this))) {}

FakeDomStorageDatabaseFactory::FakeDomStorageDatabaseFactory(
    int num_open_failures,
    DestroyResultCallback custom_destroy_result)
    : num_open_failures_(num_open_failures),
      num_destroy_failures_(0),
      custom_destroy_result_(std::move(custom_destroy_result)),
      // base::Unretained is safe because `this` owns `scoped_factory_` and
      // destructs it first.
      scoped_factory_(base::BindRepeating(&FakeDomStorageDatabaseFactory::Open,
                                          base::Unretained(this))) {}

FakeDomStorageDatabaseFactory::~FakeDomStorageDatabaseFactory() = default;

void FakeDomStorageDatabaseFactory::Open(
    StorageType,
    const base::FilePath& dir_to_open,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&,
    const base::FilePath& dir_to_destroy,
    DomStorageDatabaseFactory::OpenResultCallback callback) {
  std::optional<DomStorageDatabaseFactory::DestroyOutcome> destroy_outcome;
  if (!dir_to_destroy.empty()) {
    destroy_outcome = DomStorageDatabaseFactory::DestroyOutcome{
        NextDestroyResult(), DatabaseMetricsType::kOnDisk};
  }

  DbStatus open_status = open_count_++ < num_open_failures_
                             ? DbStatus::Corruption("test")
                             : DbStatus::OK();
  DomStorageDatabaseFactory::OpenResult result;
  result.SetDatabase(GetTaskRunnerForDb(dir_to_open),
                     std::make_unique<FakeDomStorageDatabase>(open_status));
  result.metrics_type = dir_to_open.empty() ? DatabaseMetricsType::kInMemory
                                            : DatabaseMetricsType::kOnDisk;
  result.open_status = open_status;
  result.destroy_outcome = std::move(destroy_outcome);
  std::move(callback).Run(std::move(result));
}

DbStatus FakeDomStorageDatabaseFactory::NextDestroyResult() {
  if (custom_destroy_result_) {
    return custom_destroy_result_.Run();
  }
  return destroy_count_++ < num_destroy_failures_ ? DbStatus::IOError("test")
                                                  : DbStatus::OK();
}

}  // namespace storage
