// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_cache.h"

namespace commerce {

ProductSpecificationsCache::ProductSpecificationsCache() : cache_(kCacheSize) {}
ProductSpecificationsCache::~ProductSpecificationsCache() = default;

void ProductSpecificationsCache::SetEntry(std::vector<uint64_t> cluster_ids,
                                          ProductSpecifications specs) {
  cache_.Put(GetKey(cluster_ids), std::move(specs));
}

const ProductSpecifications* ProductSpecificationsCache::GetEntry(
    std::vector<uint64_t> cluster_ids) {
  auto it = cache_.Get(GetKey(cluster_ids));
  if (it == cache_.end()) {
    return nullptr;
  }

  return &it->second;
}

ProductSpecificationsCache::Key ProductSpecificationsCache::GetKey(
    std::vector<uint64_t> cluster_ids) {
  std::sort(cluster_ids.begin(), cluster_ids.end());
  std::string key;
  for (uint64_t cluster_id : cluster_ids) {
    key += base::NumberToString(cluster_id);
  }
  return key;
}

}  // namespace commerce
