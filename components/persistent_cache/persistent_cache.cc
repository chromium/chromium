// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"

namespace persistent_cache {

const char* GetBackendTypeName(BackendType backend_type) {
  switch (backend_type) {
    case BackendType::kSqlite:
      return "SQLite";
    case BackendType::kMock:
      return "Mock";
  }
}

// static
std::unique_ptr<PersistentCache> PersistentCache::Open(
    BackendParams backend_params) {
  std::unique_ptr<Backend> backend;
  switch (backend_params.type) {
    case BackendType::kSqlite:
      backend = std::make_unique<SqliteBackendImpl>(std::move(backend_params));
      break;
    case BackendType::kMock:
      // Reserved for testing;
      NOTREACHED();
  }

  return std::make_unique<PersistentCache>(std::move(backend));
}

PersistentCache::PersistentCache(std::unique_ptr<Backend> backend) {
  CHECK(backend);

  base::ElapsedTimer timer;
  BackendType backend_type = backend->GetType();
  if (backend->Initialize()) {
    backend_ = std::move(backend);
    // TODO (https://crbug.com/377475540): Implement read-only mode.
    std::string histogram_name =
        base::StrCat({"PersistentCache.BackendInitialize.",
                      GetBackendTypeName(backend_type), ".ReadWrite"});
    base::UmaHistogramMicrosecondsTimes(histogram_name, timer.Elapsed());
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
                             base::span<const uint8_t> content,
                             EntryMetadata metadata) {
  if (!backend_) {
    return;
  }

  backend_->Insert(key, content, metadata);
}

Backend* PersistentCache::GetBackendForTesting() {
  return backend_.get();
}

}  // namespace persistent_cache
