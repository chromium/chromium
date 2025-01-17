// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"

namespace chromeos::editor_menu {

TextAndImageMode CalculateTextAndImageMode(EditorMode editor_mode,
                                           LobsterMode lobster_mode) {
  if (lobster_mode == LobsterMode::kBlocked) {
    if (editor_mode == EditorMode::kRewrite) {
      return TextAndImageMode::kEditorRewriteOnly;
    }
    if (editor_mode == EditorMode::kWrite) {
      return TextAndImageMode::kEditorWriteOnly;
    }
    if (editor_mode == EditorMode::kConsentNeeded) {
      return TextAndImageMode::kPromoCard;
    }
    return TextAndImageMode::kBlocked;
  }

  if (editor_mode == EditorMode::kRewrite) {
    return TextAndImageMode::kEditorRewriteAndLobster;
  }
  if (editor_mode == EditorMode::kWrite) {
    return TextAndImageMode::kEditorWriteAndLobster;
  }
  if (editor_mode == EditorMode::kConsentNeeded) {
    return TextAndImageMode::kPromoCard;
  }
  if (lobster_mode == LobsterMode::kNoSelectedText) {
    return TextAndImageMode::kLobsterWithNoSelectedText;
  }
  return TextAndImageMode::kLobsterWithSelectedText;
}

}  // namespace chromeos::editor_menu
