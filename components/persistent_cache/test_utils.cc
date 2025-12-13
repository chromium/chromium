// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/test_utils.h"

#include <utility>

#include "base/test/gmock_expected_support.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/persistent_cache_collection.h"

namespace persistent_cache {

Entry::Entry() = default;
Entry::Entry(EntryMetadata metadata, base::HeapArray<uint8_t> content)
    : metadata(std::move(metadata)), content(std::move(content)) {}
Entry::Entry(Entry&& other) = default;
Entry& Entry::operator=(Entry&& other) = default;
Entry::~Entry() = default;

base::expected<std::optional<Entry>, TransactionError> FindEntry(
    PersistentCache& cache,
    std::string_view key) {
  std::optional<Entry> result;
  ASSIGN_OR_RETURN(
      auto metadata,
      cache.Find(key, [&result](size_t content_size) -> base::span<uint8_t> {
        result.emplace(EntryMetadata{},
                       base::HeapArray<uint8_t>::Uninit(content_size));
        return result->content;
      }));
  if (metadata.has_value()) {  // Cache hit.
    result->metadata = *std::move(metadata);
  }
  return result;
}

base::expected<std::optional<Entry>, TransactionError> FindEntry(
    PersistentCacheCollection& collection,
    const std::string& cache_id,
    std::string_view key) {
  std::optional<Entry> result;
  ASSIGN_OR_RETURN(
      auto metadata,
      collection.Find(
          cache_id, key, [&result](size_t content_size) -> base::span<uint8_t> {
            result.emplace(EntryMetadata{},
                           base::HeapArray<uint8_t>::Uninit(content_size));
            return result->content;
          }));
  if (metadata.has_value()) {  // Cache hit.
    result->metadata = *std::move(metadata);
  }
  return result;
}

}  // namespace persistent_cache
