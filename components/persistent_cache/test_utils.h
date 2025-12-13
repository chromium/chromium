// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_TEST_UTILS_H_
#define COMPONENTS_PERSISTENT_CACHE_TEST_UTILS_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/containers/heap_array.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected.h"
#include "components/persistent_cache/entry_metadata.h"
#include "components/persistent_cache/transaction_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace persistent_cache {

class PersistentCache;
class PersistentCacheCollection;

// A container for the metadata and content associated with a key in a
// PersistentCache.
struct Entry {
  Entry();
  Entry(EntryMetadata metadata, base::HeapArray<uint8_t> content);
  Entry(Entry&& other);
  Entry& operator=(Entry&& other);
  ~Entry();

  EntryMetadata metadata;
  base::HeapArray<uint8_t> content;
};

// As described in PersistentCache::Find, but returning an Entry holding the
// metadata and content for `key`.
base::expected<std::optional<Entry>, TransactionError> FindEntry(
    PersistentCache& cache,
    std::string_view key);

// As described in PersistentCacheCollection::Find, but returning an Entry
// holding the metadata and content for `key`.
base::expected<std::optional<Entry>, TransactionError> FindEntry(
    PersistentCacheCollection& collection,
    const std::string& cache_id,
    std::string_view key);

}  // namespace persistent_cache

// Returns true if the `content` of an `Entry` equals a given span of bytes.
MATCHER_P(ContentEq, s, "") {
  if (arg.content.as_span() == s) {
    return true;
  }
  *result_listener << "the content is " << base::HexEncode(arg.content);
  return false;
}

#endif  // COMPONENTS_PERSISTENT_CACHE_TEST_UTILS_H_
