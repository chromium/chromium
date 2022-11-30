// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_LOCATOR_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_LOCATOR_MOJOM_TRAITS_H_

#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/buckets/bucket_locator.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-shared.h"

namespace mojo {

template <>
class StructTraits<storage::mojom::BucketLocatorDataView,
                   storage::BucketLocator> {
 public:
  static const storage::BucketId& id(const storage::BucketLocator& bucket) {
    return bucket.id;
  }

  static const blink::StorageKey& storage_key(
      const storage::BucketLocator& bucket) {
    return bucket.storage_key;
  }

  static const blink::mojom::StorageType& type(
      const storage::BucketLocator& bucket) {
    return bucket.type;
  }

  static bool is_default(const storage::BucketLocator& bucket) {
    return bucket.is_default;
  }

  static bool Read(storage::mojom::BucketLocatorDataView data,
                   storage::BucketLocator* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_LOCATOR_MOJOM_TRAITS_H_
