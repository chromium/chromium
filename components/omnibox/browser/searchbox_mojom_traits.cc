// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/searchbox_mojom_traits.h"

namespace mojo {

// static
searchbox::mojom::SuggestInventory EnumTraits<
    searchbox::mojom::SuggestInventory,
    omnibox::SuggestInventory>::ToMojom(omnibox::SuggestInventory input) {
  switch (input) {
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT:
      return searchbox::mojom::SuggestInventory::kDefault;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL:
      return searchbox::mojom::SuggestInventory::kTravel;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_IO_HP_TAKEOVER:
      return searchbox::mojom::SuggestInventory::kAimTakeover;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_IMG_GEN_IO_HP_TAKEOVER:
      return searchbox::mojom::SuggestInventory::kImageGenTakeover;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_CONVERSATION_STARTERS:
      return searchbox::mojom::SuggestInventory::kConversationStarters;
    default:
      return searchbox::mojom::SuggestInventory::kDefault;
  }
}

// static
omnibox::SuggestInventory
EnumTraits<searchbox::mojom::SuggestInventory, omnibox::SuggestInventory>::
    FromMojom(searchbox::mojom::SuggestInventory input) {
  switch (input) {
    case searchbox::mojom::SuggestInventory::kDefault:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT;
    case searchbox::mojom::SuggestInventory::kTravel:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL;
    case searchbox::mojom::SuggestInventory::kAimTakeover:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_IO_HP_TAKEOVER;
    case searchbox::mojom::SuggestInventory::kImageGenTakeover:
      return omnibox::SuggestInventory::
          SUGGEST_INVENTORY_IMG_GEN_IO_HP_TAKEOVER;
    case searchbox::mojom::SuggestInventory::kConversationStarters:
      return omnibox::SuggestInventory::
          SUGGEST_INVENTORY_AIM_CONVERSATION_STARTERS;
    default:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT;
  }
}

}  // namespace mojo
