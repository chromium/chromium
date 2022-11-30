// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_ID_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_ID_MOJOM_TRAITS_H_

#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/mojom/buckets/bucket_id.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<storage::mojom::BucketIdDataView, storage::BucketId> {
 public:
  static const int64_t& value(const storage::BucketId& key) {
    return key.value();
  }

  static bool IsNull(const storage::BucketId& input) { return input.is_null(); }

  static void SetToNull(storage::BucketId* out) { *out = storage::BucketId(); }

  static bool Read(storage::mojom::BucketIdDataView data,
                   storage::BucketId* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_BUCKETS_BUCKET_ID_MOJOM_TRAITS_H_
