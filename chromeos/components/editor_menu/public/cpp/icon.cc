// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/editor_menu/public/cpp/icon.h"

#include <string_view>

#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

namespace chromeos::editor_menu {

const gfx::VectorIcon& GetIconForPresetQueryCategory(
    PresetQueryCategory category) {
  switch (category) {
    case PresetQueryCategory::kUnknown:
      return vector_icons::kKeyboardIcon;
    case PresetQueryCategory::kProofread:
      return kEditorMenuProofreadIcon;
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
