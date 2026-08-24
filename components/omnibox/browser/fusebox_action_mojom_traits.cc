// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fusebox_action_mojom_traits.h"

#include <optional>

namespace mojo {

// static
fusebox_action::mojom::SuggestInventory EnumTraits<
    fusebox_action::mojom::SuggestInventory,
    omnibox::SuggestInventory>::ToMojom(omnibox::SuggestInventory input) {
  switch (input) {
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT:
      return fusebox_action::mojom::SuggestInventory::kDefault;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL:
      return fusebox_action::mojom::SuggestInventory::kTravel;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_IO_HP_TAKEOVER:
      return fusebox_action::mojom::SuggestInventory::kAimTakeover;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_IMG_GEN_IO_HP_TAKEOVER:
      return fusebox_action::mojom::SuggestInventory::kImageGenTakeover;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_CONVERSATION_STARTERS:
      return fusebox_action::mojom::SuggestInventory::kConversationStarters;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM:
      return fusebox_action::mojom::SuggestInventory::kBrainstorm;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_HELP_ME_LEARN:
      return fusebox_action::mojom::SuggestInventory::kHelpMeLearn;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_WRITE_OR_EDIT:
      return fusebox_action::mojom::SuggestInventory::kWriteOrEdit;
    default:
      return fusebox_action::mojom::SuggestInventory::kDefault;
  }
}

// static
std::optional<omnibox::SuggestInventory>
EnumTraits<fusebox_action::mojom::SuggestInventory, omnibox::SuggestInventory>::
    FromMojom(fusebox_action::mojom::SuggestInventory input) {
  switch (input) {
    case fusebox_action::mojom::SuggestInventory::kDefault:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT;
    case fusebox_action::mojom::SuggestInventory::kTravel:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_TRAVEL;
    case fusebox_action::mojom::SuggestInventory::kAimTakeover:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_IO_HP_TAKEOVER;
    case fusebox_action::mojom::SuggestInventory::kImageGenTakeover:
      return omnibox::SuggestInventory::
          SUGGEST_INVENTORY_IMG_GEN_IO_HP_TAKEOVER;
    case fusebox_action::mojom::SuggestInventory::kConversationStarters:
      return omnibox::SuggestInventory::
          SUGGEST_INVENTORY_AIM_CONVERSATION_STARTERS;
    case fusebox_action::mojom::SuggestInventory::kBrainstorm:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM;
    case fusebox_action::mojom::SuggestInventory::kHelpMeLearn:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_HELP_ME_LEARN;
    case fusebox_action::mojom::SuggestInventory::kWriteOrEdit:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_WRITE_OR_EDIT;
  }
  return std::nullopt;
}

}  // namespace mojo
