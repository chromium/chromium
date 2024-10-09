// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/color_features.h"
#include "chrome/grit/theme_resources.h"
#include "components/compose/buildflags.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace {

void ApplyDefaultChromeRefreshToolbarColors(ui::ColorMixer& mixer,
                                            const ui::ColorProviderKey& key) {
  mixer[kColorAppMenuHighlightDefault] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorAppMenuExpandedForegroundDefault] = {
      kColorTabForegroundInactiveFrameActive};

  if (key.custom_theme && key.custom_theme->HasCustomImage(IDR_THEME_TOOLBAR)) {
    mixer[kColorAppMenuHighlightDefault] = {
        kColorToolbarBackgroundSubtleEmphasis};
    mixer[kColorAppMenuExpandedForegroundDefault] = {kColorToolbarButtonText};
  }

  mixer[kColorAppMenuHighlightSeverityLow] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuHighlightSeverityMedium] = {kColorAppMenuHighlightDefault};
  mixer[kColorAppMenuHighlightSeverityHigh] = {kColorAppMenuHighlightDefault};
}

}  // namespace

void AddMaterialChromeColorMixer(ui::ColorProvider* provider,
                                 const ui::ColorProviderKey& key) {
  // Adds the color recipes for browser UI colors (toolbar, bookmarks bar,
  // downloads bar etc). While both design systems continue to exist, the
  // material recipes are intended to leverage the existing chrome color mixers,
  // overriding when required to do so according to the new material spec.
  // TODO(crbug.com/40888516): Update color recipes to match UX mocks.
  ui::ColorMixer& mixer = provider->AddMixer();

  // Apply default color transformations irrespective of whether a custom theme
  // is enabled. This is a necessary first pass with chrome refresh flag on to
  // make themes work with the feature.
  ApplyDefaultChromeRefreshToolbarColors(mixer, key);

  // Some colors in the material design should be applied regardless of whether
  // a custom theme is enabled.
  // TODO(tluk): Factor the always-applied material color definitions into a
  // separate file.

  // Content settings activity indicators popup dialog colors.
  mixer[kColorActivityIndicatorForeground] = {ui::kColorSysOnSurface};
  mixer[kColorActivityIndicatorSubtitleForeground] = {
      ui::kColorSysOnSurfaceSubtle};

  // Desktop Media picker colors.
  mixer[kColorDesktopMediaPickerDescriptionLabel] = {
      ui::kColorSysOnSurfaceSubtle};

  // Download bubble colors.
  mixer[kColorDownloadBubbleRowHover] = {ui::kColorSysStateHoverOnSubtle};
  mixer[kColorDownloadBubbleShowAllDownloadsIcon] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorDownloadBubbleInfoBackground] = {
      ui::kColorSubtleEmphasisBackground};
  mixer[kColorDownloadBubbleInfoIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorDownloadBubbleShowAllDownloadsIcon] = {ui::kColorIconSecondary};
  mixer[kColorDownloadBubblePrimaryIcon] = {ui::kColorSysPrimary};
  mixer[kColorDownloadToolbarButtonActive] = ui::PickGoogleColor(
      ui::kColorSysPrimary, kColorDownloadToolbarButtonRingBackground,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      ui::kColorSysNeutralOutline};
  mixer[kColorDownloadToolbarButtonAnimationForeground] =
      AdjustHighlightColorForContrast(ui::kColorSysPrimary,
                                      kColorDownloadShelfBackground);

  // Extensions colors.
  mixer[kColorExtensionsMenuText] = {ui::kColorSysOnSurface};
  mixer[kColorExtensionsMenuSecondaryText] = {ui::kColorSysOnSurfaceSubtle};

  // Lens overlay colors.
  mixer[kColorLensOverlayToastBackground] = {ui::kColorSysInverseSurface};
  mixer[kColorLensOverlayToastButtonBorder] = {ui::kColorSysInverseOnSurface};
  mixer[kColorLensOverlayToastForeground] = {ui::kColorSysInverseOnSurface};

  // PageInfo colors.
  mixer[kColorPageInfoPermissionBlockedOnSystemLevelDisabled] = {
      ui::kColorSysStateDisabled};
  mixer[kColorPageInfoPermissionForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorPageInfoPermissionUsedIcon] = {ui::kColorSysPrimary};

  // Permission Prompt colors.
  mixer[kColorPermissionPromptRequestText] = {ui::kColorSysOnSurfaceSubtle};

  // Performance Intervention colors.
  mixer[kColorPerformanceInterventionButtonIconActive] =
      ui::PickGoogleColor(ui::kColorSysPrimary, ui::kColorSysNeutralOutline,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorPerformanceInterventionButtonIconInactive] = {
      kColorToolbarButtonIcon};

  // Profile Menu colors.
  mixer[kColorProfileMenuBackground] = {ui::kColorSysSurface};
  mixer[kColorProfileMenuHeaderBackground] = {ui::kColorSysTonalContainer};
  mixer[kColorProfileMenuIdentityInfoBackground] = {ui::kColorSysSurface2};
  mixer[kColorProfileMenuIdentityInfoTitle] = {ui::kColorSysOnSurface};
  mixer[kColorProfileMenuIdentityInfoSubtitle] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorProfileMenuHeaderLabel] = {ui::kColorSysOnTonalContainer};
  mixer[kColorProfileMenuIconButton] = {ui::kColorSysOnTonalContainer};
  mixer[kColorProfileMenuIconButtonBackground] = {ui::kColorSysTonalContainer};
  mixer[kColorProfileMenuIconButtonBackgroundHovered] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorProfileMenuSyncErrorIcon] = {ui::kColorSysError};
  mixer[kColorProfileMenuSyncIcon] = {ui::kColorMenuIcon};
  mixer[kColorProfileMenuSyncInfoBackground] = {ui::kColorSysNeutralContainer};
  mixer[kColorProfileMenuSyncOffIcon] = {ui::kColorMenuIcon};
  mixer[kColorProfileMenuSyncPausedIcon] = {ui::kColorSysPrimary};

  // Signin bubble colors. Uses the same colors as the profle menu.
  mixer[kColorChromeSigninBubbleBackground] = {kColorProfileMenuBackground};
  mixer[kColorChromeSigninBubbleInfoBackground] = {
      kColorProfileMenuSyncInfoBackground};

  // Batch upload colors. Uses the same colors as the profile menu.
  mixer[kColorBatchUploadBackground] = {kColorProfileMenuBackground};
  mixer[kColorBatchUploadDataBackground] = {
      kColorProfileMenuSyncInfoBackground};

  // Tab Search colors.
  mixer[kColorTabSearchButtonBackground] = {ui::kColorSysSurface2};
  mixer[kColorTabSearchCardBackground] = {ui::kColorSysSurface5};
  mixer[kColorTabSearchBackground] = {ui::kColorSysSurface};
  mixer[kColorTabSearchDivider] = {ui::kColorSysDivider};
  mixer[kColorTabSearchFooterBackground] = {ui::kColorSysNeutralContainer};
  mixer[kColorTabSearchImageTabContentBottom] = {ui::kColorSysHeaderContainer};
  mixer[kColorTabSearchImageTabContentTop] = {ui::kColorSysOnPrimary};
  mixer[kColorTabSearchImageTabText] = {ui::kColorSysStateRipplePrimary};
  mixer[kColorTabSearchImageWindowFrame] = {ui::kColorSysInversePrimary};
  mixer[kColorTabSearchMediaIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabSearchMediaRecordingIcon] = {ui::kColorSysError};
  mixer[kColorTabSearchPrimaryForeground] = {ui::kColorSysOnSurface};
  mixer[kColorTabSearchSecondaryForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabSearchSelected] = {ui::kColorSysPrimary};
  mixer[kColorTabSearchScrollbarThumb] = {ui::kColorSysPrimary};

  // Side Panel colors.
  mixer[kColorSidePanelBackground] = {ui::kColorSysBaseContainer};

  // Read Anything (in the side panel) colors.
  mixer[kColorReadAnythingCurrentReadAloudHighlight] = {
      ui::kColorSysStateHoverOnSubtle};
  mixer[kColorReadAnythingForeground] = {ui::kColorSysOnSurface};
  mixer[kColorReadAnythingPreviousReadAloudHighlight] = {
      ui::kColorSysOnSurfaceSecondary};

  // Tab Group Dialog colors.
  mixer[kColorTabGroupDialogIconEnabled] = {ui::kColorSysOnSurfaceSubtle};

  // Cast Dialog colors.
  mixer[kColorCastDialogHelpIcon] = {ui::kColorSysPrimary};

  // Help bubble colors.
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

  // WebAuthn modal dialog colors.
  mixer[kColorWebAuthnBackArrowButtonIcon] = {ui::kColorSysPrimary};
  mixer[kColorWebAuthnBackArrowButtonIconDisabled] = {
      ui::kColorSysStateDisabled};
  mixer[kColorWebAuthnHoverButtonForeground] = {ui::kColorSysOnSurface};
  mixer[kColorWebAuthnHoverButtonForegroundDisabled] = {
      ui::kColorSysStateDisabled};
  mixer[kColorWebAuthnIconColor] = {ui::kColorSysPrimary};
  mixer[kColorWebAuthnIconColorDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorWebAuthnPinTextfieldBottomBorder] =
      PickGoogleColor(ui::kColorSysPrimary, ui::kColorSysSurface,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorWebAuthnProgressRingBackground] = ui::SetAlpha(
      kColorWebAuthnProgressRingForeground, gfx::kGoogleGreyAlpha400);
  mixer[kColorWebAuthnProgressRingForeground] = {ui::kColorSysPrimary};

