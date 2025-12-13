// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/transaction_error.h"

namespace persistent_cache {

const char* GetBackendTypeName(BackendType backend_type) {
  switch (backend_type) {
    case BackendType::kSqlite:
      return "SQLite";
  }
}

// static
std::unique_ptr<PersistentCache> PersistentCache::Bind(
    PendingBackend pending_backend) {
  // If there is ever occasion to have more than one type, branch on the type
  // here.
  if (auto backend = SqliteBackendImpl::Bind(std::move(pending_backend));
      backend) {
    return std::make_unique<PersistentCache>(std::move(backend));
  }

  return nullptr;
}

PersistentCache::PersistentCache(std::unique_ptr<Backend> backend)
    : backend_(std::move(backend)) {
  CHECK(backend_);
}

PersistentCache::~PersistentCache() = default;

base::expected<std::optional<EntryMetadata>, TransactionError>
PersistentCache::Find(std::string_view key, BufferProvider buffer_provider) {
  std::optional<base::ElapsedTimer> timer = MaybeGetTimerForHistogram();

  auto entry_metadata = backend_->Find(key, buffer_provider);

  if (timer.has_value()) {
    base::UmaHistogramMicrosecondsTimes(GetFullHistogramName("Find"),
                                        timer->Elapsed());
  }

  return entry_metadata;
}

base::expected<void, TransactionError> PersistentCache::Insert(
    std::string_view key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  std::optional<base::ElapsedTimer> timer = MaybeGetTimerForHistogram();

  auto result = backend_->Insert(key, content, metadata);
  if (timer.has_value()) {
    base::UmaHistogramMicrosecondsTimes(GetFullHistogramName("Insert"),
                                        timer->Elapsed());
  }

  return result;
}

LockState PersistentCache::Abandon() {
  return backend_->Abandon();
}

Backend* PersistentCache::GetBackendForTesting() {
  return backend_.get();
}

std::optional<base::ElapsedTimer> PersistentCache::MaybeGetTimerForHistogram() {
  std::optional<base::ElapsedTimer> timer;

  base::AutoLock lock(metrics_subsampler_lock_);
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
