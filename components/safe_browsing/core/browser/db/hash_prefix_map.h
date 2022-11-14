// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_

#include <string>
#include <unordered_map>

#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

namespace safe_browsing {

// The sorted list of hash prefixes.
using HashPrefixes = std::string;

using HashPrefixesView = base::StringPiece;
using HashPrefixMapView = std::unordered_map<PrefixSize, HashPrefixesView>;

// Stores the list of sorted hash prefixes, by size.
// For instance: {4: ["abcd", "bcde", "cdef", "gggg"], 5: ["fffff"]}
class HashPrefixMap {
 public:
  virtual ~HashPrefixMap() = default;

  // Clears the underlying map.
  virtual void Clear() = 0;

  // Returns a read-only view of the data stored in this map.
  virtual HashPrefixMapView view() const = 0;

  // Appends |prefix| to the prefix list of size |size|.
  virtual void Append(PrefixSize size, HashPrefixesView prefix) = 0;

  // Reserves space for the prefix list of size |size|.
  virtual void Reserve(PrefixSize size, size_t capacity) = 0;
};

// An in-memory implementation of HashPrefixMap.
class InMemoryHashPrefixMap : public HashPrefixMap {
 public:
  InMemoryHashPrefixMap();
  ~InMemoryHashPrefixMap() override;

  void Clear() override;
  HashPrefixMapView view() const override;
  void Append(PrefixSize size, HashPrefixesView prefix) override;
  void Reserve(PrefixSize size, size_t capacity) override;

 private:
  std::unordered_map<PrefixSize, HashPrefixes> map_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