#if BUILDFLAG(ENABLE_COMPOSE)
  // Compose colors.
  mixer[kColorComposeDialogBackground] = {ui::kColorSysSurface};
  mixer[kColorComposeDialogDivider] = {ui::kColorSysDivider};
  mixer[kColorComposeDialogError] = {ui::kColorSysError};
  mixer[kColorComposeDialogForegroundSubtle] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorComposeDialogLink] = {ui::kColorSysPrimary};
  mixer[kColorComposeDialogLogo] = {ui::kColorSysOnTonalContainer};
  mixer[kColorComposeDialogScrollbarThumb] = {ui::kColorSysPrimary};
  mixer[kColorComposeDialogResultBackground] = {ui::kColorSysSurface5};
  mixer[kColorComposeDialogResultForeground] = {ui::kColorSysOnSurface};
  mixer[kColorComposeDialogResultForegroundWhileLoading] = {
      ui::kColorSysPrimary};
  mixer[kColorComposeDialogResultIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorComposeDialogResultContainerScrollbarThumb] = {
      ui::kColorSysTonalOutline};
  mixer[kColorComposeDialogTitle] = {ui::kColorSysOnSurface};
  mixer[kColorComposeDialogTextarea] = {ui::kColorSysOnSurface};
  mixer[kColorComposeDialogTextareaOutline] = {ui::kColorSysNeutralOutline};
  mixer[kColorComposeDialogTextareaPlaceholder] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorComposeDialogTextareaReadonlyBackground] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorComposeDialogTextareaReadonlyForeground] = {
      ui::kColorSysOnSurface};
  mixer[kColorComposeDialogTextareaIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorComposeDialogSelectOptionDisabled] = {
      ui::kColorLabelForegroundDisabled};
