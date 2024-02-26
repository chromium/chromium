// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_ICON_H_

#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"

namespace gfx {
struct VectorIcon;
}

namespace chromeos::editor_menu {

const gfx::VectorIcon& GetIconForPresetQueryCategory(
    PresetQueryCategory category);

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_ICON_H_
