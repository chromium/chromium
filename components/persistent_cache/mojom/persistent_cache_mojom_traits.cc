// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/mojom/persistent_cache_mojom_traits.h"

#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/read_only_file_mojom_traits.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<persistent_cache::mojom::PendingReadOnlyBackendDataView,
                  persistent_cache::PendingBackend>::
    Read(persistent_cache::mojom::PendingReadOnlyBackendDataView data,
         persistent_cache::PendingBackend* out_pending_backend) {
  auto& sqlite_data = out_pending_backend->sqlite_data;
  if (!data.ReadDbFile(&sqlite_data.db_file)) {
    return false;
  }
  if (!data.ReadJournalFile(&sqlite_data.journal_file)) {
    return false;
  }
  if (!data.ReadSharedLock(&sqlite_data.shared_lock)) {
    return false;
  }
  out_pending_backend->read_write = false;
  return true;
}

// static
bool StructTraits<persistent_cache::mojom::PendingReadWriteBackendDataView,
                  persistent_cache::PendingBackend>::
    Read(persistent_cache::mojom::PendingReadWriteBackendDataView data,
         persistent_cache::PendingBackend* out_pending_backend) {
  auto& sqlite_data = out_pending_backend->sqlite_data;
  if (!data.ReadDbFile(&sqlite_data.db_file)) {
    return false;
  }
  if (!data.ReadJournalFile(&sqlite_data.journal_file)) {
    return false;
  }
  if (!data.ReadWalFile(&sqlite_data.wal_file)) {
    return false;
  }
  if (!data.ReadSharedLock(&sqlite_data.shared_lock)) {
    return false;
  }
  out_pending_backend->read_write = true;
  return true;
}

}  // namespace mojo
