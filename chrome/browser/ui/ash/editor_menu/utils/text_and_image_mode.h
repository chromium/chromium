// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_EDITOR_MENU_UTILS_TEXT_AND_IMAGE_MODE_H_
#define CHROME_BROWSER_UI_ASH_EDITOR_MENU_UTILS_TEXT_AND_IMAGE_MODE_H_

#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"

namespace chromeos::editor_menu {

enum class LobsterMode {
  // TODO: b:388964690 - Merge kNoSelectedText and kSelectedText into one enum
  // value.
  kNoSelectedText = 0,
  kSelectedText,
  kBlocked,
};

enum class TextAndImageMode {
  kBlocked,
  kPromoCard,
  kEditorWriteOnly,
  kEditorRewriteOnly,
  kLobsterWithNoSelectedText,
  kLobsterWithSelectedText,
  kEditorWriteAndLobster,
  kEditorRewriteAndLobster,
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_ASH_EDITOR_MENU_UTILS_TEXT_AND_IMAGE_MODE_H_
