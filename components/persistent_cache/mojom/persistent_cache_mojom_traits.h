// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_
#define COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/mojom/persistent_cache.mojom-data-view.h"

namespace mojo {

template <>
struct StructTraits<persistent_cache::mojom::ReadWriteBackendParamsDataView,
                    persistent_cache::BackendParams> {
  static base::File db_file(persistent_cache::BackendParams& backend_params) {
    CHECK_EQ(backend_params.type, persistent_cache::BackendType::kSqlite);
    CHECK_EQ(backend_params.db_file_is_writable, true);
    // `ReadWriteBackendParams::db_file` is not nullable, so it is not
    // permissible to serialize `backend_params` if it does not have a valid db
    // file handle.
    CHECK(backend_params.db_file.IsValid());
    return std::move(backend_params.db_file);
  }

  static base::File journal_file(
      persistent_cache::BackendParams& backend_params) {
    CHECK_EQ(backend_params.type, persistent_cache::BackendType::kSqlite);
    CHECK_EQ(backend_params.journal_file_is_writable, true);
    // `ReadWriteBackendParams::journal_file` is not nullable, so it is not
    // permissible to serialize `backend_params` if it does not have a valid
    // journal file handle.
    CHECK(backend_params.journal_file.IsValid());
    return std::move(backend_params.journal_file);
  }

  static base::UnsafeSharedMemoryRegion shared_lock(
      persistent_cache::BackendParams& backend_params) {
    CHECK_EQ(backend_params.type, persistent_cache::BackendType::kSqlite);
    // `ReadWriteBackendParams::shared_lock` is not nullable, so it is not
    // permissible to serialize `backend_params` if it does not have a valid
    // shared lock handle.
    CHECK(backend_params.shared_lock.IsValid());
    return std::move(backend_params.shared_lock);
  }

  static bool Read(persistent_cache::mojom::ReadWriteBackendParamsDataView data,
                   persistent_cache::BackendParams* out_backend_params);
};

}  // namespace mojo

#endif  // COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_
