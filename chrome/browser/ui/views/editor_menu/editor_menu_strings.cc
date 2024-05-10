// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_strings.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos::editor_menu {

namespace {

constexpr auto kAllowedLanguagesForShowingL10nStrings =
    base::MakeFixedFlatSet<std::string_view>({"de", "en", "en-GB", "fr", "ja"});

std::string GetSystemLocale() {
  return g_browser_process != nullptr
             ? g_browser_process->GetApplicationLocale()
             : "";
}

bool ShouldUseL10nStrings() {
  return chromeos::features::IsOrcaUseL10nStringsEnabled() ||
         (chromeos::features::IsOrcaInternationalizeEnabled() &&
          base::Contains(kAllowedLanguagesForShowingL10nStrings,
                         GetSystemLocale()));
}

}  // namespace

std::u16string GetEditorMenuPromoCardTitle() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_TITLE)
             : u"Write faster and with more confidence";
}

std::u16string GetEditorMenuPromoCardDescription() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_PROMO_CARD_DESC)
             : u"Use Help me write to create a draft or refine existing work, "
               u"powered by Google AI";
}

std::u16string GetEditorMenuPromoCardDismissButtonText() {
  return ShouldUseL10nStrings() ? l10n_util::GetStringUTF16(
                                      IDS_EDITOR_MENU_PROMO_CARD_DISMISS_BUTTON)
                                : u"No thanks";
}

std::u16string GetEditorMenuPromoCardTryItButtonText() {
  return ShouldUseL10nStrings() ? l10n_util::GetStringUTF16(
                                      IDS_EDITOR_MENU_PROMO_CARD_TRY_IT_BUTTON)
                                : u"Try it";
}

std::u16string GetEditorMenuWriteCardTitle() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_WRITE_CARD_TITLE)
             : u"Help me write";
}

std::u16string GetEditorMenuRewriteCardTitle() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_REWRITE_CARD_TITLE)
             : u"Rewrite";
}

std::u16string GetEditorMenuFreeformPromptInputFieldPlaceholder() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_FREEFORM_PROMPT_INPUT_FIELD_PLACEHOLDER)
             : u"Enter a prompt";
}

std::u16string GetEditorMenuSettingsTooltip() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_SETTINGS_TOOLTIP)
             : u"Help me write settings";
}

std::u16string GetEditorMenuFreeformTextfieldArrowButtonTooltip() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(
                   IDS_EDITOR_MENU_FREEFORM_TEXTFIELD_ARROW_BUTTON_TOOLTIP)
             : u"Submit";
}

std::u16string GetEditorMenuExperimentBadgeLabel() {
  return ShouldUseL10nStrings()
             ? l10n_util::GetStringUTF16(IDS_EDITOR_MENU_EXPERIMENT_BADGE)
             : u"Experiment";
}

}  // namespace chromeos::editor_menu
