// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"

namespace history_embeddings {

SqlDatabase::SqlDatabase(const base::FilePath& storage_dir) {}

SqlDatabase::~SqlDatabase() = default;

}  // namespace history_embeddings
