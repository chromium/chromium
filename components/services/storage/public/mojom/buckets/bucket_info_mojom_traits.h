// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_INFO_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_INFO_MOJOM_TRAITS_H_

#include <string_view>

#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/mojom/buckets/bucket_info.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<storage::mojom::BucketInfoDataView, storage::BucketInfo> {
 public:
  static int64_t id(const storage::BucketInfo& bucket) {
    return bucket.id.value();
  }
  static const blink::StorageKey& storage_key(
      const storage::BucketInfo& bucket) {
    return bucket.storage_key;
  }
  static blink::mojom::StorageType type(const storage::BucketInfo& bucket) {
    return bucket.type;
  }
  static std::string_view name(const storage::BucketInfo& bucket) {
    return std::string_view(bucket.name.c_str(), bucket.name.length());
  }
  static const base::Time& expiration(const storage::BucketInfo& bucket) {
    return bucket.expiration;
  }
  static uint64_t quota(const storage::BucketInfo& bucket) {
    return bucket.quota;
  }
  static bool persistent(const storage::BucketInfo& bucket) {
    return bucket.persistent;
  }
  static blink::mojom::BucketDurability durability(
      const storage::BucketInfo& bucket) {
    return bucket.durability;
  }

  static bool Read(storage::mojom::BucketInfoDataView data,
                   storage::BucketInfo* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_INFO_MOJOM_TRAITS_H_
