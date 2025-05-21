// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <memory>

#include "base/notreached.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

namespace persistent_cache {

// PersistentCache is not compatible with Fuchsia. This is a placeholder
// implementation to avoid littering the code with ifdefs.

// static
std::unique_ptr<PersistentCache> PersistentCache::Open(
    BackendParams backend_params) {
  NOTREACHED();
}

PersistentCache::PersistentCache(std::unique_ptr<Backend> backend) {
  NOTREACHED();
}

PersistentCache::~PersistentCache() {
  NOTREACHED();
}

std::unique_ptr<Entry> PersistentCache::Find(std::string_view key) {
  NOTREACHED();
}

void PersistentCache::Insert(std::string_view key,
                             base::span<const uint8_t> content,
                             EntryMetadata metadata) {
  NOTREACHED();
}

Backend* PersistentCache::GetBackendForTesting() {
  NOTREACHED();
}

}  // namespace persistent_cache
