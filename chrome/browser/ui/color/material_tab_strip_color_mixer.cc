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
                                   const ui::ColorProviderKey& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }

  // TODO(crbug.com/40883407): Validate final mappings for ChromeRefresh23
  // color.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorTabBackgroundActiveFrameActive] = {ui::kColorSysBase};
  mixer[kColorTabBackgroundActiveFrameInactive] = {
      kColorTabBackgroundActiveFrameActive};

  mixer[kColorTabBackgroundInactiveFrameActive] = {ui::kColorSysHeader};
  mixer[kColorTabBackgroundInactiveFrameInactive] = {
      ui::kColorSysHeaderInactive};
  mixer[kColorTabBackgroundInactiveHoverFrameActive] = {
      ui::kColorSysStateHeaderHover};
  mixer[kColorTabStripControlButtonInkDrop] = {ui::kColorSysStateHeaderHover};
  mixer[kColorTabStripControlButtonInkDropRipple] = {
      ui::kColorSysStateRippleNeutralOnSubtle};

  // TODO(tbergquist): Use kColorSysStateHeaderHoverInactive, once it exists.
  mixer[kColorTabBackgroundInactiveHoverFrameInactive] = {
      ui::kColorSysStateHeaderHoverInactive};

  mixer[kColorTabBackgroundSelectedFrameActive] = {ui::GetResultingPaintColor(
      ui::kColorSysStateHeaderSelect, kColorTabBackgroundInactiveFrameActive)};
  mixer[kColorTabBackgroundSelectedFrameInactive] = {
      ui::GetResultingPaintColor(ui::kColorSysStateHeaderSelect,
                                 kColorTabBackgroundInactiveFrameInactive)};
  mixer[kColorTabBackgroundSelectedHoverFrameActive] = {
      ui::GetResultingPaintColor(ui::kColorSysStateHoverDimBlendProtection,
                                 kColorTabBackgroundSelectedFrameActive)};
  mixer[kColorTabBackgroundSelectedHoverFrameInactive] = {
      ui::GetResultingPaintColor(ui::kColorSysStateHoverDimBlendProtection,
                                 kColorTabBackgroundSelectedFrameInactive)};
#if !BUILDFLAG(IS_ANDROID)
  mixer[kColorTabDiscardRingFrameActive] = {ui::kColorSysStateInactiveRing};
  mixer[kColorTabDiscardRingFrameInactive] = {kColorTabDiscardRingFrameActive};
#endif
  mixer[kColorTabForegroundActiveFrameActive] = {ui::kColorSysOnSurface};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabForegroundInactiveFrameActive] =
      ui::BlendForMinContrast({ui::kColorSysOnSurfaceSecondary},
                              {kColorTabBackgroundInactiveFrameActive});
  mixer[kColorTabForegroundInactiveFrameInactive] =
      ui::BlendForMinContrast({kColorTabForegroundInactiveFrameActive},
                              {kColorTabBackgroundInactiveFrameInactive});

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

  // Tabstrip Control Button colors.
  mixer[kColorNewTabButtonCRForegroundFrameActive] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorNewTabButtonCRForegroundFrameInactive] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorNewTabButtonCRBackgroundFrameActive] = {
      ui::kColorSysHeaderContainer};
  mixer[kColorNewTabButtonCRBackgroundFrameInactive] = {
      ui::kColorSysHeaderContainerInactive};

  mixer[kColorTabSearchButtonCRForegroundFrameActive] = {
      ui::kColorSysOnSurfacePrimary};
  mixer[kColorTabSearchButtonCRForegroundFrameInactive] = {
      ui::kColorSysOnSurfacePrimaryInactive};
}
