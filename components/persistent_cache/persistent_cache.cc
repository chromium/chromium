// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <memory>
#include <optional>
#include <string>

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
  if (backend->Initialize()) {
    backend_ = std::move(backend);
    base::UmaHistogramMicrosecondsTimes(
        GetFullHistogramName("BackendInitialize"), timer.Elapsed());
  }
}

PersistentCache::~PersistentCache() = default;

std::unique_ptr<Entry> PersistentCache::Find(std::string_view key) {
  if (!backend_) {
    return nullptr;
  }

  std::optional<base::ElapsedTimer> timer = MaybeGetTimerForHistogram();

  auto entry = backend_->Find(key);

  if (timer.has_value()) {
    base::UmaHistogramMicrosecondsTimes(GetFullHistogramName("Find"),
                                        timer->Elapsed());
  }

  return entry;
}

void PersistentCache::Insert(std::string_view key,
                             base::span<const uint8_t> content,
                             EntryMetadata metadata) {
  if (!backend_) {
    return;
  }

  std::optional<base::ElapsedTimer> timer = MaybeGetTimerForHistogram();

  backend_->Insert(key, content, metadata);

  if (timer.has_value()) {
    base::UmaHistogramMicrosecondsTimes(GetFullHistogramName("Insert"),
                                        timer->Elapsed());
  }
}

Backend* PersistentCache::GetBackendForTesting() {
  return backend_.get();
}

std::optional<base::ElapsedTimer> PersistentCache::MaybeGetTimerForHistogram() {
  std::optional<base::ElapsedTimer> timer;

  if (metrics_subsampler_.ShouldSample(kTimingLoggingProbability)) {
    timer.emplace();
  }

  return timer;
}

std::string PersistentCache::GetFullHistogramName(std::string_view name) const {
  const char* file_access_suffix =
      backend_->IsReadOnly() ? ".ReadOnly" : ".ReadWrite";
  return base::StrCat({"PersistentCache.", name, ".",
                       GetBackendTypeName(backend_->GetType()),
                       file_access_suffix});
}

}  // namespace persistent_cache
