// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/mojom/persistent_cache_mojom_traits.h"

#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<persistent_cache::mojom::ReadWriteBackendParamsDataView,
                  persistent_cache::BackendParams>::
    Read(persistent_cache::mojom::ReadWriteBackendParamsDataView data,
         persistent_cache::BackendParams* out_backend_params) {
  out_backend_params->type = persistent_cache::BackendType::kSqlite;
  if (!data.ReadDbFile(&out_backend_params->db_file)) {
    return false;
  }
  out_backend_params->db_file_is_writable = true;
  if (!data.ReadJournalFile(&out_backend_params->journal_file)) {
    return false;
  }
  out_backend_params->journal_file_is_writable = true;
  if (!data.ReadSharedLock(&out_backend_params->shared_lock)) {
    return false;
  }
  return true;
}

}  // namespace mojo
