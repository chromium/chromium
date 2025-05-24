// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_cache.h"

#include "base/feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"

namespace commerce {

ProductSpecificationsCache::ProductSpecificationsCache() : cache_(kCacheSize) {}
ProductSpecificationsCache::~ProductSpecificationsCache() = default;

ProductSpecificationsCache::Entry::Entry(ProductSpecifications specs) {
  this->specs = std::move(specs);
  creation_time = base::Time::Now();
}

void ProductSpecificationsCache::SetEntry(std::vector<uint64_t> cluster_ids,
                                          ProductSpecifications specs) {
  if (!base::FeatureList::IsEnabled(kProductSpecificationsCache)) {
    return;
  }

  cache_.Put(GetKey(cluster_ids), Entry(std::move(specs)));
}

const ProductSpecifications* ProductSpecificationsCache::GetEntry(
    std::vector<uint64_t> cluster_ids) {
  if (!base::FeatureList::IsEnabled(kProductSpecificationsCache)) {
    return nullptr;
  }

  auto it = cache_.Get(GetKey(cluster_ids));
  if (it == cache_.end()) {
    return nullptr;
  }

  // Remove the entry from the cache if it has expired.
  const Entry* entry = &it->second;
  if (!entry ||
      (base::Time::Now() - entry->creation_time) > kEntryInvalidationTime) {
    cache_.Erase(it);
    return nullptr;
  }

  return &entry->specs;
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
