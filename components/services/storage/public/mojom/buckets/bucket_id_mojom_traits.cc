// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/buckets/bucket_id_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<storage::mojom::BucketIdDataView, storage::BucketId>::Read(
    storage::mojom::BucketIdDataView data,
    storage::BucketId* out) {
  *out = storage::BucketId(data.value());
  return true;
}

}  // namespace mojo
