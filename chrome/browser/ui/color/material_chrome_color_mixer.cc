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

namespace {

void ApplyDefaultChromeRefreshToolbarColors(
    ui::ColorMixer& mixer,
    const ui::ColorProviderManager::Key& key) {
  mixer[kColorAppMenuHighlightDefault] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorAppMenuExpandedForegroundDefault] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorAppMenuHighlightSeverityLow] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuHighlightSeverityMedium] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuHighlightSeverityHigh] = {kColorAppMenuHighlightDefault};
}

}  // namespace

void AddMaterialChromeColorMixer(ui::ColorProvider* provider,
                                 const ui::ColorProviderManager::Key& key) {
  // Adds the color recipes for browser UI colors (toolbar, bookmarks bar,
  // downloads bar etc). While both design systems continue to exist, the
  // material recipes are intended to leverage the existing chrome color mixers,
  // overriding when required to do so according to the new material spec.
  // TODO(crbug.com/1408542): Update color recipes to match UX mocks.
  ui::ColorMixer& mixer = provider->AddMixer();

  // Apply default color transformations irrespective of whether a custom theme
  // is enabled. This is a necessary first pass with chrome refresh flag on to
  // make themes work with the feature.
  ApplyDefaultChromeRefreshToolbarColors(mixer, key);

  // Some colors in the material design should be applied regardless of whether
  // a custom theme is enabled.
  // TODO(tluk): Factor the always-applied material color definitions into a
  // separate file.

  // Download bubble colors.
  mixer[kColorDownloadBubbleRowHover] = {ui::kColorSysStateHoverOnSubtle};

  // Profile Menu colors.
  mixer[kColorProfileMenuHeaderBackground] = {ui::kColorSysTonalContainer};
  mixer[kColorProfileMenuHeaderLabel] = {ui::kColorSysOnTonalContainer};
  mixer[kColorProfileMenuIconButton] = {ui::kColorSysOnTonalContainer};
  mixer[kColorProfileMenuIconButtonBackground] = {ui::kColorSysTonalContainer};
  mixer[kColorProfileMenuIconButtonBackgroundHovered] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorProfileMenuSyncIcon] = {ui::kColorMenuIcon};
  mixer[kColorProfileMenuSyncErrorIcon] = {ui::kColorSysError};
  mixer[kColorProfileMenuSyncOffIcon] = {ui::kColorMenuIcon};
  mixer[kColorProfileMenuSyncPausedIcon] = {ui::kColorSysPrimary};

  // Tab Search colors.
  mixer[kColorTabSearchBackground] = {ui::kColorSysSurface};
  mixer[kColorTabSearchDivider] = {ui::kColorSysOnBaseDivider};
  mixer[kColorTabSearchMediaIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabSearchMediaRecordingIcon] = {ui::kColorSysError};
  mixer[kColorTabSearchPrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorTabSearchSecondaryForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabSearchScrollbarThumb] = {ui::kColorSysPrimary};

  mixer[kColorSidePanelBackground] = {ui::kColorSysBaseContainer};

  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }

  mixer[kColorAppMenuHighlightDefault] = {ui::kColorSysTonalContainer};
  mixer[kColorAppMenuHighlightSeverityLow] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuHighlightSeverityMedium] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuHighlightSeverityHigh] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuExpandedForegroundDefault] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorAppMenuChipInkDropHover] = {ui::kColorSysStateHoverOnSubtle};
  mixer[kColorAppMenuChipInkDropRipple] = {ui::kColorSysStateRipplePrimary};
  mixer[kColorAvatarButtonHighlightNormal] = {ui::kColorSysTonalContainer};
  mixer[kColorAvatarButtonHighlightSyncPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarButtonHighlightSyncError] = {ui::kColorSysErrorContainer};
  mixer[kColorAvatarButtonHighlightIncognito] = {ui::kColorSysBaseContainer};
  mixer[kColorAvatarButtonHighlightNormalForeground] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorAvatarButtonHighlightDefaultForeground] = {
      ui::kColorSysOnSecondaryContainer};
  mixer[kColorAvatarButtonHighlightSyncErrorForeground] = {
      ui::kColorSysOnErrorContainer};
  mixer[kColorAvatarButtonHighlightIncognitoForeground] = {
      ui::kColorSysOnSurface};
  mixer[kColorAvatarButtonIncognitoHover] = {
      ui::kColorSysStateHoverBrightBlendProtection};
  mixer[kColorAvatarButtonNormalRipple] = {ui::kColorSysStateRipplePrimary};
  mixer[kColorBookmarkBarBackground] = {ui::kColorSysBase};
  mixer[kColorBookmarkBarForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorBookmarkBarSeparatorChromeRefresh] = {ui::kColorSysOnBaseDivider};
  mixer[kColorBookmarkButtonIcon] = {kColorBookmarkBarForeground};
  mixer[kColorBookmarkDialogProductImageBorder] = {ui::kColorSysNeutralOutline};
  mixer[kColorBookmarkDialogTrackPriceIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorBookmarkDragImageBackground] = {ui::kColorSysPrimary};
  mixer[kColorBookmarkFolderIcon] = {kColorBookmarkBarForeground};
  mixer[kColorCapturedTabContentsBorder] = {ui::kColorSysPrimary};
  mixer[kColorDownloadItemForegroundDisabled] = BlendForMinContrast(
      ui::GetResultingPaintColor(ui::kColorSysStateDisabled,
                                 kColorDownloadShelfBackground),
      kColorDownloadShelfBackground);
  mixer[kColorDownloadItemIconDangerous] = {ui::kColorSysError};
  // TODO(crbug.com/1399939): use a yellow-ish CR2023 color instead.
  mixer[kColorDownloadItemIconWarning] = {ui::kColorAlertMediumSeverityIcon};
  mixer[kColorDownloadItemProgressRingForeground] = {ui::kColorSysPrimary};
  mixer[kColorDownloadItemTextDangerous] = {ui::kColorSysError};
  // TODO(crbug.com/1399939): use a yellow-ish CR2023 color instead.
  mixer[kColorDownloadItemTextWarning] = {ui::kColorAlertMediumSeverityText};
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
  mixer[kColorExtensionIconBadgeBackgroundDefault] = {
      ui::kColorSysNeutralContainer};
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
  mixer[kColorFindBarBackground] = {ui::kColorSysSurface};
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
  mixer[kColorOmniboxChipForegroundLowVisibility] = {ui::kColorSysOnSurface};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = {ui::kColorSysPrimary};
  mixer[kColorOmniboxChipInkDropHover] = {
      ui::kColorSysStateHoverDimBlendProtection};
  mixer[kColorOmniboxChipInkDropRipple] = {
      ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorTabAlertMediaRecordingIcon] = {ui::kColorSysError};
  mixer[kColorTabAlertPipPlayingIcon] = {ui::kColorSysPrimary};
  mixer[kColorTabAlertAudioPlayingIcon] = {ui::kColorSysOnSurfaceSubtle};
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
