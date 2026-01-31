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
  static sqlite_vfs::PendingFileSet pending_file_set(
      persistent_cache::PendingBackend& pending_backend) {
    return std::move(pending_backend.pending_file_set);
  }
  static bool Read(persistent_cache::mojom::PendingReadOnlyBackendDataView data,
                   persistent_cache::PendingBackend* out_pending_backend);
};

template <>
struct COMPONENT_EXPORT(PERSISTENT_CACHE_MOJOM_TRAITS)
    StructTraits<persistent_cache::mojom::PendingReadWriteBackendDataView,
                 persistent_cache::PendingBackend> {
  static sqlite_vfs::PendingFileSet pending_file_set(
      persistent_cache::PendingBackend& pending_backend) {
    return std::move(pending_backend.pending_file_set);
  }
  static bool Read(
      persistent_cache::mojom::PendingReadWriteBackendDataView data,
      persistent_cache::PendingBackend* out_pending_backend);
};

}  // namespace mojo

#endif  // COMPONENTS_PERSISTENT_CACHE_MOJOM_PERSISTENT_CACHE_MOJOM_TRAITS_H_
