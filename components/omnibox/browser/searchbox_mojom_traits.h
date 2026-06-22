// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_MOJOM_TRAITS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_MOJOM_TRAITS_H_

#include "components/omnibox/browser/searchbox.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/omnibox_proto/suggest_inventory.pb.h"

namespace mojo {

template <>
struct EnumTraits<searchbox::mojom::SuggestInventory,
                  omnibox::SuggestInventory> {
  static searchbox::mojom::SuggestInventory ToMojom(
      omnibox::SuggestInventory input);
  static omnibox::SuggestInventory FromMojom(
      searchbox::mojom::SuggestInventory input);
};

}  // namespace mojo

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCHBOX_MOJOM_TRAITS_H_
