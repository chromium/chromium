// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element_type.h"

#include "base/check_op.h"

namespace vr {

namespace {

static const char* g_ui_element_type_strings[] = {
    "kTypeNone",
    "kTypeButtonBackground",
    "kTypeButtonForeground",
    "kTypeButtonHitTarget",
    "kTypeButtonText",
    "kTypeHostedUiBackplane",
    "kTypeScaledDepthAdjuster",
    "kTypeOmniboxSuggestionBackground",
    "kTypeOmniboxSuggestionLayout",
    "kTypeOmniboxSuggestionTextLayout",
    "kTypeOmniboxSuggestionIconField",
    "kTypeOmniboxSuggestionIcon",
    "kTypeOmniboxSuggestionContentText",
    "kTypeOmniboxSuggestionDescriptionText",
    "kTypePromptBackplane",
    "kTypePromptShadow",
    "kTypePromptBackground",
    "kTypePromptIcon",
    "kTypePromptText",
    "kTypePromptPrimaryButton",
    "kTypePromptSecondaryButton",
    "kTypeSpacer",
    "kTypeTextInputHint",
    "kTypeTextInputText",
    "kTypeTextInputCursor",
    "kTypeToastBackground",
    "kTypeToastText",
    "kTypeCursorBackground",
    "kTypeCursorForeground",
    "kTypeOverflowMenuButton",
    "kTypeOverflowMenuItem",
    "kTypeTooltip",
    "kTypeLabel",
    "kTypeTabItem",
    "kTypeTabItemRemoveButton",
};

static_assert(
    kNumUiElementTypes == std::size(g_ui_element_type_strings),
    "Mismatch between the kUiElementType enum and the corresponding array "
    "of strings.");

}  // namespace

std::string UiElementTypeToString(UiElementType type) {
  DCHECK_GT(kNumUiElementTypes, type);
  return g_ui_element_type_strings[type];
}

}  // namespace vr