#endif  // BUILDFLAG(ENABLE_COMPOSE)

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
  mixer[kColorAvatarButtonHighlightSigninPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarButtonHighlightSyncError] = {ui::kColorSysErrorContainer};
  mixer[kColorAvatarButtonHighlightIncognito] = {ui::kColorSysBaseContainer};
  mixer[kColorAvatarButtonHighlightNormalForeground] = {
      ui::kColorSysOnTonalContainer};
  mixer[kColorAvatarButtonHighlightDefaultForeground] = {
      ui::kColorSysOnSurfaceSecondary};
  mixer[kColorAvatarButtonHighlightSyncErrorForeground] = {
      ui::kColorSysOnErrorContainer};
  mixer[kColorAvatarButtonHighlightIncognitoForeground] = {
      ui::kColorSysOnSurface};
  mixer[kColorAvatarButtonIncognitoHover] = {
      ui::kColorSysStateHoverBrightBlendProtection};
  mixer[kColorAvatarButtonNormalRipple] = {ui::kColorSysStateRipplePrimary};
  mixer[kColorBookmarkBarBackground] = {ui::kColorSysBase};
  mixer[kColorBookmarkBarForeground] = {ui::kColorSysOnSurfaceSubtle};
  // Aligns with kColorToolbarButtonIconInactive.
  mixer[kColorBookmarkBarForegroundDisabled] = {ui::GetResultingPaintColor(
      {ui::kColorSysStateDisabled}, {kColorToolbar})};
  mixer[kColorBookmarkBarSeparatorChromeRefresh] = {ui::kColorSysDivider};
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
  mixer[kColorDownloadItemIconWarning] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorDownloadItemProgressRingForeground] = {ui::kColorSysPrimary};
  mixer[kColorDownloadItemTextDangerous] = {ui::kColorSysError};
  mixer[kColorDownloadItemTextWarning] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorDownloadShelfBackground] = {ui::kColorSysBase};
  mixer[kColorDownloadShelfButtonIcon] = {kColorDownloadShelfForeground};
  mixer[kColorDownloadShelfButtonIconDisabled] = {ui::kColorSysStateDisabled};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorSysPrimary, kColorDownloadShelfBackground,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarSeparatorDefault};
  mixer[kColorDownloadShelfForeground] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorExtensionIconBadgeBackgroundDefault] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorFindBarBackground] = {ui::kColorSysSurface};
  mixer[kColorFlyingIndicatorBackground] = {kColorToolbar};
  mixer[kColorFlyingIndicatorForeground] = {kColorToolbarButtonIcon};
  mixer[kColorFrameCaptionActive] = {ui::kColorSysOnHeaderPrimary};
  mixer[kColorFrameCaptionInactive] = {ui::kColorSysOnHeaderPrimaryInactive};

  // History Embeddings colors.
  mixer[kColorHistoryEmbeddingsBackground] = {ui::kColorSysSurface};
  mixer[kColorHistoryEmbeddingsDivider] = {ui::kColorSysDivider};
  mixer[kColorHistoryEmbeddingsForeground] = {ui::kColorSysOnSurface};
  mixer[kColorHistoryEmbeddingsForegroundSubtle] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorHistoryEmbeddingsImageBackground] = {
      ui::kColorSysNeutralContainer};
  mixer[kColorHistoryEmbeddingsImageBackgroundGradientEnd] = {
      ui::kColorSysGradientTertiary};
  mixer[kColorHistoryEmbeddingsImageBackgroundGradientStart] = {
      ui::kColorSysGradientPrimary};
  mixer[kColorHistoryEmbeddingsWithAnswersBackground] = {ui::kColorSysSurface1};

  mixer[kColorTabHoverCardSecondaryText] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorInfoBarBackground] = {ui::kColorSysBase};
  mixer[kColorInfoBarButtonIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorInfoBarButtonIconDisabled] = {ui::kColorSysStateDisabled};
  if (base::FeatureList::IsEnabled(features::kInfoBarIconMonochrome)) {
    mixer[kColorInfoBarForeground] = {ui::kColorSysOnSurface};
    mixer[ui::kColorInfoBarIcon] = {ui::kColorSysOnSurfaceSubtle};
  } else {
    mixer[kColorInfoBarForeground] = {ui::kColorSysOnSurfaceSubtle};
    mixer[ui::kColorInfoBarIcon] =
        ui::PickGoogleColor(ui::kColorSysPrimary, kColorInfoBarBackground,
                            color_utils::kMinimumVisibleContrastRatio);
  }
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

  // Omnibox chip colors.
  mixer[kColorOmniboxChipInUseActivityIndicatorBackground] = {
      ui::kColorSysPrimary};
  mixer[kColorOmniboxChipInUseActivityIndicatorForeground] = {
      ui::kColorSysOnPrimary};
  mixer[kColorOmniboxChipBackground] = {ui::kColorSysBaseContainerElevated};
  mixer[kColorOmniboxChipBlockedActivityIndicatorBackground] = {
      ui::kColorSysBaseContainerElevated};
  mixer[kColorOmniboxChipBlockedActivityIndicatorForeground] = {
      ui::kColorSysOnSurface};
  mixer[kColorOmniboxChipForegroundLowVisibility] = {ui::kColorSysOnSurface};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = {ui::kColorSysPrimary};
  mixer[kColorOmniboxChipInkDropHover] = {
      ui::kColorSysStateHoverDimBlendProtection};
  mixer[kColorOmniboxChipInkDropRipple] = {
      ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorOmniboxChipOnSystemBlockedActivityIndicatorBackground] = {
      ui::kColorSysBaseContainerElevated};
  mixer[kColorOmniboxChipOnSystemBlockedActivityIndicatorForeground] = {
      ui::kColorSysError};

  // Tabstrip tab alert colors.
  mixer[kColorTabAlertAudioPlayingActiveFrameActive] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabAlertAudioPlayingActiveFrameInactive] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabAlertAudioPlayingInactiveFrameActive] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabAlertAudioPlayingInactiveFrameInactive] = {
      ui::kColorSysOnSurfaceSubtle};
  mixer[kColorTabAlertMediaRecordingActiveFrameActive] = {ui::kColorSysError};
  mixer[kColorTabAlertMediaRecordingActiveFrameInactive] = {ui::kColorSysError};
  mixer[kColorTabAlertMediaRecordingInactiveFrameActive] = {ui::kColorSysError};
  mixer[kColorTabAlertMediaRecordingInactiveFrameInactive] = {
      ui::kColorSysError};
  mixer[kColorTabAlertPipPlayingActiveFrameActive] = {ui::kColorSysPrimary};
  mixer[kColorTabAlertPipPlayingActiveFrameInactive] = {ui::kColorSysPrimary};
  mixer[kColorTabAlertPipPlayingInactiveFrameActive] = {ui::kColorSysPrimary};
  mixer[kColorTabAlertPipPlayingInactiveFrameInactive] = {ui::kColorSysPrimary};

  // Hover card tab alert colors.
  mixer[kColorHoverCardTabAlertMediaRecordingIcon] = {ui::kColorSysError};
  mixer[kColorHoverCardTabAlertPipPlayingIcon] = {ui::kColorSysPrimary};
  mixer[kColorHoverCardTabAlertAudioPlayingIcon] = {
      ui::kColorSysOnSurfaceSubtle};

  // Toolbar colors.
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
  mixer[kColorToolbarIconContainerBorder] = {ui::kColorSysNeutralOutline};
  mixer[kColorToolbarInkDropHover] = {ui::kColorSysStateHoverOnSubtle};
  mixer[kColorToolbarInkDropRipple] = {ui::kColorSysStateRippleNeutralOnSubtle};
  mixer[kColorToolbarExtensionSeparatorEnabled] = {ui::kColorSysDivider};
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
