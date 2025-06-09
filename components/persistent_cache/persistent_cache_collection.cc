// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache_collection.h"

#include <memory>

#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/backend_params_manager.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"

namespace {
constexpr size_t kLruCacheCapacity = 100;
}  // namespace

namespace persistent_cache {

PersistentCacheCollection::PersistentCacheCollection(
    std::unique_ptr<BackendParamsManager> backend_params_manager)
    : backend_params_manager_(std::move(backend_params_manager)),
      persistent_caches_(kLruCacheCapacity) {}
PersistentCacheCollection::~PersistentCacheCollection() = default;

std::unique_ptr<Entry> PersistentCacheCollection::Find(
    const std::string& cache_id,
    std::string_view key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetOrCreateCache(cache_id)->Find(key);
}

void PersistentCacheCollection::Insert(const std::string& cache_id,
                                       std::string_view key,
                                       base::span<const uint8_t> content,
                                       EntryMetadata metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetOrCreateCache(cache_id)->Insert(key, content, metadata);
}

void PersistentCacheCollection::ClearForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  persistent_caches_.Clear();
}

void PersistentCacheCollection::DeleteAllFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Delete all managed parsistent caches so they don't hold on to files or
  // prevent their deletion.
  persistent_caches_.Clear();

  backend_params_manager_->DeleteAllFiles();
}

PersistentCache* PersistentCacheCollection::GetOrCreateCache(
    const std::string& cache_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = persistent_caches_.Get(cache_id);

  // If the cache is already created.
  if (it != persistent_caches_.end()) {
    return it->second.get();
  }

  // Create the cache
  // TODO(crbug.com/377475540): Currently this class is deeply tied to the
  // sqlite implementation. Once the conversion to and from mojo types is
  // implemented this class should get a way to select the desired backend type.
  // TODO: Allow choosing the desired access rights.
  auto inserted_it = persistent_caches_.Put(
      cache_id,
      PersistentCache::Open(backend_params_manager_->GetOrCreateParamsSync(
          BackendType::kSqlite, cache_id,
          BackendParamsManager::AccessRights::kReadWrite)));
  return inserted_it->second.get();
}

}  // namespace persistent_cache
