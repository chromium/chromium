// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/mojom/persistent_cache_mojom_traits.h"

#include "components/sqlite_vfs/mojom/sqlite_vfs_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<persistent_cache::mojom::PendingReadOnlyBackendDataView,
                  persistent_cache::PendingBackend>::
    Read(persistent_cache::mojom::PendingReadOnlyBackendDataView data,
         persistent_cache::PendingBackend* out_pending_backend) {
  if (!data.ReadPendingFileSet(&out_pending_backend->pending_file_set)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<persistent_cache::mojom::PendingReadWriteBackendDataView,
                  persistent_cache::PendingBackend>::
    Read(persistent_cache::mojom::PendingReadWriteBackendDataView data,
         persistent_cache::PendingBackend* out_pending_backend) {
  if (!data.ReadPendingFileSet(&out_pending_backend->pending_file_set)) {
    return false;
  }
  return true;
}

}  // namespace mojo
