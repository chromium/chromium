// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include "base/notreached.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/entry.h"

namespace persistent_cache {

// static
std::unique_ptr<PersistentCache> PersistentCache::Open(
    const BackendParams& backend_params) {
  std::unique_ptr<Backend> backend;
  switch (backend_params.type) {
    case BackendType::kMock:
      // Reserved for testing;
      NOTREACHED();
  }

  return std::make_unique<PersistentCache>(std::move(backend));
}

PersistentCache::PersistentCache(std::unique_ptr<Backend> backend) {
  CHECK(backend);

  if (backend->Initialize()) {
    backend_ = std::move(backend);
  }
}

PersistentCache::~PersistentCache() = default;

std::unique_ptr<Entry> PersistentCache::Find(std::string_view key) {
  if (!backend_) {
    return nullptr;
  }

  return backend_->Find(key);
}

void PersistentCache::Insert(std::string_view key,
                             base::span<const uint8_t> content) {
  if (!backend_) {
    return;
  }

  backend_->Insert(key, content);
}

Backend* PersistentCache::GetBackendForTesting() {
  return backend_.get();
}

}  // namespace persistent_cache
