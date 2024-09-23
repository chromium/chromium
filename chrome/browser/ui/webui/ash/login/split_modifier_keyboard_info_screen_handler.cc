// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/split_modifier_keyboard_info_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

SplitModifierKeyboardInfoScreenHandler::SplitModifierKeyboardInfoScreenHandler()
    : BaseScreenHandler(kScreenId) {}

SplitModifierKeyboardInfoScreenHandler::
    ~SplitModifierKeyboardInfoScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void SplitModifierKeyboardInfoScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("splitModifierTitle", IDS_OOBE_SPLIT_MODIFIER_INFO_TITLE);
  builder->Add("splitModifierSubtitle", IDS_OOBE_SPLIT_MODIFIER_INFO_SUBTITLE);
  builder->Add("splitModifierFirstDescriptionTitle",
               IDS_OOBE_SPLIT_MODIFIER_INFO_FIRST_DESCRIPTION_TITLE);
  builder->Add("splitModifierFirstDescriptionText",
               IDS_OOBE_SPLIT_MODIFIER_INFO_FIRST_DESCRIPTION_TEXT);
  builder->Add("splitModifierFirstDescriptionAccessibility",
               IDS_OOBE_SPLIT_MODIFIER_INFO_FIRST_DESCRIPTION_ACCESSIBILITY);
  builder->Add("splitModifierSecondDescriptionTitle",
               IDS_OOBE_SPLIT_MODIFIER_INFO_SECOND_DESCRIPTION_TITLE);
  builder->Add("splitModifierSecondDescriptionText",
               IDS_OOBE_SPLIT_MODIFIER_INFO_SECOND_DESCRIPTION_TEXT);
  builder->Add("splitModifierSecondDescriptionAccessibility",
               IDS_OOBE_SPLIT_MODIFIER_INFO_SECOND_DESCRIPTION_ACCESSIBILITY);
}

void SplitModifierKeyboardInfoScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<SplitModifierKeyboardInfoScreenView>
SplitModifierKeyboardInfoScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
