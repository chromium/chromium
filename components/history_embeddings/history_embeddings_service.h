// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_

#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
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
  // The underlying SQL database, bound to a separate storage sequence.
  // This will be null if the feature flag is disabled.
  base::SequenceBound<SqlDatabase> database_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
