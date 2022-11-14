// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"

namespace safe_browsing {

InMemoryHashPrefixMap::InMemoryHashPrefixMap() = default;
InMemoryHashPrefixMap::~InMemoryHashPrefixMap() = default;

void InMemoryHashPrefixMap::Clear() {
  map_.clear();
}

HashPrefixMapView InMemoryHashPrefixMap::view() const {
  HashPrefixMapView view;
  for (const auto& kv : map_)
    view.emplace(kv.first, kv.second);
  return view;
}

void InMemoryHashPrefixMap::Append(PrefixSize size, HashPrefixesView prefix) {
  map_[size].append(prefix.data(), prefix.size());
}

void InMemoryHashPrefixMap::Reserve(PrefixSize size, size_t capacity) {
  map_[size].reserve(capacity);
}

}  // namespace safe_browsing
