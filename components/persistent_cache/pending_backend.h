// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PENDING_BACKEND_H_
#define COMPONENTS_PERSISTENT_CACHE_PENDING_BACKEND_H_

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/memory/unsafe_shared_memory_region.h"

namespace persistent_cache {

// The state required to connect to a backend. Instances are created via a
// BackendStorage and are bound by a PersistentCache to establish a connection.
struct COMPONENT_EXPORT(PERSISTENT_CACHE) PendingBackend {
  PendingBackend();
  PendingBackend(PendingBackend&&);
  PendingBackend& operator=(PendingBackend&&);
  ~PendingBackend();

  struct COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteData {
    SqliteData();
    SqliteData(SqliteData&&);
    SqliteData& operator=(SqliteData&&);
    ~SqliteData();

    base::File db_file;
    base::File journal_file;

    // An optional write-ahead log file, specified only if this backend uses a
    // write-ahead log rather than a rollback journal.
    base::File wal_file;

    // An optional read-write region of memory shared by all processes accessing
    // `db_file_` that holds the locking state for the database. Locks are not
    // released upon abnormal process termination.
    base::UnsafeSharedMemoryRegion shared_lock;
  };

  // The data specific to the SQLite backend. If there is ever occasion to have
  // more than one type, use std::variant<> to hold the data for each.
  SqliteData sqlite_data;

  // False if this backend is read-only, true if read/write.
  bool read_write = false;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PENDING_BACKEND_H_
