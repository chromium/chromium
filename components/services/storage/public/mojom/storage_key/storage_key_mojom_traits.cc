// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/storage_key/storage_key_mojom_traits.h"

#include "url/origin.h"

namespace mojo {

// static
bool StructTraits<storage::mojom::StorageKeyDataView, storage::StorageKey>::
    Read(storage::mojom::StorageKeyDataView data, storage::StorageKey* out) {
  url::Origin origin;
  if (!data.ReadOrigin(&origin))
    return false;

  *out = storage::StorageKey(origin);
  return true;
}

}  // namespace mojo
