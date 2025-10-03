// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_BACKEND_STORAGE_DELEGATE_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_BACKEND_STORAGE_DELEGATE_H_

#include "base/component_export.h"
#include "components/persistent_cache/backend_storage.h"

namespace persistent_cache::sqlite {

// A delegate that emits SqliteBackendImpl instances and manages their
// storage.
class COMPONENT_EXPORT(PERSISTENT_CACHE) BackendStorageDelegate
    : public BackendStorage::Delegate {
 public:
  // BackendStorage::Delegate:

  // Returns a SqliteBackendImpl backend with read-write access to `base_name`.
  std::unique_ptr<Backend> MakeBackend(
      const base::FilePath& directory,
      const base::FilePath& base_name) override;

  // Returns the basename of `file` without its extension if its extension is
  // ".db".
  base::FilePath GetBaseName(const base::FilePath& file) override;

  // Deletes all SQLite files for `base_name` in `directory` (e.g., the .db and
  // .journal files).
  int64_t DeleteFiles(const base::FilePath& directory,
                      const base::FilePath& base_name) override;
};

}  // namespace persistent_cache::sqlite

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_BACKEND_STORAGE_DELEGATE_H_
