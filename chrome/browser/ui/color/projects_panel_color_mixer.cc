// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/projects_panel_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

void AddProjectsPanelColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorProjectsPanelBackground] = {ui::kColorSysSurface2};
  mixer[kColorProjectsPanelButtonDisabledIcon] = {ui::kColorSysStateDisabled};
  mixer[kColorProjectsPanelButtonHoverBackground] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorProjectsPanelButtonIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorProjectsPanelListsSeparator] = {ui::kColorSysOnHeaderDivider};
  mixer[kColorProjectsPanelNoTabGroupsText] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorProjectsPanelTabGroupsDragPlaceholder] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorProjectsPanelTabGroupsDropIndicator] = {ui::kColorSysOutline};
}
