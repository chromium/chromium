// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos::editor_menu {

std::u16string GetEditorMenuLobsterTitle() {
  return l10n_util::GetStringUTF16(IDS_LOBSTER_EDITOR_MENU_CARD_TAB_LABEL);
}

std::u16string GetEditorMenuLobsterChipLabel() {
  return l10n_util::GetStringUTF16(IDS_LOBSTER_EDITOR_MENU_CARD_CHIP_LABEL);
}

std::u16string GetEditorMenuPromoCardTitle() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_TITLE);
}

std::u16string GetEditorMenuPromoCardDescription() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_DESC);
}

std::u16string GetEditorMenuPromoCardDismissButtonText() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_DISMISS_BUTTON);
}

std::u16string GetEditorMenuPromoCardTryItButtonText() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_TRY_IT_BUTTON);
}

std::u16string GetEditorMenuWriteCardTitle() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_WRITE_CARD_TITLE);
}

std::u16string GetEditorMenuRewriteCardTitle() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_REWRITE_CARD_TITLE);
}

std::u16string
GetEditorMenuFreeformPromptInputFieldPlaceholderForHelpMeWrite() {
  return l10n_util::GetStringUTF16(
      IDS_EDITOR_MENU_FREEFORM_PROMPT_INPUT_FIELD_PLACEHOLDER);
}

std::u16string GetEditorMenuFreeformPromptInputFieldPlaceholderForLobster() {
  return l10n_util::GetStringUTF16(
      IDS_LOBSTER_EDITOR_MENU_CARD_FREEFORM_PLACEHOLDER);
}

std::u16string GetEditorMenuSettingsTooltip() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_SETTINGS_TOOLTIP);
}

std::u16string GetEditorMenuFreeformTextfieldArrowButtonTooltip() {
  return l10n_util::GetStringUTF16(
      IDS_EDITOR_MENU_FREEFORM_TEXTFIELD_ARROW_BUTTON_TOOLTIP);
}

std::u16string GetEditorMenuExperimentBadgeLabel() {
  return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_EXPERIMENT_BADGE);
}

}  // namespace chromeos::editor_menu
