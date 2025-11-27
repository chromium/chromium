// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_
#define COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/component_export.h"
#include "components/persistent_cache/mojom/persistent_cache.mojom-data-view.h"
#include "components/persistent_cache/pending_backend.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(PERSISTENT_CACHE_MOJOM_TRAITS)
    StructTraits<persistent_cache::mojom::PendingReadOnlyBackendDataView,
                 persistent_cache::PendingBackend> {
  static base::File db_file(persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, false);
    // `PendingReadOnlyBackend::db_file` is not nullable, so it is not
    // permissible to serialize `pending_backend` if it does not have a valid db
    // file handle.
    CHECK(pending_backend.sqlite_data.db_file.IsValid());
    return std::move(pending_backend.sqlite_data.db_file);
  }

  static base::File journal_file(
      persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, false);
    // `PendingReadOnlyBackend::journal_file` is not nullable, so it is not
    // permissible to serialize `pending_backend` if it does not have a valid
    // journal file handle.
    CHECK(pending_backend.sqlite_data.journal_file.IsValid());
    return std::move(pending_backend.sqlite_data.journal_file);
  }

  static base::UnsafeSharedMemoryRegion shared_lock(
      persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, false);
    // `PendingReadOnlyBackend::shared_lock` is not nullable, so it is not
    // permissible to serialize `pending_backend` if it does not have a valid
    // shared lock handle.
    CHECK(pending_backend.sqlite_data.shared_lock.IsValid());
    return std::move(pending_backend.sqlite_data.shared_lock);
  }

  static bool Read(persistent_cache::mojom::PendingReadOnlyBackendDataView data,
                   persistent_cache::PendingBackend* out_pending_backend);
};

template <>
struct COMPONENT_EXPORT(PERSISTENT_CACHE_MOJOM_TRAITS)
    StructTraits<persistent_cache::mojom::PendingReadWriteBackendDataView,
                 persistent_cache::PendingBackend> {
  static base::File db_file(persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, true);
    // `PendingReadWriteBackend::db_file` is not nullable, so it is not
    // permissible to serialize `pending_backend` if it does not have a valid db
    // file handle.
    CHECK(pending_backend.sqlite_data.db_file.IsValid());
    return std::move(pending_backend.sqlite_data.db_file);
  }

  static base::File journal_file(
      persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, true);
    // `PendingReadWriteBackend::journal_file` is not nullable, so it is not
    // permissible to serialize `pending_backend` if it does not have a valid
    // journal file handle.
    CHECK(pending_backend.sqlite_data.journal_file.IsValid());
    return std::move(pending_backend.sqlite_data.journal_file);
  }

  static base::File wal_file(
      persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, true);
    return std::move(pending_backend.sqlite_data.wal_file);
  }

  static base::UnsafeSharedMemoryRegion shared_lock(
      persistent_cache::PendingBackend& pending_backend) {
    CHECK_EQ(pending_backend.read_write, true);
    return std::move(pending_backend.sqlite_data.shared_lock);
  }

  static bool Read(
      persistent_cache::mojom::PendingReadWriteBackendDataView data,
      persistent_cache::PendingBackend* out_pending_backend);
};

}  // namespace mojo

#endif  // COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_
