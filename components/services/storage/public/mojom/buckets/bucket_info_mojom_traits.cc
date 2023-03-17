// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/buckets/bucket_info_mojom_traits.h"

#include <string>

#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"

namespace mojo {

// static
bool StructTraits<storage::mojom::BucketInfoDataView, storage::BucketInfo>::
    Read(storage::mojom::BucketInfoDataView data, storage::BucketInfo* out) {
  blink::StorageKey storage_key;
  if (!data.ReadStorageKey(&storage_key)) {
    return false;
  }
  std::string name;
  if (!data.ReadName(&name)) {
    return false;
  }
  base::Time expiration;
  if (!data.ReadExpiration(&expiration)) {
    return false;
  }

  *out = storage::BucketInfo(storage::BucketId(data.id()), storage_key,
                             data.type(), name, expiration, data.quota(),
                             data.persistent(), data.durability());
  return true;
}

}  // namespace mojo
