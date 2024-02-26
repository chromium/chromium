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

HistoryEmbeddingsService::HistoryEmbeddingsService(
    const base::FilePath& storage_dir) {
  if (!base::FeatureList::IsEnabled(kHistoryEmbeddings)) {
    // If the feature flag is disabled, skip initialization. Note we don't also
    // check the pref here, because the pref can change at runtime.
    return;
  }

  database_ = base::SequenceBound<SqlDatabase>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      storage_dir);
}

HistoryEmbeddingsService::~HistoryEmbeddingsService() = default;

void HistoryEmbeddingsService::Shutdown() {
  database_.Reset();
}

}  // namespace history_embeddings
