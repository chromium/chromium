// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

ui::ColorTransform AdjustHighlightColorForContrast(
    ui::ColorTransform background,
    ui::ColorTransform desired_dark,
    ui::ColorTransform desired_light,
    ui::ColorTransform dark_extreme,
    ui::ColorTransform light_extreme) {
  const auto generator = [](ui::ColorTransform background,
                            ui::ColorTransform desired_dark,
                            ui::ColorTransform desired_light,
                            ui::ColorTransform dark_extreme,
                            ui::ColorTransform light_extreme,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor background_color = background.Run(input_color, mixer);
    const SkColor desired_dark_color = desired_dark.Run(input_color, mixer);
    const SkColor desired_light_color = desired_light.Run(input_color, mixer);
    const SkColor dark_extreme_color = dark_extreme.Run(input_color, mixer);
    const SkColor light_extreme_color = light_extreme.Run(input_color, mixer);
    const SkColor contrasting_color = color_utils::PickContrastingColor(
        desired_dark_color, desired_light_color, background_color);
    const SkColor limit_color = contrasting_color == desired_dark_color
                                    ? dark_extreme_color
                                    : light_extreme_color;
    // Setting highlight color will set the text to the highlight color, and the
    // background to the same color with a low alpha. This means that our target
    // contrast is between the text (the highlight color) and a blend of the
    // highlight color and the toolbar color.
    const SkColor base_color = color_utils::AlphaBlend(
        contrasting_color, background_color, SkAlpha{0x20});

    // Add a fudge factor to the minimum contrast ratio since we'll actually be
    // blending with the adjusted color.
    const SkColor result_color =
        color_utils::BlendForMinContrast(
            contrasting_color, base_color, limit_color,
            color_utils::kMinimumReadableContrastRatio * 1.05)
            .color;
    DVLOG(2) << "ColorTransform AdjustHighlightColorForContrast:"
             << " Background: " << ui::SkColorName(background_color)
             << " Desired Dark: " << ui::SkColorName(desired_dark_color)
             << " Desired Light: " << ui::SkColorName(desired_light_color)
             << " Dark Extreme: " << ui::SkColorName(dark_extreme_color)
             << " Light Extreme: " << ui::SkColorName(light_extreme_color)
             << " Contrasting Color: " << ui::SkColorName(contrasting_color)
             << " Limit Color: " << ui::SkColorName(limit_color)
             << " Base Color: " << ui::SkColorName(base_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(background),
                             std::move(desired_dark), std::move(desired_light),
                             std::move(dark_extreme), std::move(light_extreme));
}

ui::ColorTransform IncreaseLightness(ui::ColorTransform input_transform,
                                     double percent) {
  const auto generator = [](ui::ColorTransform input_transform, double percent,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor color = input_transform.Run(input_color, mixer);
    color_utils::HSL result;
    color_utils::SkColorToHSL(color, &result);
    result.l += (1 - result.l) * percent;
    const SkColor result_color =
        color_utils::HSLToSkColor(result, SkColorGetA(color));
    DVLOG(2) << "ColorTransform IncreaseLightness:"
             << " Percent: " << percent
             << " Transform Color: " << ui::SkColorName(color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(input_transform), percent);
}

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

// Flat version of dark mode colors used in bookmarks bar to fill
// the buttons.
constexpr SkColor kFlatGrey = SkColorSetRGB(0x5D, 0x5E, 0x62);
constexpr SkColor kFlatBlue = SkColorSetRGB(0x49, 0x54, 0x68);
constexpr SkColor kFlatRed = SkColorSetRGB(0x62, 0x4A, 0x4B);
constexpr SkColor kFlatGreen = SkColorSetRGB(0x47, 0x59, 0x50);
constexpr SkColor kFlatYellow = SkColorSetRGB(0x65, 0x5C, 0x44);
constexpr SkColor kFlatCyan = SkColorSetRGB(0x45, 0x5D, 0x65);
constexpr SkColor kFlatPurple = SkColorSetRGB(0x58, 0x4A, 0x68);
constexpr SkColor kFlatPink = SkColorSetRGB(0x65, 0x4A, 0x5D);

// Default toolbar colors.
constexpr SkColor kDarkToolbarColor = SkColorSetRGB(0x35, 0x36, 0x3A);
constexpr SkColor kLightToolbarColor = SK_ColorWHITE;

// Alpha of 61 = 24%. From GetTabGroupColors() in theme_helper.cc.
constexpr SkAlpha tab_group_chip_alpha = 61;

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAppMenuHighlightSeverityLow] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleGreen600, gfx::kGoogleGreen300,
      gfx::kGoogleGreen900, gfx::kGoogleGreen050);
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      kColorAvatarButtonHighlightSyncError};
  mixer[kColorAppMenuHighlightSeverityMedium] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleYellow600,
      gfx::kGoogleYellow300, gfx::kGoogleYellow900, gfx::kGoogleYellow050);
  mixer[kColorAvatarButtonHighlightNormal] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleBlue600, gfx::kGoogleBlue300,
      gfx::kGoogleBlue900, gfx::kGoogleBlue050);
  mixer[kColorAvatarButtonHighlightSyncError] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleRed600, gfx::kGoogleRed300,
      gfx::kGoogleRed900, gfx::kGoogleRed050);
  mixer[kColorAvatarButtonHighlightSyncPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarStrokeLight] = {SK_ColorWHITE};
  mixer[kColorBookmarkBarBackground] = {kColorToolbar};
  mixer[kColorBookmarkBarForeground] = {kColorToolbarText};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIcon};
  // If the custom theme supplies a specific color for the bookmark text, use
  // that color to derive folder icon color. We don't actually use the color
  // returned, rather we use the color provider color transform corresponding to
  // that color.
  SkColor color;
  const bool custom_icon_color =
      key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON,
                                 &color);
  mixer[kColorBookmarkFavicon] =
      custom_icon_color ? ui::ColorTransform(kColorToolbarButtonIcon)
                        : ui::ColorTransform(SK_ColorTRANSPARENT);
  const bool custom_text_color =
      key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT, &color);
  mixer[kColorBookmarkFolderIcon] =
      custom_text_color
          ? ui::DeriveDefaultIconColor(kColorBookmarkBarForeground)
          : ui::ColorTransform(ui::kColorIcon);
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparator};
  mixer[kColorCaptionButtonBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorDesktopMediaTabListBorder] = {ui::kColorMidground};
  mixer[kColorDesktopMediaTabListPreviewBackground] = {ui::kColorMidground};
  mixer[kColorDownloadShelfBackground] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelfBackground};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelfBackground,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadShelfContentAreaSeparator] = ui::AlphaBlend(
      kColorToolbarButtonIcon, kColorDownloadShelfBackground, 0x3A);
  mixer[kColorDownloadShelfForeground] = {kColorToolbarText};
  mixer[kColorDownloadToolbarButtonActive] = {ui::kColorThrobber};
  mixer[kColorDownloadToolbarButtonInactive] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      SkColorSetA(kColorDownloadToolbarButtonInactive, 0x33)};
  mixer[kColorEyedropperBoundary] = {SK_ColorDKGRAY};
  mixer[kColorEyedropperCentralPixelInnerRing] = {SK_ColorBLACK};
  mixer[kColorEyedropperCentralPixelOuterRing] = {SK_ColorWHITE};
  mixer[kColorEyedropperGrid] = {SK_ColorGRAY};
  mixer[kColorFeaturePromoBubbleBackground] = {gfx::kGoogleBlue700};
  mixer[kColorFeaturePromoBubbleButtonBorder] = {gfx::kGoogleGrey300};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {gfx::kGoogleBlue300};
  mixer[kColorFeaturePromoBubbleDefaultButtonBackground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFeaturePromoBubbleDefaultButtonForeground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeaturePromoBubbleForeground] = {SK_ColorWHITE};
  mixer[kColorFlyingIndicatorBackground] = {kColorToolbar};
  mixer[kColorFlyingIndicatorForeground] = {kColorToolbarButtonIcon};
  mixer[kColorFrameCaptionActive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameActive});
  mixer[kColorFrameCaptionInactive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameInactive});
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  mixer[kColorGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                          : gfx::kGoogleGrey700};
#endif
  mixer[kColorInfoBarBackground] = {kColorToolbar};
  mixer[kColorInfoBarContentAreaSeparator] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorInfoBarBackground, 0x3A);
  mixer[kColorInfoBarForeground] = {kColorToolbarText};
  mixer[kColorLocationBarBorder] = {SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorLocationBarBorderOpaque] =
      ui::GetResultingPaintColor(kColorLocationBarBorder, kColorToolbar);
  mixer[kColorNewTabPageBackground] = {kColorToolbar};
  mixer[kColorNewTabPageHeader] = {SkColorSetRGB(0x96, 0x96, 0x96)};
  mixer[kColorNewTabPageLink] = {dark_mode ? gfx::kGoogleBlue300
                                           : SkColorSetRGB(0x06, 0x37, 0x74)};
  mixer[kColorNewTabPageLogo] = {kColorNewTabPageLogoUnthemed};
  mixer[kColorNewTabPageLogoUnthemed] = {SkColorSetRGB(0xEE, 0xEE, 0xEE)};
  if (dark_mode) {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {gfx::kGoogleGrey900};
  } else {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {
        kColorNewTabPageMostVisitedTileBackgroundUnthemed};
  }
  mixer[kColorNewTabPageMostVisitedTileBackgroundUnthemed] = {
      gfx::kGoogleGrey100};
  mixer[kColorNewTabPageSectionBorder] =
      ui::SetAlpha(kColorNewTabPageHeader, 0x50);
  mixer[kColorNewTabPageText] = {dark_mode ? gfx::kGoogleGrey200
                                           : SK_ColorBLACK};
  mixer[kColorNewTabPageTextUnthemed] = {gfx::kGoogleGrey050};
  mixer[kColorNewTabPageTextLight] =
      IncreaseLightness(kColorNewTabPageText, 0.40);
  mixer[kColorOmniboxAnswerIconBackground] = {
      ui::kColorButtonBackgroundProminent};
  mixer[kColorOmniboxAnswerIconForeground] = {
      ui::kColorButtonForegroundProminent};
  mixer[kColorOmniboxBackground] = {dark_mode ? gfx::kGoogleGrey900
                                              : gfx::kGoogleGrey100};
  mixer[kColorOmniboxChipBackgroundLowVisibility] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorOmniboxChipBackgroundNormalVisibility] = {
      ui::kColorButtonBackground};
  mixer[kColorOmniboxChipForegroundLowVisibility] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = {
      ui::kColorButtonForeground};
  mixer[kColorOmniboxText] =
      ui::GetColorWithMaxContrast(kColorOmniboxBackground);
  mixer[kColorPaymentRequestRowBackgroundHighlighted] = {
      SkColorSetA(SK_ColorBLACK, 0x0D)};
  mixer[kColorPipWindowBackToTabButtonBackground] = {
      SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorPipWindowBackground] = {SK_ColorBLACK};
  mixer[kColorPipWindowForeground] =
      ui::GetColorWithMaxContrast(kColorPipWindowBackground);
  mixer[kColorPipWindowHangUpButtonForeground] = {gfx::kGoogleRed300};
  mixer[kColorPipWindowSkipAdButtonBackground] = {gfx::kGoogleGrey700};
  mixer[kColorPipWindowSkipAdButtonBorder] = {kColorPipWindowForeground};
  mixer[kColorQrCodeBackground] = {SK_ColorWHITE};
  mixer[kColorQrCodeBorder] = {ui::kColorMidground};
  mixer[kColorReadLaterButtonHighlight] = {kColorAvatarButtonHighlightNormal};
  mixer[kColorScreenshotCapturedImageBackground] = {ui::kColorBubbleBackground};
  mixer[kColorScreenshotCapturedImageBorder] = {ui::kColorMidground};
  mixer[kColorSidePanelContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorStatusBubbleBackgroundFrameActive] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorStatusBubbleBackgroundFrameInactive] = {
      kColorTabBackgroundInactiveFrameInactive};
  mixer[kColorStatusBubbleForegroundFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorStatusBubbleForegroundFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorStatusBubbleShadow] = {SkColorSetA(SK_ColorBLACK, 0x1E)};

  ui::ColorTransform input_transform = {kColorTabBackgroundInactiveFrameActive};
  mixer[kColorTabGroupTabStripFrameActiveBlue] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleBlue300}, {gfx::kGoogleBlue600});
  mixer[kColorTabGroupTabStripFrameActiveCyan] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleCyan300}, {gfx::kGoogleCyan900});
  mixer[kColorTabGroupTabStripFrameActiveGreen] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleGreen300}, {gfx::kGoogleGreen700});
  mixer[kColorTabGroupTabStripFrameActiveGrey] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleGrey300}, {gfx::kGoogleGrey700});
  mixer[kColorTabGroupTabStripFrameActiveOrange] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleOrange300}, {gfx::kGoogleOrange400});
  mixer[kColorTabGroupTabStripFrameActivePink] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGooglePink300}, {gfx::kGooglePink700});
  mixer[kColorTabGroupTabStripFrameActivePurple] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGooglePurple300}, {gfx::kGooglePurple500});
  mixer[kColorTabGroupTabStripFrameActiveRed] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleRed300}, {gfx::kGoogleRed600});
  mixer[kColorTabGroupTabStripFrameActiveYellow] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleYellow300}, {gfx::kGoogleYellow600});

  input_transform = {kColorTabBackgroundInactiveFrameInactive};
  mixer[kColorTabGroupTabStripFrameInactiveBlue] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleBlue300}, {gfx::kGoogleBlue600});
  mixer[kColorTabGroupTabStripFrameInactiveCyan] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleCyan300}, {gfx::kGoogleCyan900});
  mixer[kColorTabGroupTabStripFrameInactiveGreen] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleGreen300}, {gfx::kGoogleGreen700});
  mixer[kColorTabGroupTabStripFrameInactiveGrey] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleGrey300}, {gfx::kGoogleGrey700});
  mixer[kColorTabGroupTabStripFrameInactiveOrange] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleOrange300}, {gfx::kGoogleOrange400});
  mixer[kColorTabGroupTabStripFrameInactivePink] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGooglePink300}, {gfx::kGooglePink700});
  mixer[kColorTabGroupTabStripFrameInactivePurple] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGooglePurple300}, {gfx::kGooglePurple500});
  mixer[kColorTabGroupTabStripFrameInactiveRed] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleRed300}, {gfx::kGoogleRed600});
  mixer[kColorTabGroupTabStripFrameInactiveYellow] = ui::SelectBasedOnDarkInput(
      input_transform, {gfx::kGoogleYellow300}, {gfx::kGoogleYellow600});

  mixer[kColorTabGroupDialogBlue] = {kColorTabGroupContextMenuBlue};
  mixer[kColorTabGroupDialogCyan] = {kColorTabGroupContextMenuCyan};
  mixer[kColorTabGroupDialogGreen] = {kColorTabGroupContextMenuGreen};
  mixer[kColorTabGroupDialogGrey] = {kColorTabGroupContextMenuGrey};
  mixer[kColorTabGroupDialogOrange] = {kColorTabGroupContextMenuOrange};
  mixer[kColorTabGroupDialogPink] = {kColorTabGroupContextMenuPink};
  mixer[kColorTabGroupDialogPurple] = {kColorTabGroupContextMenuPurple};
  mixer[kColorTabGroupDialogRed] = {kColorTabGroupContextMenuRed};
  mixer[kColorTabGroupDialogYellow] = {kColorTabGroupContextMenuYellow};

  input_transform = {kColorBookmarkBarForeground};
  mixer[kColorTabGroupContextMenuBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleBlue300}, {gfx::kGoogleBlue600});
  mixer[kColorTabGroupContextMenuCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleCyan300}, {gfx::kGoogleCyan900});
  mixer[kColorTabGroupContextMenuGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleGreen300},
      {gfx::kGoogleGreen700});
  mixer[kColorTabGroupContextMenuGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleGrey300}, {gfx::kGoogleGrey700});
  mixer[kColorTabGroupContextMenuOrange] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleOrange300},
      {gfx::kGoogleOrange400});
  mixer[kColorTabGroupContextMenuPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGooglePink300}, {gfx::kGooglePink700});
  mixer[kColorTabGroupContextMenuPurple] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGooglePurple300},
      {gfx::kGooglePurple500});
  mixer[kColorTabGroupContextMenuRed] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleRed300}, {gfx::kGoogleRed600});
  mixer[kColorTabGroupContextMenuYellow] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {gfx::kGoogleYellow300},
      {gfx::kGoogleYellow600});

  mixer[kColorTabGroupBookmarkBarBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatBlue}, {gfx::kGoogleBlue050});
  mixer[kColorTabGroupBookmarkBarCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatCyan}, {gfx::kGoogleCyan050});
  mixer[kColorTabGroupBookmarkBarGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatGreen}, {gfx::kGoogleGreen050});
  mixer[kColorTabGroupBookmarkBarGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatGrey}, {gfx::kGoogleGrey100});
  mixer[kColorTabGroupBookmarkBarPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatPink}, {gfx::kGooglePink050});
  mixer[kColorTabGroupBookmarkBarPurple] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatPurple}, {gfx::kGooglePurple050});
  mixer[kColorTabGroupBookmarkBarRed] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatRed}, {gfx::kGoogleRed050});
  mixer[kColorTabGroupBookmarkBarYellow] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, {kFlatYellow}, {gfx::kGoogleYellow050});
  auto flat_orange = ui::AlphaBlend({gfx::kGoogleOrange300},
                                    {kDarkToolbarColor}, tab_group_chip_alpha);
  mixer[kColorTabGroupBookmarkBarOrange] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, input_transform, flat_orange, {gfx::kGoogleOrange050});

  mixer[kColorTabHoverCardBackground] = {dark_mode ? gfx::kGoogleGrey900
                                                   : gfx::kGoogleGrey050};
  mixer[kColorTabHoverCardForeground] = {dark_mode ? gfx::kGoogleGrey700
                                                   : gfx::kGoogleGrey300};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorThumbnailTabBackground] = ui::BlendForMinContrast(
      ui::kColorAccent, ui::kColorFrameActive, absl::nullopt,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorThumbnailTabForeground] =
      ui::GetColorWithMaxContrast(kColorThumbnailTabBackground);
  mixer[kColorToolbar] = {dark_mode ? kDarkToolbarColor : kLightToolbarColor};
  mixer[kColorToolbarButtonBackground] =
      ui::GetColorWithMaxContrast(kColorToolbarButtonText);
  mixer[kColorToolbarButtonBorder] = ui::SetAlpha(kColorToolbarInkDrop, 0x20);
  mixer[kColorToolbarButtonIcon] = ui::HSLShift(
      gfx::kChromeIconGrey, GetThemeTint(ThemeProperties::TINT_BUTTONS, key));
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kGoogleGreyAlpha500)};
  mixer[kColorToolbarButtonIconPressed] = {kColorToolbarButtonIconHovered};
  // TODO(crbug.com/967317): Update to match mocks, i.e. return
  // gfx::kGoogleGrey900, if needed.
  mixer[kColorToolbarButtonText] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarContentAreaSeparator] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorToolbar, 0x3A);
  mixer[kColorToolbarFeaturePromoHighlight] = AdjustHighlightColorForContrast(
      kColorToolbarButtonBackground, gfx::kGoogleBlue600, gfx::kGoogleGrey100,
      gfx::kGoogleBlue900, SK_ColorWHITE);
  mixer[kColorToolbarInkDrop] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarSeparator] = ui::SetAlpha(kColorToolbarButtonIcon, 0x4D);
  mixer[kColorToolbarText] = {dark_mode ? SK_ColorWHITE : gfx::kGoogleGrey800};
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameActive);
  mixer[kColorToolbarTopSeparatorFrameInactive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameInactive);
  mixer[kColorWindowControlButtonBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorWindowControlButtonBackgroundInactive] = {
      ui::kColorFrameInactive};

  // Apply high contrast recipes if necessary.
  if (!ShouldApplyHighContrastColors(key))
    return;
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
