// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/history_embeddings_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/sql_database.h"

namespace history_embeddings {

namespace {

void ShutdownSqlDatabaseOnStorageSequence(
    std::unique_ptr<SqlDatabase> database) {
  // Does nothing right now. Just takes ownership of `database`, and then
  // it falls out of scope and is destroyed right after.
}

}  // namespace

HistoryEmbeddingsService::HistoryEmbeddingsService(
    const base::FilePath& storage_dir) {
  if (!base::FeatureList::IsEnabled(kHistoryEmbeddings)) {
    // If the feature flag is disabled, skip initialization. Note we don't also
    // check the pref here, because the pref can change at runtime.
    return;
  }

  storage_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Created on the main thread for ease, but subsequent usage should all be
  // on the `storage_task_runner_`.
  database_ = std::make_unique<SqlDatabase>(storage_dir);
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

void HistoryEmbeddingsService::Shutdown() {
  if (storage_task_runner_) {
    storage_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ShutdownSqlDatabaseOnStorageSequence,
                                  std::move(database_)));
    storage_task_runner_.reset();
  }
}

}  // namespace history_embeddings
