// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_ENTRY_IMPL_H_
#define COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_ENTRY_IMPL_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"
#include "components/persistent_cache/entry.h"

namespace persistent_cache {

class SqliteBackendImpl;

class COMPONENT_EXPORT(PERSISTENT_CACHE) SqliteEntryImpl : public Entry {
 public:
  ~SqliteEntryImpl() override;

  // Entry:
  [[nodiscard]] base::span<const uint8_t> GetContentSpan() const override;
  [[nodiscard]] EntryMetadata GetMetadata() const override;

  // Use to create `unique_ptr`s of this class from `SqliteBackendImpl`.
  // Protected with `PassKey` so that only `SqliteBackendImpl` can create
  // instance. This is done allow future implementations to tie entry
  // implementation to the backend.
  static std::unique_ptr<SqliteEntryImpl> MakeUnique(
      base::PassKey<SqliteBackendImpl> passkey,
      std::string&& content,
      EntryMetadata metadata);

 private:
  FRIEND_TEST_ALL_PREFIXES(SqliteEntryTest, ConstructionTakesOwnershipOfValue);
  FRIEND_TEST_ALL_PREFIXES(SqliteEntryTest,
                           ConstructionFromEmptyValueLeadsToEmptyEntry);

  // Private constructor and destructor because entry creation should be left
  // to the associated backend.
  //
  // Takes ownership of `content`. Only this constructor is defined to avoid
  // unwanted copies as much as possible. There is precedent for copies coming
  // from disk caches leading to OOMs and this class tries to avoid the
  // situation.
  explicit SqliteEntryImpl(std::string&& content, EntryMetadata metadata);

  std::string content_;
  EntryMetadata metadata_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_SQLITE_SQLITE_ENTRY_IMPL_H_
