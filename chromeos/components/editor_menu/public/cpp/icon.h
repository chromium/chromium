// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_ICON_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_ICON_H_

#include "base/component_export.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"

namespace gfx {
struct VectorIcon;
}

namespace chromeos::editor_menu {

const COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP)
    gfx::VectorIcon& GetIconForPresetQueryCategory(
        PresetQueryCategory category);

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_ICON_H_
