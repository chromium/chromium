// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_MOJOM_TRAITS_H_

#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<
    unexportable_keys_internals::mojom::UnexportableKeyIdDataView,
    unexportable_keys::UnexportableKeyId> {
  static const base::UnguessableToken& key_id(
      const unexportable_keys::UnexportableKeyId& input) {
    return input.value();
  }

  static bool Read(
      unexportable_keys_internals::mojom::UnexportableKeyIdDataView data,
      unexportable_keys::UnexportableKeyId* output);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_MOJOM_TRAITS_H_
