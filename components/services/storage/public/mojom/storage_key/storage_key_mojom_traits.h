// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_

#include "components/services/storage/public/cpp/storage_key.h"
#include "components/services/storage/public/mojom/storage_key/storage_key.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace url {
class Origin;
}  // namespace url

namespace mojo {

template <>
class StructTraits<storage::mojom::StorageKeyDataView, storage::StorageKey> {
 public:
  static const url::Origin& origin(const storage::StorageKey& key) {
    return key.origin();
  }

  static bool Read(storage::mojom::StorageKeyDataView data,
                   storage::StorageKey* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_MOJOM_STORAGE_KEY_STORAGE_KEY_MOJOM_TRAITS_H_
