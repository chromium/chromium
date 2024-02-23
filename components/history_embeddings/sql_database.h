// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_

#include "base/files/file_path.h"

namespace history_embeddings {

// Wraps the Sqlite database that provides on-disk storage for History
// Embeddings component. This class is expected to live and die on a backend
// sequence owned by `HistoryEmbeddingsService`.
class SqlDatabase {
 public:
  // `storage_dir` will generally be the Profile directory.
  explicit SqlDatabase(const base::FilePath& storage_dir);
  SqlDatabase(const SqlDatabase&) = delete;
  SqlDatabase& operator=(const SqlDatabase&) = delete;
  ~SqlDatabase();
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_
