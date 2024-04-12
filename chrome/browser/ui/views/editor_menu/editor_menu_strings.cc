// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_strings.h"

#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos::editor_menu {

std::u16string GetEditorMenuPromoCardTitle() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_TITLE)
             : u"Write faster and with more confidence";
}

std::u16string GetEditorMenuPromoCardDescription() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_DESC)
             : u"Use Help me write to create a draft or refine existing work, "
               u"powered by Google AI";
}

std::u16string GetEditorMenuPromoCardDismissButtonText() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_PROMO_CARD_DISMISS_BUTTON)
             : u"No thanks";
}

std::u16string GetEditorMenuPromoCardTryItButtonText() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_PROMO_CARD_TRY_IT_BUTTON)
             : u"Try it";
}

std::u16string GetEditorMenuWriteCardTitle() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_WRITE_CARD_TITLE)
             : u"Help me write";
}

std::u16string GetEditorMenuRewriteCardTitle() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_REWRITE_CARD_TITLE)
             : u"Rewrite";
}

std::u16string GetEditorMenuWriteCardFreeformHolder() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_WRITE_CARD_FREEFORM_PLACEHOLDER)
             : u"Enter a prompt like \"write a thank you note\"";
}

std::u16string GetEditorMenuRewriteCardFreeformHolder() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_REWRITE_CARD_FREEFORM_PLACEHOLDER)
             : u"Enter a prompt like \"make it more confident\"";
}

std::u16string GetEditorMenuSettingsTooltip() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_SETTINGS_TOOLTIP)
             : u"Help me write settings";
}

std::u16string GetEditorMenuFreeformTextfieldArrowButtonTooltip() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_FREEFORM_TEXTFIELD_ARROW_BUTTON_TOOLTIP)
             : u"Submit";
}

std::u16string GetEditorMenuExperimentBadgeLabel() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_EXPERIMENT_BADGE)
             : u"Experiment";
}

}  // namespace chromeos::editor_menu
