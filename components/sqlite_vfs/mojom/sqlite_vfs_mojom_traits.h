// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_MOJOM_SQLITE_VFS_MOJOM_TRAITS_H_
#define COMPONENTS_SQLITE_VFS_MOJOM_SQLITE_VFS_MOJOM_TRAITS_H_

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/component_export.h"
#include "components/sqlite_vfs/mojom/sqlite_vfs.mojom-data-view.h"
#include "components/sqlite_vfs/pending_file_set.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(SQLITE_VFS_MOJOM_TRAITS)
    StructTraits<sqlite_vfs::mojom::PendingReadOnlyFileSetDataView,
                 sqlite_vfs::PendingFileSet> {
  static base::File db_file(sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, false);
    // `PendingReadOnlyFileSet::db_file` is not nullable, so it is not
    // permissible to serialize `pending_file_set` if it does not have a valid
    // db file handle.
    CHECK(pending_file_set.db_file.IsValid());
    return std::move(pending_file_set.db_file);
  }

  static base::File journal_file(sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, false);
    // `PendingReadOnlyFileSet::journal_file` is not nullable, so it is not
    // permissible to serialize `pending_file_set` if it does not have a valid
    // journal file handle.
    CHECK(pending_file_set.journal_file.IsValid());
    return std::move(pending_file_set.journal_file);
  }

  static base::UnsafeSharedMemoryRegion shared_lock(
      sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, false);
    // `PendingReadOnlyFileSet::shared_lock` is not nullable, so it is not
    // permissible to serialize `pending_file_set` if it does not have a valid
    // shared lock handle.
    CHECK(pending_file_set.shared_lock.IsValid());
    return std::move(pending_file_set.shared_lock);
  }

  static bool Read(sqlite_vfs::mojom::PendingReadOnlyFileSetDataView data,
                   sqlite_vfs::PendingFileSet* out_pending_file_set);
};

template <>
struct COMPONENT_EXPORT(SQLITE_VFS_MOJOM_TRAITS)
    StructTraits<sqlite_vfs::mojom::PendingReadWriteFileSetDataView,
                 sqlite_vfs::PendingFileSet> {
  static base::File db_file(sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, true);
    // `PendingReadWriteFileSet::db_file` is not nullable, so it is not
    // permissible to serialize `pending_file_set` if it does not have a valid
    // db file handle.
    CHECK(pending_file_set.db_file.IsValid());
    return std::move(pending_file_set.db_file);
  }

  static base::File journal_file(sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, true);
    // `PendingReadWriteFileSet::journal_file` is not nullable, so it is not
    // permissible to serialize `pending_file_set` if it does not have a valid
    // journal file handle.
    CHECK(pending_file_set.journal_file.IsValid());
    return std::move(pending_file_set.journal_file);
  }

  static base::File wal_file(sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, true);
    return std::move(pending_file_set.wal_file);
  }

  static base::UnsafeSharedMemoryRegion shared_lock(
      sqlite_vfs::PendingFileSet& pending_file_set) {
    CHECK_EQ(pending_file_set.read_write, true);
    return std::move(pending_file_set.shared_lock);
  }

  static bool Read(sqlite_vfs::mojom::PendingReadWriteFileSetDataView data,
                   sqlite_vfs::PendingFileSet* out_pending_file_set);
};

}  // namespace mojo

#endif  // COMPONENTS_SQLITE_VFS_MOJOM_SQLITE_VFS_MOJOM_TRAITS_H_
