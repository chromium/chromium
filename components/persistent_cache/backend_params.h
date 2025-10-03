// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/unsafe_shared_memory_region.h"

namespace persistent_cache {

enum class BackendType {
  kMock = 0,
  kSqlite = 1,
  kMaxValue = kSqlite,
};

// This struct contains fields necessary to configure a persistent
// cache backend.
struct COMPONENT_EXPORT(PERSISTENT_CACHE) BackendParams {
  BackendParams();
  BackendParams(BackendParams& other) = delete;
  BackendParams& operator=(const BackendParams& other) = delete;
  BackendParams(BackendParams&& other);
  BackendParams& operator=(BackendParams&& other);
  ~BackendParams();

  // TODO(crbug.com/377475540): Currently this class is deeply tied to the
  // sqlite implementation. Once the conversion to and from mojo types is
  // implemented this class should become an abstract class specialized for each
  // backend type.
  BackendType type;
  base::File db_file;
  bool db_file_is_writable = false;
  base::File journal_file;
  bool journal_file_is_writable = false;

  // The optional paths to the database and its journal files. Must be provided
  // when read-only access to read-write files may be exported from a
  // backend. May be omitted otherwise.
  base::FilePath db_file_path;
  base::FilePath journal_file_path;

  // A read-write region of memory shared by all processes accessing `db_file`.
  // This memory holds the locking state for the database. Locks held by a
  // process are not released upon abnormal termination.
  base::UnsafeSharedMemoryRegion shared_lock;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_H_
