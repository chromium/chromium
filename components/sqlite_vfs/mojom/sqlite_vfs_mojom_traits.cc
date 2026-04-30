// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/mojom/sqlite_vfs_mojom_traits.h"

#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/read_only_file_mojom_traits.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<sqlite_vfs::mojom::PendingReadOnlyFileSetDataView,
                  sqlite_vfs::PendingFileSet>::
    Read(sqlite_vfs::mojom::PendingReadOnlyFileSetDataView data,
         sqlite_vfs::PendingFileSet* out_pending_file_set) {
  if (!data.ReadDbFile(&out_pending_file_set->db_file)) {
    return false;
  }
  if (!data.ReadJournalFile(&out_pending_file_set->journal_file)) {
    return false;
  }
  if (!data.ReadWalFile(&out_pending_file_set->wal_file)) {
    return false;
  }
  if (!data.ReadWalIndexFile(&out_pending_file_set->wal_index_file)) {
    return false;
  }
  if (!data.ReadSharedLock(&out_pending_file_set->shared_lock)) {
    return false;
  }
  out_pending_file_set->read_write = false;
  out_pending_file_set->wal_mode = data.wal_mode();
  return true;
}

// static
bool StructTraits<sqlite_vfs::mojom::PendingReadWriteFileSetDataView,
                  sqlite_vfs::PendingFileSet>::
    Read(sqlite_vfs::mojom::PendingReadWriteFileSetDataView data,
         sqlite_vfs::PendingFileSet* out_pending_file_set) {
  if (!data.ReadDbFile(&out_pending_file_set->db_file)) {
    return false;
  }
  if (!data.ReadJournalFile(&out_pending_file_set->journal_file)) {
    return false;
  }
  if (!data.ReadWalFile(&out_pending_file_set->wal_file)) {
    return false;
  }
  if (!data.ReadWalIndexFile(&out_pending_file_set->wal_index_file)) {
    return false;
  }
#if !BUILDFLAG(IS_WIN)
  if (!data.ReadWalIndexFileReadOnly(
          &out_pending_file_set->wal_index_file_read_only)) {
    return false;
  }
#endif
  if (!data.ReadSharedLock(&out_pending_file_set->shared_lock)) {
    return false;
  }
  out_pending_file_set->read_write = true;
  out_pending_file_set->wal_mode = data.wal_mode();
  return true;
}

}  // namespace mojo
