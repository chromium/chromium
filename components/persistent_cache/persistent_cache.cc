// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/lock.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/metrics_util.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/sqlite/sqlite_backend_impl.h"
#include "components/persistent_cache/transaction_error.h"

namespace persistent_cache {

// static
std::unique_ptr<PersistentCache> PersistentCache::Bind(
    Client client,
    PendingBackend pending_backend) {
  // If there is ever occasion to have more than one type, branch on the type
  // here.
  if (auto backend =
          SqliteBackendImpl::Bind(std::move(pending_backend), client);
      backend) {
    return std::make_unique<PersistentCache>(client, std::move(backend));
  }

  return nullptr;
}

PersistentCache::PersistentCache(Client client,
                                 std::unique_ptr<Backend> backend)
    : client_(client), backend_(std::move(backend)) {
  CHECK(backend_);
}

PersistentCache::~PersistentCache() = default;

base::expected<std::optional<EntryMetadata>, TransactionError>
PersistentCache::Find(base::span<const uint8_t> key,
                      BufferProvider buffer_provider) {
  std::optional<base::ElapsedTimer> timer = MaybeGetTimerForHistogram();

  auto entry_metadata = backend_->Find(key, buffer_provider);

  if (timer.has_value()) {
    base::UmaHistogramMicrosecondsTimes(
        GetHistogramName(client_, "Find", !backend_->IsReadOnly()),
        timer->Elapsed());
  }

  return entry_metadata;
}

base::expected<void, TransactionError> PersistentCache::Insert(
    base::span<const uint8_t> key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  std::optional<base::ElapsedTimer> timer = MaybeGetTimerForHistogram();

  auto result = backend_->Insert(key, content, metadata);
  if (timer.has_value()) {
    base::UmaHistogramMicrosecondsTimes(GetHistogramName(client_, "Insert"),
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
  static constexpr double kTimingLoggingProbability = 0.01;
  if (metrics_subsampler_.ShouldSample(kTimingLoggingProbability)) {
    timer.emplace();
  }

  return timer;
}

}  // namespace persistent_cache
