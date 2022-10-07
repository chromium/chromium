// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/buckets/bucket_locator_mojom_traits.h"

#include "components/services/storage/public/mojom/buckets/bucket_id_mojom_traits.h"
#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    storage::mojom::BucketLocatorDataView,
    storage::BucketLocator>::Read(storage::mojom::BucketLocatorDataView data,
                                  storage::BucketLocator* out) {
  storage::BucketId id;
  if (!data.ReadId(&id))
    return false;

  blink::StorageKey storage_key;
  if (!data.ReadStorageKey(&storage_key))
    return false;

  blink::mojom::StorageType type;
  if (!data.ReadType(&type))
    return false;

  bool is_default = data.is_default();

  if (id.is_null()) {
    CHECK(is_default);
  }

  *out = storage::BucketLocator(id, std::move(storage_key), type, is_default);
  return true;
}

}  // namespace mojo
