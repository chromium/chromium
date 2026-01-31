// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_traits.h"

#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

bool StructTraits<unexportable_keys_internals::mojom::UnexportableKeyIdDataView,
                  unexportable_keys::UnexportableKeyId>::
    Read(unexportable_keys_internals::mojom::UnexportableKeyIdDataView data,
         unexportable_keys::UnexportableKeyId* output) {
  base::UnguessableToken key_id;
  if (!data.ReadKeyId(&key_id)) {
    return false;
  }
  *output = unexportable_keys::UnexportableKeyId(key_id);
  return true;
}

}  // namespace mojo
