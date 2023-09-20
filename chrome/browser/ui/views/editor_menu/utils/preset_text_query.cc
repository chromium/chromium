// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"

#include <string_view>

#include "chrome/browser/ui/views/editor_menu/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

namespace chromeos::editor_menu {

PresetTextQuery::PresetTextQuery(std::string_view text_query_id,
                                 std::u16string_view name,
                                 PresetQueryCategory category)
    : text_query_id(text_query_id), name(name), category(category) {}

const gfx::VectorIcon& GetIconForPresetQueryCategory(
    PresetQueryCategory category) {
  switch (category) {
    case PresetQueryCategory::kUnknown:
      return vector_icons::kKeyboardIcon;
    case PresetQueryCategory::kShorten:
      return kEditorMenuShortenIcon;
    case PresetQueryCategory::kElaborate:
      return kEditorMenuElaborateIcon;
    case PresetQueryCategory::kRephrase:
      return kEditorMenuRephraseIcon;
    case PresetQueryCategory::kFormalize:
      return kEditorMenuFormalizeIcon;
    case PresetQueryCategory::kEmojify:
      return kEditorMenuEmojifyIcon;
  }
}

}  // namespace chromeos::editor_menu
