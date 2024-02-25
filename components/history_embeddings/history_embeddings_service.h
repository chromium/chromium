// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "components/history_embeddings/sql_database.h"
#include "components/keyed_service/core/keyed_service.h"

namespace history_embeddings {

class HistoryEmbeddingsService : public KeyedService {
 public:
  // `storage_dir` will generally be the Profile directory.
  explicit HistoryEmbeddingsService(const base::FilePath& storage_dir);
  HistoryEmbeddingsService(const HistoryEmbeddingsService&) = delete;
  HistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) = delete;
  ~HistoryEmbeddingsService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // All storage tasks are posted to this to avoid blocking the main thread.
  scoped_refptr<base::SequencedTaskRunner> storage_task_runner_;

  // Created on this main thread, but afterwards lives and is destroyed on the
  // `storage_task_runner_` sequence.
  std::unique_ptr<SqlDatabase> database_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
