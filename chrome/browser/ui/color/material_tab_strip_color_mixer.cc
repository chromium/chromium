// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_tab_strip_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace {
/* 70% opacity */
constexpr SkAlpha kWebUiTabStripScrollbarThumbAlpha = 0.7 * 255;

/* 16% opacity */
constexpr SkAlpha kWebUiTabStripTabSeparatorAlpha = 0.16 * 255;
}  // namespace

void AddMaterialTabStripColorMixer(ui::ColorProvider* provider,
                                   const ui::ColorProviderManager::Key& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }

  // TODO(crbug.com/1399942): Validate final mappings for ChromeRefresh23 color.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorTabBackgroundActiveFrameActive] = {ui::kColorSysBase};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorSysHeader};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {
      ui::kColorSysHeaderInactive};

  mixer[kColorTabForegroundActiveFrameActive] = {ui::kColorSysOnSurface};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      ui::kColorSysOnSurfaceSecondary};
  mixer[kColorTabForegroundInactiveFrameInactive] = {
      kColorTabForegroundInactiveFrameActive};

  mixer[kColorTabBackgroundHoverFrameActive] = {ui::kColorSysStateHeaderHover};
  mixer[kColorTabBackgroundHoverFrameInactive] = {
      ui::kColorSysStateHoverOnSubtle};

  /* WebUI Tab Strip colors. */
  mixer[kColorWebUiTabStripBackground] = {ui::kColorSysHeader};
  mixer[kColorWebUiTabStripFocusOutline] = {ui::kColorSysPrimary};
  mixer[kColorWebUiTabStripScrollbarThumb] =
      ui::SetAlpha(ui::GetColorWithMaxContrast(ui::kColorSysHeader),
                   kWebUiTabStripScrollbarThumbAlpha);
  mixer[kColorWebUiTabStripTabActiveTitleBackground] = {ui::kColorSysPrimary};
  mixer[kColorWebUiTabStripTabActiveTitleContent] = {ui::kColorSysOnPrimary};
  mixer[kColorWebUiTabStripTabBackground] = {ui::kColorSysSurface};
  mixer[kColorWebUiTabStripTabSeparator] =
      ui::SetAlpha(ui::kColorSysOnSurface, kWebUiTabStripTabSeparatorAlpha);
  mixer[kColorWebUiTabStripTabText] = {ui::kColorSysOnSurface};

  // TabDivider colors.
  mixer[kColorTabDividerFrameActive] = {ui::kColorSysOnHeaderDivider};
  mixer[kColorTabDividerFrameInactive] = {ui::kColorSysOnHeaderDividerInactive};

  // Tabstrip Button colors.
  mixer[kColorNewTabButtonCRForegroundFrameActive] = {
      ui::kColorSysOnSurfacePrimary};
  mixer[kColorNewTabButtonCRForegroundFrameInactive] = {
      ui::kColorSysOnSurfacePrimaryInactive};
  mixer[kColorNewTabButtonCRBackgroundFrameActive] = {
      ui::kColorSysHeaderContainer};
  mixer[kColorNewTabButtonCRBackgroundFrameInactive] = {
      ui::kColorSysHeaderContainerInactive};
  // MISSING 2
}
