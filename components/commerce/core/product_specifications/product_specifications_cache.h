// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_CACHE_H_
#define COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_CACHE_H_

#include "base/containers/lru_cache.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_types.h"

namespace commerce {

class ProductSpecificationsCache {
 public:
  ProductSpecificationsCache();
  ProductSpecificationsCache(const ProductSpecificationsCache&) = delete;
  ProductSpecificationsCache& operator=(const ProductSpecificationsCache&) =
      delete;
  ~ProductSpecificationsCache();

  void SetEntry(std::vector<uint64_t> cluster_ids, ProductSpecifications specs);
  const ProductSpecifications* GetEntry(std::vector<uint64_t> cluster_ids);

 private:
  struct Entry {
   public:
    explicit Entry(ProductSpecifications specs);
    ~Entry() = default;

    ProductSpecifications specs;
    base::Time creation_time;
  };

  friend class ProductSpecificationsCacheTest;

  using Key = std::string;
  Key GetKey(std::vector<uint64_t> cluster_ids);

  static constexpr int kCacheSize = 10;
  static constexpr base::TimeDelta kEntryInvalidationTime = base::Hours(6);

  base::HashingLRUCache<Key, Entry> cache_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_CACHE_H_
