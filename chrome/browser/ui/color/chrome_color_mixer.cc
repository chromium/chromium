// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/color_features.h"
#include "chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

// This differs from ui::SelectColorBasedOnInput in that we're checking if the
// input transform is *not* dark under the assumption that the background color
// *is* dark from a potential custom theme. Additionally, if the mode is
// explicitly dark just select the correct color for that mode.
ui::ColorTransform SelectColorBasedOnDarkInputOrMode(
    bool dark_mode,
    ui::ColorTransform input_transform,
    ui::ColorTransform dark_mode_color_transform,
    ui::ColorTransform light_mode_color_transform) {
  const auto generator = [](bool dark_mode, ui::ColorTransform input_transform,
                            ui::ColorTransform dark_mode_color_transform,
                            ui::ColorTransform light_mode_color_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor transform_color = input_transform.Run(input_color, mixer);
    const SkColor dark_mode_color =
        dark_mode_color_transform.Run(input_color, mixer);
    const SkColor light_mode_color =
        light_mode_color_transform.Run(input_color, mixer);
    const SkColor result_color =
        dark_mode || !color_utils::IsDark(transform_color) ? dark_mode_color
                                                           : light_mode_color;
    DVLOG(2) << "ColorTransform SelectColorBasedOnDarkColorOrMode:"
             << " Dark Mode: " << dark_mode
             << " Transform Color: " << ui::SkColorName(transform_color)
             << " Dark Mode Color: " << ui::SkColorName(dark_mode_color)
             << " Light Mode Color: " << ui::SkColorName(light_mode_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, dark_mode, std::move(input_transform),
                             std::move(dark_mode_color_transform),
                             std::move(light_mode_color_transform));
}

ui::ColorTransform GetToolbarTopSeparatorColorTransform(
    ui::ColorTransform toolbar_color_transform,
    ui::ColorTransform frame_color_transform) {
  const auto generator = [](ui::ColorTransform toolbar_color_transform,
                            ui::ColorTransform frame_color_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor toolbar_color =
        toolbar_color_transform.Run(input_color, mixer);
    const SkColor frame_color = frame_color_transform.Run(input_color, mixer);
    const SkColor result_color =
        GetToolbarTopSeparatorColor(toolbar_color, frame_color);
    DVLOG(2) << "ColorTransform GetToolbarTopSeparatorColor:"
             << " Input Color: " << ui::SkColorName(input_color)
             << " Toolbar Transform Color: " << ui::SkColorName(toolbar_color)
             << " Frame Transform Color: " << ui::SkColorName(frame_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(toolbar_color_transform),
                             std::move(frame_color_transform));
}

// Apply updates to the Omnibox background color tokens per GM3 spec.
void ApplyGM3OmniboxBackgroundColor(ui::ColorMixer& mixer,
                                    const ui::ColorProviderKey& key) {
  // Apply omnibox background color updates only to non-themed clients.
  if (!key.custom_theme) {
    mixer[kColorLocationBarBackground] = {ui::kColorSysOmniboxContainer};
    mixer[kColorLocationBarBackgroundHovered] =
        ui::GetResultingPaintColor(ui::kColorSysStateHoverBrightBlendProtection,
                                   kColorLocationBarBackground);

    // Update colors to account for "mismatched input/URL" in the omnibox.
    mixer[kColorLocationBarBorderOnMismatch] = {ui::kColorSysNeutralOutline};
  }
}

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderKey& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAppMenuHighlightSeverityLow] = AdjustHighlightColorForContrast(
      ui::kColorAlertLowSeverity, kColorToolbar);
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      kColorAvatarButtonHighlightSyncError};
  mixer[kColorAppMenuHighlightSeverityMedium] = AdjustHighlightColorForContrast(
      ui::kColorAlertMediumSeverityIcon, kColorToolbar);
  mixer[kColorAppMenuHighlightPrimary] = {ui::kColorButtonBackgroundProminent};
  mixer[kColorAppMenuExpandedForegroundPrimary] = {
      ui::kColorButtonForegroundProminent};
  mixer[kColorAvatarButtonHighlightNormal] =
      AdjustHighlightColorForContrast(ui::kColorAccent, kColorToolbar);
  mixer[kColorAvatarButtonHighlightSyncError] = AdjustHighlightColorForContrast(
      ui::kColorAlertHighSeverity, kColorToolbar);
  mixer[kColorAvatarButtonHighlightSyncPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarButtonHighlightSigninPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarButtonHighlightExplicitText] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarStrokeLight] = {SK_ColorWHITE};
  mixer[kColorAvatarStroke] = {kColorToolbarButtonIcon};
  mixer[kColorAvatarFillForContrast] = {kColorToolbar};
  mixer[kColorBookmarkBarBackground] = {kColorToolbar};
  mixer[kColorBookmarkBarForeground] = {kColorToolbarText};
  // Uses the alpha of kColorToolbarButtonIconInactive.
  mixer[kColorBookmarkBarForegroundDisabled] =
      ui::SetAlpha(kColorBookmarkBarForeground, gfx::kGoogleGreyAlpha500);
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorBookmarkDialogProductImageBorder] =
      ui::SetAlpha(gfx::kGoogleGrey900, 0x24);
  mixer[kColorBookmarkDialogTrackPriceIcon] = {gfx::kGoogleGrey700};
  mixer[kColorBookmarkFavicon] = ui::PickGoogleColor(
      gfx::kGoogleGrey500, kColorBookmarkBarBackground, 6.0f);
  mixer[kColorBookmarkFolderIcon] = {ui::kColorIcon};
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparator};
  mixer[kColorBookmarkBarSeparatorChromeRefresh] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorBookmarkDragImageBackground] = {ui::kColorAccent};
  mixer[kColorBookmarkDragImageCountBackground] = {ui::kColorAlertHighSeverity};
  mixer[kColorBookmarkDragImageCountForeground] =
      ui::GetColorWithMaxContrast(kColorBookmarkDragImageCountBackground);
  mixer[kColorBookmarkDragImageForeground] =
      ui::GetColorWithMaxContrast(kColorBookmarkDragImageBackground);
  mixer[kColorBookmarkDragImageIconBackground] = {
      kColorBookmarkDragImageForeground};
  mixer[kColorCaptionButtonBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorCapturedTabContentsBorder] = {ui::kColorAccent};
  mixer[kColorCastDialogHelpIcon] = {ui::kColorAccent};
  mixer[kColorChromeSigninBubbleBackground] = {kColorProfileMenuBackground};
  mixer[kColorChromeSigninBubbleInfoBackground] = {
      kColorProfileMenuSyncInfoBackground};
  mixer[kColorBatchUploadBackground] = {kColorProfileMenuBackground};
  mixer[kColorBatchUploadDataBackground] = {
      kColorProfileMenuSyncInfoBackground};
  mixer[kColorDesktopMediaTabListBorder] = {ui::kColorMidground};
  mixer[kColorDesktopMediaTabListPreviewBackground] = {ui::kColorMidground};
  mixer[kColorDownloadItemForeground] = {kColorDownloadShelfForeground};
  mixer[kColorDownloadItemForegroundDangerous] = ui::PickGoogleColor(
      ui::kColorAlertHighSeverity, kColorDownloadShelfBackground,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadItemForegroundDisabled] = BlendForMinContrast(
      ui::AlphaBlend(kColorDownloadItemForeground,
                     kColorDownloadShelfBackground, gfx::kGoogleGreyAlpha600),
      kColorDownloadShelfBackground, kColorDownloadItemForeground);
  mixer[kColorDownloadItemForegroundSafe] = ui::PickGoogleColor(
      ui::kColorAlertLowSeverity, kColorDownloadShelfBackground,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadItemIconDangerous] = {ui::kColorAlertHighSeverity};
  mixer[kColorDownloadItemIconWarning] = {ui::kColorSecondaryForeground};
  mixer[kColorDownloadItemProgressRingBackground] = ui::SetAlpha(
      kColorDownloadItemProgressRingForeground, gfx::kGoogleGreyAlpha400);
  mixer[kColorDownloadItemProgressRingForeground] = {ui::kColorThrobber};
  mixer[kColorDownloadItemTextDangerous] = {ui::kColorAlertHighSeverity};
  mixer[kColorDownloadItemTextWarning] = {ui::kColorSecondaryForeground};
  mixer[kColorDownloadShelfBackground] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelfBackground};
  mixer[kColorDownloadShelfButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadShelfButtonIconDisabled] = {
      kColorToolbarButtonIconDisabled};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelfBackground,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadShelfContentAreaSeparator] = ui::AlphaBlend(
      kColorDownloadShelfButtonIcon, kColorDownloadShelfBackground, 0x3A);
  mixer[kColorDownloadShelfForeground] = {kColorToolbarText};
  mixer[kColorDownloadStartedAnimationForeground] =
      PickGoogleColor(ui::kColorAccent, kColorDownloadShelfBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorDownloadToolbarButtonActive] =
      ui::PickGoogleColor(ui::kColorThrobber, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorDownloadToolbarButtonAnimationBackground] =
      ui::AlphaBlend(kColorDownloadToolbarButtonAnimationForeground,
                     kColorToolbar, kToolbarInkDropHighlightVisibleAlpha);
  mixer[kColorDownloadToolbarButtonAnimationForeground] =
      AdjustHighlightColorForContrast(ui::kColorAccent, kColorToolbar);
  mixer[kColorDownloadToolbarButtonInactive] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      SkColorSetA(kColorDownloadToolbarButtonInactive, 0x33)};
  mixer[kColorExtensionDialogBackground] = {SK_ColorWHITE};
  mixer[kColorExtensionIconBadgeBackgroundDefault] =
      PickGoogleColor(ui::kColorAccent, kColorToolbar,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorExtensionIconDecorationAmbientShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x26);
  mixer[kColorExtensionIconDecorationBackground] = {SK_ColorWHITE};
  mixer[kColorExtensionIconDecorationKeyShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x4D);
  mixer[kColorExtensionMenuIcon] = {ui::kColorIcon};
  mixer[kColorExtensionMenuIconDisabled] = {ui::kColorIconDisabled};
  mixer[kColorExtensionMenuPinButtonIcon] = PickGoogleColor(
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
      ui::kColorMenuBackground, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorExtensionMenuPinButtonIconDisabled] = ui::SetAlpha(
      kColorExtensionMenuPinButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorExtensionsMenuContainerBackground] = {ui::kColorSysSurface3};
  mixer[kColorExtensionsToolbarControlsBackground] = {
      kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorFeaturePromoBubbleBackground] = {gfx::kGoogleBlue700};
  mixer[kColorFeaturePromoBubbleButtonBorder] = {gfx::kGoogleGrey300};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {gfx::kGoogleBlue300};
  mixer[kColorFeaturePromoBubbleDefaultButtonBackground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFeaturePromoBubbleDefaultButtonForeground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeaturePromoBubbleForeground] = {SK_ColorWHITE};
  mixer[kColorFeatureLensPromoBubbleBackground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeatureLensPromoBubbleForeground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFindBarBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorFindBarButtonIcon] =
      ui::DeriveDefaultIconColor(ui::kColorTextfieldForeground);
  mixer[kColorFindBarButtonIconDisabled] =
      ui::DeriveDefaultIconColor(ui::kColorTextfieldForegroundDisabled);
  mixer[kColorFindBarForeground] = {ui::kColorTextfieldForeground};
  mixer[kColorFindBarMatchCount] = {ui::kColorSecondaryForeground};
  mixer[kColorFlyingIndicatorBackground] = {kColorToolbar};
  mixer[kColorFlyingIndicatorForeground] = {kColorToolbarButtonIcon};
  mixer[kColorFocusHighlightDefault] = {SkColorSetRGB(0x10, 0x10, 0x10)};
  mixer[kColorFrameCaptionActive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameActive});
  mixer[kColorFrameCaptionInactive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameInactive});
  mixer[kColorInfoBarBackground] = {kColorToolbar};
  mixer[kColorInfoBarButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorInfoBarButtonIconDisabled] = {kColorToolbarButtonIconDisabled};
  mixer[kColorInfoBarContentAreaSeparator] =
      ui::AlphaBlend(kColorInfoBarButtonIcon, kColorInfoBarBackground, 0x3A);
  mixer[kColorInfoBarForeground] = {kColorToolbarText};
  // kColorInfoBarIcon is referenced in //components/infobars, so
  // we can't use a color id from the chrome namespace. Here we're
  // overriding the default color with something more suitable.
  if (base::FeatureList::IsEnabled(features::kInfoBarIconMonochrome)) {
    mixer[ui::kColorInfoBarIcon] = {kColorToolbarButtonIcon};
  } else {
    mixer[ui::kColorInfoBarIcon] =
        ui::PickGoogleColor(ui::kColorAccent, kColorInfoBarBackground,
                            color_utils::kMinimumVisibleContrastRatio);
  }
  mixer[kColorIntentPickerItemBackgroundHovered] = ui::SetAlpha(
      ui::GetColorWithMaxContrast(ui::kColorDialogBackground), 0x0F);  // 6%.
  mixer[kColorIntentPickerItemBackgroundSelected] = ui::BlendForMinContrast(
      ui::kColorDialogBackground, ui::kColorDialogBackground,
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground, 1.2);

  mixer[kColorHoverButtonBackgroundHovered] = {ui::kColorSysStateHoverOnSubtle};

  // By default, the Omnibox background color will be determined by the toolbar
  // color.
  mixer[kColorLocationBarBackground] = {kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorLocationBarBackgroundHovered] = {
      kColorToolbarBackgroundSubtleEmphasisHovered};

  mixer[kColorLocationBarBorder] = {SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorLocationBarBorderOpaque] =
      ui::GetResultingPaintColor(kColorLocationBarBorder, kColorToolbar);

  mixer[kColorLocationBarBorderOnMismatch] = {kColorLocationBarBorder};

  // Override Omnibox background color tokens per GM3 spec when appropriate.
  ApplyGM3OmniboxBackgroundColor(mixer, key);

  mixer[kColorMediaRouterIconActive] =
      PickGoogleColor(ui::kColorAccent, kColorToolbar,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorMediaRouterIconWarning] = {ui::kColorAlertMediumSeverityIcon};
  mixer[kColorOmniboxChipBackground] = {kColorTabBackgroundActiveFrameActive};
  mixer[kColorOmniboxChipBlockedActivityIndicatorBackground] = {
      kColorInfoBarBackground};
  mixer[kColorOmniboxChipBlockedActivityIndicatorForeground] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorOmniboxChipInUseActivityIndicatorBackground] = {
      ui::kColorButtonBackgroundProminent};
  mixer[kColorOmniboxChipInUseActivityIndicatorForeground] = {
      ui::kColorButtonForegroundProminent};
  mixer[kColorOmniboxChipOnSystemBlockedActivityIndicatorBackground] = {
      kColorInfoBarBackground};
  mixer[kColorOmniboxChipOnSystemBlockedActivityIndicatorForeground] =
      ui::PickGoogleColor(
          ui::kColorSysError,
          kColorOmniboxChipOnSystemBlockedActivityIndicatorBackground,
          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorOmniboxChipForegroundLowVisibility] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = ui::PickGoogleColor(
      ui::kColorButtonForeground, kColorOmniboxChipBackground,
      color_utils::kMaximumPossibleContrast);
  // This color ID is only for Material Refresh 2023, but is a fallback when
  // themes are used.
  mixer[kColorOmniboxChipInkDropHover] = {
      ui::SetAlpha(kColorToolbarButtonIcon, std::ceil(0.10f * 255.0f))};
  mixer[kColorOmniboxChipInkDropRipple] = {
      ui::SetAlpha(kColorToolbarButtonIcon, std::ceil(0.16f * 255.0f))};
  mixer[kColorOmniboxIntentChipBackground] = {
      ui::kColorSysBaseContainerElevated};
  mixer[kColorOmniboxIntentChipIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorPageInfoChosenObjectDeleteButtonIcon] = {ui::kColorIcon};
  mixer[kColorPageInfoChosenObjectDeleteButtonIconDisabled] = {
      ui::kColorIconDisabled};
  mixer[kColorPaymentsFeedbackTipBackground] = {
      ui::kColorSubtleEmphasisBackground};
  mixer[kColorPaymentsFeedbackTipBorder] = {ui::kColorBubbleFooterBorder};
  mixer[kColorPaymentsFeedbackTipForeground] = {
      ui::kColorLabelForegroundSecondary};
  mixer[kColorPaymentsFeedbackTipIcon] = {ui::kColorAlertMediumSeverityIcon};
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  mixer[kColorPaymentsGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                                  : gfx::kGoogleGrey700};
#endif
  mixer[kColorPaymentsPromoCodeBackground] = {
      dark_mode ? SkColorSetA(gfx::kGoogleGreen300, 0x1F)
                : gfx::kGoogleGreen050};
  mixer[kColorPaymentsPromoCodeForeground] = {dark_mode ? gfx::kGoogleGreen300
                                                        : gfx::kGoogleGreen800};
  mixer[kColorPaymentsPromoCodeForegroundHovered] = {
      dark_mode ? gfx::kGoogleGreen200 : gfx::kGoogleGreen900};
  mixer[kColorPaymentsPromoCodeForegroundPressed] = {
      kColorPaymentsPromoCodeForegroundHovered};
  mixer[kColorPaymentsPromoCodeInkDrop] = {dark_mode ? gfx::kGoogleGreen300
                                                     : gfx::kGoogleGreen600};
  mixer[kColorPaymentsRequestBackArrowButtonIcon] = {ui::kColorIcon};
  mixer[kColorPaymentsRequestBackArrowButtonIconDisabled] = {
      ui::kColorIconDisabled};
  mixer[kColorPaymentsRequestRowBackgroundHighlighted] = {
      SkColorSetA(SK_ColorBLACK, 0x0D)};
  mixer[kColorPerformanceInterventionButtonIconActive] =
      ui::PickGoogleColor(ui::kColorThrobber, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorPerformanceInterventionButtonIconInactive] = {
      kColorToolbarButtonIcon};
  mixer[kColorPipWindowBackToTabButtonBackground] = {
      SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorPipWindowBackground] = {SK_ColorBLACK};
  mixer[kColorPipWindowControlsBackground] = {
      SkColorSetA(gfx::kGoogleGrey900, 0xC1)};
  mixer[kColorPipWindowTopBarBackground] = {gfx::kGoogleGrey900};
  mixer[kColorPipWindowForeground] =
      ui::GetColorWithMaxContrast(kColorPipWindowBackground);
  mixer[kColorPipWindowForegroundInactive] = {gfx::kGoogleGrey500};
  mixer[kColorPipWindowHangUpButtonForeground] = {gfx::kGoogleRed300};
  mixer[kColorPipWindowSkipAdButtonBackground] = {gfx::kGoogleGrey700};
  mixer[kColorPipWindowSkipAdButtonBorder] = {kColorPipWindowForeground};
  mixer[kColorProfileMenuBackground] = {ui::kColorDialogBackground};
  mixer[kColorProfileMenuSyncInfoBackground] = {ui::kColorSyncInfoBackground};
  // TODO(crbug.com/40833357): stop forcing the light theme once the
  // reauth dialog supports the dark mode.
  mixer[kColorProfilesReauthDialogBorder] = {SK_ColorWHITE};
  mixer[kColorQrCodeBackground] = {SK_ColorWHITE};
  mixer[kColorQrCodeBorder] = {ui::kColorMidground};
  mixer[kColorQuickAnswersReportQueryButtonBackground] = ui::SetAlpha(
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground, 0x0A);
  mixer[kColorQuickAnswersReportQueryButtonForeground] = PickGoogleColor(
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
      ui::GetResultingPaintColor(kColorQuickAnswersReportQueryButtonBackground,
                                 ui::kColorPrimaryBackground),
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorScreenshotCapturedImageBackground] = {ui::kColorBubbleBackground};
  mixer[kColorScreenshotCapturedImageBorder] = {ui::kColorMidground};
  mixer[kColorShareThisTabAudioToggleBackground] = {
      ui::kColorSubtleEmphasisBackground};
  mixer[kColorShareThisTabSourceViewBorder] = {ui::kColorMidground};
  mixer[kColorShoppingPageActionIconBackgroundVariant] = {
      ui::kColorSysSecondary};
  mixer[kColorShoppingPageActionIconForegroundVariant] = {
      ui::kColorSysOnSecondary};
  mixer[kColorSidePanelBackground] = {kColorToolbar};
  mixer[kColorSidePanelContentAreaSeparator] = {ui::kColorSeparator};
  mixer[kColorSidePanelComboboxEntryIcon] = {ui::kColorIcon};
  mixer[kColorSidePanelEntryIcon] = {ui::kColorIcon};
  mixer[kColorSidePanelEntryTitle] = {ui::kColorLabelForeground};
  mixer[kColorSidePanelEntryDropdownIcon] = {ui::kColorIcon};
  mixer[kColorSidePanelHeaderButtonIcon] = {ui::kColorIcon};
  mixer[kColorSidePanelHeaderButtonIconDisabled] = {ui::kColorIconDisabled};
  mixer[kColorSidePanelResizeAreaHandle] = {kColorToolbarContentAreaSeparator};
  mixer[kColorStatusBubbleBackgroundFrameActive] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorStatusBubbleBackgroundFrameInactive] = {
      kColorTabBackgroundInactiveFrameInactive};
  mixer[kColorStatusBubbleForegroundFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorStatusBubbleForegroundFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorStatusBubbleShadow] = {SkColorSetA(SK_ColorBLACK, 0x1E)};
  mixer[kColorTabAlertAudioPlayingActiveFrameActive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabAlertAudioPlayingActiveFrameInactive] = {
      kColorTabForegroundActiveFrameInactive};
  mixer[kColorTabAlertAudioPlayingInactiveFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorTabAlertAudioPlayingInactiveFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorTabAlertMediaRecordingActiveFrameActive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundActiveFrameActive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingActiveFrameInactive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundActiveFrameInactive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingInactiveFrameActive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundInactiveFrameActive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingInactiveFrameInactive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundInactiveFrameInactive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertPipPlayingActiveFrameActive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundActiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingActiveFrameInactive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundActiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingInactiveFrameActive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingInactiveFrameInactive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundInactiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorHoverCardTabAlertAudioPlayingIcon] = ui::SelectBasedOnDarkInput(
      ui::kColorBubbleFooterBackground, SK_ColorWHITE, gfx::kGoogleGrey800);
  mixer[kColorHoverCardTabAlertMediaRecordingIcon] = ui::SelectBasedOnDarkInput(
      ui::kColorBubbleFooterBackground, gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorHoverCardTabAlertPipPlayingIcon] =
      ui::PickGoogleColor(ui::kColorAccent, ui::kColorBubbleFooterBackground,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabCloseButtonFocusRingActive] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundActiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabCloseButtonFocusRingInactive] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabFocusRingActive] = ui::PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundActiveFrameActive,
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabFocusRingInactive] = ui::PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundInactiveFrameActive,
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);

  mixer[kColorTabGroupTabStripFrameActiveBlue] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorTabGroupTabStripFrameActiveCyan] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorTabGroupTabStripFrameActiveGreen] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorTabGroupTabStripFrameActiveGrey] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorTabGroupTabStripFrameActiveOrange] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorTabGroupTabStripFrameActivePink] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorTabGroupTabStripFrameActivePurple] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorTabGroupTabStripFrameActiveRed] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupTabStripFrameActiveYellow] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorTabGroupTabStripFrameInactiveBlue] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorTabGroupTabStripFrameInactiveCyan] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorTabGroupTabStripFrameInactiveGreen] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorTabGroupTabStripFrameInactiveGrey] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorTabGroupTabStripFrameInactiveOrange] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorTabGroupTabStripFrameInactivePink] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorTabGroupTabStripFrameInactivePurple] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorTabGroupTabStripFrameInactiveRed] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupTabStripFrameInactiveYellow] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorTabGroupDialogBlue] = {kColorTabGroupContextMenuBlue};
  mixer[kColorTabGroupDialogCyan] = {kColorTabGroupContextMenuCyan};
  mixer[kColorTabGroupDialogGreen] = {kColorTabGroupContextMenuGreen};
  mixer[kColorTabGroupDialogGrey] = {kColorTabGroupContextMenuGrey};
  mixer[kColorTabGroupDialogOrange] = {kColorTabGroupContextMenuOrange};
  mixer[kColorTabGroupDialogPink] = {kColorTabGroupContextMenuPink};
  mixer[kColorTabGroupDialogPurple] = {kColorTabGroupContextMenuPurple};
  mixer[kColorTabGroupDialogRed] = {kColorTabGroupContextMenuRed};
  mixer[kColorTabGroupDialogYellow] = {kColorTabGroupContextMenuYellow};

  mixer[kColorTabGroupContextMenuBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleBlue300,
      gfx::kGoogleBlue600);
  mixer[kColorTabGroupContextMenuCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleCyan300,
      gfx::kGoogleCyan900);
  mixer[kColorTabGroupContextMenuGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleGreen300,
      gfx::kGoogleGreen700);
  mixer[kColorTabGroupContextMenuGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleGrey300,
      gfx::kGoogleGrey700);
  mixer[kColorTabGroupContextMenuOrange] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleOrange300,
      gfx::kGoogleOrange400);
  mixer[kColorTabGroupContextMenuPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGooglePink300,
      gfx::kGooglePink700);
  mixer[kColorTabGroupContextMenuPurple] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGooglePurple300,
      gfx::kGooglePurple500);
  mixer[kColorTabGroupContextMenuRed] =
      SelectColorBasedOnDarkInputOrMode(dark_mode, kColorBookmarkBarForeground,
                                        gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupContextMenuYellow] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleYellow300,
      gfx::kGoogleYellow600);

  mixer[kColorSavedTabGroupForegroundBlue] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleBlue100, gfx::kGoogleBlue700);
  mixer[kColorSavedTabGroupForegroundGrey] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleGrey100, gfx::kGoogleGrey700);
  mixer[kColorSavedTabGroupForegroundRed] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleRed100, gfx::kGoogleRed700);
  mixer[kColorSavedTabGroupForegroundGreen] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleGreen100, gfx::kGoogleGreen800);
  mixer[kColorSavedTabGroupForegroundYellow] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleYellow100, gfx::kGoogleGrey800);
  mixer[kColorSavedTabGroupForegroundCyan] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleCyan100, gfx::kGoogleCyan900);
  mixer[kColorSavedTabGroupForegroundPurple] =
      ui::SelectBasedOnDarkInput(kColorBookmarkBarBackground,
                                 gfx::kGooglePurple100, gfx::kGooglePurple700);
  mixer[kColorSavedTabGroupForegroundPink] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGooglePink100, gfx::kGooglePink800);
  mixer[kColorSavedTabGroupForegroundOrange] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleOrange100, gfx::kGoogleGrey800);

  mixer[kColorSavedTabGroupOutlineBlue] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleBlue300, gfx::kGoogleBlue700);
  mixer[kColorSavedTabGroupOutlineGrey] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorSavedTabGroupOutlineRed] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleRed300, gfx::kGoogleRed700);
  mixer[kColorSavedTabGroupOutlineGreen] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleGreen300, gfx::kGoogleGreen800);
  mixer[kColorSavedTabGroupOutlineYellow] =
      ui::SelectBasedOnDarkInput(kColorBookmarkBarBackground,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);
  mixer[kColorSavedTabGroupOutlineCyan] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorSavedTabGroupOutlinePurple] =
      ui::SelectBasedOnDarkInput(kColorBookmarkBarBackground,
                                 gfx::kGooglePurple300, gfx::kGooglePurple700);
  mixer[kColorSavedTabGroupOutlinePink] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorSavedTabGroupOutlineOrange] =
      ui::SelectBasedOnDarkInput(kColorBookmarkBarBackground,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange800);
  mixer[kColorTabGroupBookmarkBarBlue] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x39, 0x43, 0x54),
      gfx::kGoogleBlue050);
  mixer[kColorTabGroupBookmarkBarCyan] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x35, 0x4C, 0x51),
      gfx::kGoogleCyan050);
  mixer[kColorTabGroupBookmarkBarGreen] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x37, 0x48, 0x3C),
      gfx::kGoogleGreen050);
  mixer[kColorTabGroupBookmarkBarGrey] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x4C, 0x4D, 0x4E),
      gfx::kGoogleGrey100);
  mixer[kColorTabGroupBookmarkBarPink] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x55, 0x39, 0x49),
      gfx::kGooglePink050);
  mixer[kColorTabGroupBookmarkBarPurple] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x47, 0x39, 0x54),
      gfx::kGooglePurple050);
  mixer[kColorTabGroupBookmarkBarRed] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x52, 0x39, 0x37),
      gfx::kGoogleRed050);
  mixer[kColorTabGroupBookmarkBarYellow] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x55, 0x4B, 0x30),
      gfx::kGoogleYellow050);
  mixer[kColorTabGroupBookmarkBarOrange] = ui::SelectBasedOnDarkInput(
      kColorBookmarkBarBackground, SkColorSetRGB(0x54, 0x42, 0x33),
      gfx::kGoogleOrange050);

  mixer[kColorTabHoverCardBackground] = {dark_mode ? gfx::kGoogleGrey900
                                                   : gfx::kGoogleGrey050};
  mixer[kColorTabHoverCardForeground] = {dark_mode ? gfx::kGoogleGrey700
                                                   : gfx::kGoogleGrey300};
  mixer[kColorTabHoverCardSecondaryText] = {ui::kColorLabelForeground};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorTabstripLoadingProgressBackground] = ui::AlphaBlend(
      kColorTabstripLoadingProgressForeground, kColorToolbar, 0x32);
  // 4.5 and 6.0 approximate the default light and dark theme contrasts of
  // accent-against-toolbar.
  mixer[kColorTabstripLoadingProgressForeground] =
      PickGoogleColor(ui::kColorAccent, kColorToolbar, 4.5f, 6.0f);
  mixer[kColorTabstripScrollContainerShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x4D);
  mixer[kColorTabThrobber] = {ui::kColorThrobber};
  mixer[kColorTabThrobberPreconnect] = {ui::kColorThrobberPreconnect};
  mixer[kColorThumbnailTabBackground] =
      ui::PickGoogleColor(ui::kColorAccent, ui::kColorFrameActive,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorThumbnailTabForeground] =
      ui::GetColorWithMaxContrast(kColorThumbnailTabBackground);
  mixer[kColorThumbnailTabStripBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorThumbnailTabStripBackgroundInactive] = {ui::kColorFrameInactive};
  mixer[kColorThumbnailTabStripTabGroupFrameActiveBlue] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveCyan] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveGreen] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveGrey] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveOrange] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorThumbnailTabStripTabGroupFrameActivePink] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorThumbnailTabStripTabGroupFrameActivePurple] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveRed] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveYellow] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveBlue] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveCyan] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveGreen] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveGrey] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveOrange] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorThumbnailTabStripTabGroupFrameInactivePink] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactivePurple] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveRed] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveYellow] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorToolbar] = {dark_mode ? SkColorSetRGB(0x35, 0x36, 0x3A)
                                    : SK_ColorWHITE};
  mixer[kColorToolbarButtonBackgroundHighlightedDefault] =
      ui::SetAlpha(ui::GetColorWithMaxContrast(kColorToolbarButtonText), 0xCC);
  mixer[kColorAvatarButtonHighlightIncognito] = {
      kColorToolbarButtonBackgroundHighlightedDefault};
  mixer[kColorAvatarButtonHighlightDefault] = {kColorToolbar};
  mixer[kColorAvatarButtonHighlightNormalForeground] =
      AdjustHighlightColorForContrast(ui::kColorAccent, kColorToolbar);
  mixer[kColorAvatarButtonHighlightDefaultForeground] = {
      kColorToolbarButtonText};
  mixer[kColorAvatarButtonHighlightSyncErrorForeground] =
      AdjustHighlightColorForContrast(ui::kColorAlertMediumSeverityIcon,
                                      kColorToolbar);
  mixer[kColorAvatarButtonHighlightIncognitoForeground] = {
      kColorToolbarButtonText};
  mixer[kColorAvatarButtonIncognitoHover] = {kColorToolbarInkDropHover};
  mixer[kColorAvatarButtonNormalRipple] = {kColorToolbarInkDropRipple};
  mixer[kColorToolbarButtonBorder] = ui::SetAlpha(kColorToolbarInkDrop, 0x20);
  mixer[kColorToolbarButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorToolbarButtonIconDefault] = ui::HSLShift(
      gfx::kGoogleGrey700, GetThemeTint(ThemeProperties::TINT_BUTTONS, key));
  mixer[kColorToolbarButtonIconDisabled] =
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kGoogleGreyAlpha500)};
  mixer[kColorToolbarButtonIconPressed] = {kColorToolbarButtonIconHovered};
  mixer[kColorToolbarButtonText] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarContentAreaSeparator] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorToolbar, 0x3A);
  mixer[kColorToolbarFeaturePromoHighlight] =
      ui::PickGoogleColor(ui::kColorAccent, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorToolbarIconContainerBorder] = {kColorToolbarButtonBorder};
  mixer[kColorToolbarInkDrop] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarInkDropHover] =
      ui::SetAlpha(kColorToolbarInkDrop, kToolbarInkDropHighlightVisibleAlpha);
  mixer[kColorToolbarInkDropRipple] =
      ui::SetAlpha(kColorToolbarInkDrop, std::ceil(0.06f * 255.0f));
  mixer[kColorAppMenuChipInkDropHover] = {kColorToolbarInkDropHover};
  mixer[kColorAppMenuChipInkDropRipple] = {kColorToolbarInkDropRipple};
  mixer[kColorToolbarExtensionSeparatorEnabled] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorToolbarExtensionSeparatorDisabled] = {
      kColorToolbarButtonIconInactive};
  mixer[kColorToolbarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorToolbarActionItemEngaged] = {ui::kColorSysPrimary};
  mixer[kColorToolbarSeparatorDefault] =
      ui::SetAlpha(kColorToolbarButtonIcon, 0x4D);
  mixer[kColorToolbarText] = {kColorToolbarTextDefault};
  mixer[kColorToolbarTextDefault] = {dark_mode ? SK_ColorWHITE
                                               : gfx::kGoogleGrey800};
  mixer[kColorToolbarTextDisabled] = {kColorToolbarTextDisabledDefault};
  mixer[kColorToolbarTextDisabledDefault] =
      ui::SetAlpha(kColorToolbarText, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameActive);
  mixer[kColorToolbarTopSeparatorFrameInactive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameInactive);
  mixer[kColorWebContentsBackground] = {kColorNewTabPageBackground};
  mixer[kColorWebContentsBackgroundLetterboxing] =
      ui::AlphaBlend(kColorWebContentsBackground, SK_ColorBLACK, 0x33);
  mixer[kColorWindowControlButtonBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorWindowControlButtonBackgroundInactive] = {
      ui::kColorFrameInactive};

  mixer[kColorReadAnythingBackground] = {
      dark_mode ? kColorReadAnythingBackgroundDark
                : kColorReadAnythingBackgroundLight};
  mixer[kColorReadAnythingBackgroundBlue] = {gfx::kGoogleBlue100};
  mixer[kColorReadAnythingBackgroundDark] = {gfx::kGoogleGrey900};
  mixer[kColorReadAnythingBackgroundLight] = {SK_ColorWHITE};
  mixer[kColorReadAnythingBackgroundYellow] = {gfx::kGoogleYellow100};
  // The Read Anything themes need to be hard coded because they do not
  // change with the chrome theme, which is the purpose of the Read Anything
  // feature.
  mixer[kColorReadAnythingCurrentReadAloudHighlight] = {
      dark_mode ? kColorReadAnythingCurrentReadAloudHighlightDark
                : kColorReadAnythingCurrentReadAloudHighlightLight};
  mixer[kColorReadAnythingCurrentReadAloudHighlightDark] = {
      SkColorSetARGB(46, 253, 252, 251)};
  mixer[kColorReadAnythingCurrentReadAloudHighlightLight] = {
      SkColorSetARGB(46, 6, 46, 111)};
  mixer[kColorReadAnythingCurrentReadAloudHighlightBlue] = {
      SkColorSetARGB(46, 6, 46, 111)};
  mixer[kColorReadAnythingCurrentReadAloudHighlightYellow] = {
      SkColorSetARGB(46, 6, 46, 111)};
  mixer[kColorReadAnythingForeground] = {
      dark_mode ? kColorReadAnythingForegroundDark
                : kColorReadAnythingForegroundLight};
  mixer[kColorReadAnythingForegroundBlue] = {SkColorSetRGB(31, 31, 31)};
  mixer[kColorReadAnythingForegroundDark] = {SkColorSetRGB(227, 227, 227)};
  mixer[kColorReadAnythingForegroundLight] = {SkColorSetRGB(31, 31, 31)};
  mixer[kColorReadAnythingForegroundYellow] = {SkColorSetRGB(31, 31, 31)};
  mixer[kColorReadAnythingPreviousReadAloudHighlight] = {
      dark_mode ? kColorReadAnythingPreviousReadAloudHighlightDark
                : kColorReadAnythingPreviousReadAloudHighlightLight};
  mixer[kColorReadAnythingPreviousReadAloudHighlightDark] = {
      SkColorSetRGB(199, 199, 199)};
  mixer[kColorReadAnythingPreviousReadAloudHighlightLight] = {
      SkColorSetRGB(71, 71, 71)};
  mixer[kColorReadAnythingPreviousReadAloudHighlightBlue] = {
      SkColorSetRGB(71, 71, 71)};
  mixer[kColorReadAnythingPreviousReadAloudHighlightYellow] = {
      SkColorSetRGB(71, 71, 71)};
  mixer[kColorReadAnythingSeparator] = {dark_mode
                                            ? kColorReadAnythingSeparatorDark
                                            : kColorReadAnythingSeparatorLight};
  mixer[kColorReadAnythingSeparatorBlue] = {gfx::kGoogleGrey500};
  mixer[kColorReadAnythingSeparatorDark] = {gfx::kGoogleGrey800};
  mixer[kColorReadAnythingSeparatorLight] = {gfx::kGoogleGrey300};
  mixer[kColorReadAnythingSeparatorYellow] = {gfx::kGoogleGrey500};
  mixer[kColorReadAnythingDropdownBackground] = {
      dark_mode ? kColorReadAnythingDropdownBackgroundDark
                : kColorReadAnythingDropdownBackgroundLight};
  mixer[kColorReadAnythingDropdownBackgroundBlue] = {gfx::kGoogleBlue100};
  mixer[kColorReadAnythingDropdownBackgroundDark] = {gfx::kGoogleGrey900};
  mixer[kColorReadAnythingDropdownBackgroundLight] = {SK_ColorWHITE};
  mixer[kColorReadAnythingDropdownBackgroundYellow] = {gfx::kGoogleYellow050};
  mixer[kColorReadAnythingDropdownSelected] = {
      dark_mode ? kColorReadAnythingDropdownSelectedDark
                : kColorReadAnythingDropdownSelectedLight};
  mixer[kColorReadAnythingDropdownSelectedBlue] = {gfx::kGoogleBlue200};
  mixer[kColorReadAnythingDropdownSelectedDark] = {gfx::kGoogleGrey800};
  mixer[kColorReadAnythingDropdownSelectedLight] = {gfx::kGoogleGrey200};
  mixer[kColorReadAnythingDropdownSelectedYellow] = {gfx::kGoogleYellow200};
  mixer[kColorReadAnythingTextSelection] = {
      dark_mode ? kColorReadAnythingTextSelectionDark
                : kColorReadAnythingTextSelectionLight};
  mixer[kColorReadAnythingTextSelectionBlue] = {gfx::kGoogleYellow100};
  mixer[kColorReadAnythingTextSelectionDark] = {gfx::kGoogleBlue200};
  mixer[kColorReadAnythingTextSelectionLight] = {gfx::kGoogleYellow100};
  mixer[kColorReadAnythingTextSelectionYellow] = {gfx::kGoogleBlue100};
  mixer[kColorReadAnythingLinkDefault] = {
      dark_mode ? kColorReadAnythingLinkDefaultDark
                : kColorReadAnythingLinkDefaultLight};
  mixer[kColorReadAnythingLinkDefaultBlue] = {gfx::kGoogleBlue900};
  mixer[kColorReadAnythingLinkDefaultDark] = {gfx::kGoogleBlue300};
  mixer[kColorReadAnythingLinkDefaultLight] = {gfx::kGoogleBlue800};
  mixer[kColorReadAnythingLinkDefaultYellow] = {gfx::kGoogleBlue900};
  mixer[kColorReadAnythingLinkVisited] = {
      dark_mode ? kColorReadAnythingLinkVisitedDark
                : kColorReadAnythingLinkVisitedLight};
  mixer[kColorReadAnythingLinkVisitedBlue] = {gfx::kGooglePurple900};
  mixer[kColorReadAnythingLinkVisitedDark] = {gfx::kGooglePurple200};
  mixer[kColorReadAnythingLinkVisitedLight] = {gfx::kGooglePurple900};
  mixer[kColorReadAnythingLinkVisitedYellow] = {gfx::kGooglePurple900};
  // Read Anything UX has decided to prefer hard-coded blue values over the
  // adaptive focus ring color to ensure that the contrast with our custom
  // colors is always correct.
  // TODO(b/1266555): Expose a dark/light mode independent focus ring color here
  // for calculating these colors in Read Anything. This will be much easier
  // after the Chrome Refresh project has fully rolled out.
  mixer[kColorReadAnythingFocusRingBackground] = {
      dark_mode ? kColorReadAnythingFocusRingBackgroundDark
                : kColorReadAnythingFocusRingBackgroundLight};
  mixer[kColorReadAnythingFocusRingBackgroundBlue] =
      ui::PickGoogleColor(gfx::kGoogleBlue500, kColorReadAnythingBackgroundBlue,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorReadAnythingFocusRingBackgroundDark] =
      ui::PickGoogleColor(gfx::kGoogleBlue300, kColorReadAnythingBackgroundDark,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorReadAnythingFocusRingBackgroundLight] = ui::PickGoogleColor(
      gfx::kGoogleBlue500, kColorReadAnythingBackgroundLight,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorReadAnythingFocusRingBackgroundYellow] = ui::PickGoogleColor(
      gfx::kGoogleBlue500, kColorReadAnythingBackgroundYellow,
      color_utils::kMinimumVisibleContrastRatio);

  // Apply high contrast recipes if necessary.
  if (!ShouldApplyHighContrastColors(key)) {
    return;
  }
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] = {kColorToolbarText};
  mixer[kColorToolbar] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarText};
  mixer[kColorToolbarText] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};
  mixer[kColorToolbarTopSeparatorFrameActive] = {dark_mode ? SK_ColorDKGRAY
                                                           : SK_ColorLTGRAY};
  mixer[ui::kColorFrameActive] = {SK_ColorDKGRAY};
  mixer[ui::kColorFrameInactive] = {SK_ColorGRAY};
}
