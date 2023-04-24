// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

void AddMaterialChromeColorMixer(ui::ColorProvider* provider,
                                 const ui::ColorProviderManager::Key& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }

  // Adds the color recipes for browser UI colors (toolbar, bookmarks bar,
  // downloads bar etc). While both design systems continue to exist, the
  // material recipes are intended to leverage the existing chrome color mixers,
  // overriding when required to do so according to the new material spec.
  // TODO(crbug.com/1408542): Update color recipes to match UX mocks.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAvatarButtonHighlightNormal] =
      AdjustHighlightColorForContrast(ui::kColorSysPrimary, kColorToolbar);
  mixer[kColorBookmarkBarBackground] = {ui::kColorSysBase};
  mixer[kColorBookmarkBarForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorBookmarkButtonIcon] = {kColorBookmarkBarForeground};
  mixer[kColorBookmarkFolderIcon] = {kColorBookmarkBarForeground};
  mixer[kColorBookmarkDragImageBackground] = {ui::kColorSysPrimary};
  mixer[kColorCapturedTabContentsBorder] = {ui::kColorSysPrimary};
  mixer[kColorDownloadItemForegroundDisabled] = BlendForMinContrast(
      ui::GetResultingPaintColor(ui::kColorSysStateDisabled,
                                 kColorDownloadShelfBackground),
      kColorDownloadShelfBackground);
  mixer[kColorDownloadItemProgressRingForeground] = {ui::kColorSysPrimary};
  mixer[kColorDownloadShelfBackground] = {ui::kColorSysBase};
  mixer[kColorDownloadShelfButtonIcon] = {kColorDownloadShelfForeground};
  mixer[kColorDownloadShelfButtonIconDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorSysPrimary, kColorDownloadShelfBackground,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarSeparatorDefault};
  mixer[kColorDownloadShelfForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorDownloadToolbarButtonActive] =
      ui::PickGoogleColor(ui::kColorSysPrimary, kColorDownloadShelfBackground,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorDownloadToolbarButtonAnimationForeground] =
      AdjustHighlightColorForContrast(ui::kColorSysPrimary,
                                      kColorDownloadShelfBackground);
  mixer[kColorExtensionIconBadgeBackgroundDefault] =
      ui::PickGoogleColor(ui::kColorSysPrimary, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorFeaturePromoBubbleBackground] = {ui::kColorSysPrimary};
  mixer[kColorFeaturePromoBubbleButtonBorder] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] =
      AdjustHighlightColorForContrast(kColorFeaturePromoBubbleForeground,
                                      kColorFeaturePromoBubbleBackground);
  mixer[kColorFeaturePromoBubbleDefaultButtonBackground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFeaturePromoBubbleDefaultButtonForeground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeaturePromoBubbleForeground] = {ui::kColorSysOnPrimary};
  mixer[kColorFlyingIndicatorBackground] = {kColorToolbar};
  mixer[kColorFlyingIndicatorForeground] = {kColorToolbarButtonIcon};
  mixer[kColorFrameCaptionActive] = {ui::kColorSysOnHeaderPrimary};
  mixer[kColorFrameCaptionInactive] = {ui::kColorSysOnHeaderPrimaryInactive};
  mixer[kColorInfoBarBackground] = {ui::kColorSysBase};
  mixer[kColorInfoBarButtonIcon] = {kColorInfoBarForeground};
  mixer[kColorInfoBarButtonIconDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorInfoBarForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[ui::kColorInfoBarIcon] =
      ui::PickGoogleColor(ui::kColorSysPrimary, kColorInfoBarBackground,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorMediaRouterIconActive] =
      ui::PickGoogleColor(ui::kColorSysPrimary, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorNewTabButtonFocusRing] = ui::PickGoogleColorTwoBackgrounds(
      ui::kColorSysStateFocusRing,
      ui::GetResultingPaintColor(kColorNewTabButtonBackgroundFrameActive,
                                 ui::kColorFrameActive),
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorNewTabButtonInkDropFrameActive] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorNewTabButtonInkDropFrameInactive] = {
      kColorNewTabButtonInkDropFrameActive};
  mixer[kColorOmniboxChipBackground] = {ui::kColorSysBaseContainerElevated};
  mixer[kColorOmniboxChipForegroundLowVisibility] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = {ui::kColorSysOnSurface};
  mixer[kColorToolbar] = {ui::kColorSysBase};
  mixer[kColorToolbarButtonBackgroundHighlightedDefault] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorToolbarButtonBorder] = {ui::kColorSysOutline};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorToolbarButtonIconDefault] = {ui::kColorSysOnSurfaceSecondary};
  mixer[kColorToolbarButtonIconDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarButtonIconInactive] = {ui::GetResultingPaintColor(
      {ui::kColorSysStateDisabled}, {kColorToolbar})};
  mixer[kColorToolbarButtonIconPressed] = {kColorToolbarButtonIconHovered};
  mixer[kColorToolbarButtonText] = {ui::kColorSysOnSurfaceSecondary};
  mixer[kColorToolbarContentAreaSeparator] = {ui::kColorSysSurfaceVariant};
  mixer[kColorToolbarFeaturePromoHighlight] = {ui::kColorSysPrimary};
  mixer[kColorToolbarInkDropHover] = {ui::kColorSysStateHoverOnSubtle};
  mixer[kColorToolbarInkDropRipple] = {ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorToolbarExtensionSeparatorEnabled] = {ui::kColorSysOnBaseDivider};
  mixer[kColorToolbarExtensionSeparatorDisabled] = {
      kColorToolbarButtonIconInactive};
  mixer[kColorToolbarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorToolbarSeparatorDefault] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorToolbar, 0x3A);
  mixer[kColorToolbarText] = {kColorToolbarTextDefault};
  mixer[kColorToolbarTextDefault] = {ui::kColorSysOnSurfaceSecondary};
  mixer[kColorToolbarTextDisabled] = {kColorToolbarTextDisabledDefault};
  mixer[kColorToolbarTextDisabledDefault] = {ui::kColorSysStateDisabled};
}
