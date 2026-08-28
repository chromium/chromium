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
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_CREATE_CHIP:
      return fusebox_action::mojom::SuggestInventory::kAiHpCreateChip;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_GET_IT_DONE_CHIP:
      return fusebox_action::mojom::SuggestInventory::kAiHpGetItDoneChip;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_SIMPLIFY_CHIP:
      return fusebox_action::mojom::SuggestInventory::kAiHpSimplifyChip;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_IFL:
      return fusebox_action::mojom::SuggestInventory::kAiHpIfl;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_RESERVATIONS:
      return fusebox_action::mojom::SuggestInventory::kAiHpReservations;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM:
      return fusebox_action::mojom::SuggestInventory::kBrainstorm;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_HELP_ME_LEARN:
      return fusebox_action::mojom::SuggestInventory::kHelpMeLearn;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_WRITE_OR_EDIT:
      return fusebox_action::mojom::SuggestInventory::kWriteOrEdit;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_ADD_FILE:
      return fusebox_action::mojom::SuggestInventory::kAddFile;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_ADD_TAB:
      return fusebox_action::mojom::SuggestInventory::kAddTab;
    case omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_IMAGES:
      return fusebox_action::mojom::SuggestInventory::kAimImages;
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
    case fusebox_action::mojom::SuggestInventory::kAiHpCreateChip:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_CREATE_CHIP;
    case fusebox_action::mojom::SuggestInventory::kAiHpGetItDoneChip:
      return omnibox::SuggestInventory::
          SUGGEST_INVENTORY_AI_HP_GET_IT_DONE_CHIP;
    case fusebox_action::mojom::SuggestInventory::kAiHpSimplifyChip:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_SIMPLIFY_CHIP;
    case fusebox_action::mojom::SuggestInventory::kAiHpIfl:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_IFL;
    case fusebox_action::mojom::SuggestInventory::kAiHpReservations:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AI_HP_RESERVATIONS;
    case fusebox_action::mojom::SuggestInventory::kBrainstorm:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM;
    case fusebox_action::mojom::SuggestInventory::kHelpMeLearn:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_HELP_ME_LEARN;
    case fusebox_action::mojom::SuggestInventory::kWriteOrEdit:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_WRITE_OR_EDIT;
    case fusebox_action::mojom::SuggestInventory::kAddFile:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_ADD_FILE;
    case fusebox_action::mojom::SuggestInventory::kAddTab:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_ADD_TAB;
    case fusebox_action::mojom::SuggestInventory::kAimImages:
      return omnibox::SuggestInventory::SUGGEST_INVENTORY_AIM_IMAGES;
  }
  return std::nullopt;
}

}  // namespace mojo
