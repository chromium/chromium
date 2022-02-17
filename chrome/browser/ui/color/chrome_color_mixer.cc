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
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

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
  mixer[kColorDownloadShelf] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelf};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelf,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadToolbarButtonActive] = {ui::kColorThrobber};
  mixer[kColorDownloadToolbarButtonInactive] = {ui::kColorMidground};
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      SkColorSetA(kColorDownloadToolbarButtonInactive, 0x33)};
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  mixer[kColorGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                          : gfx::kGoogleGrey700};
#endif
  mixer[kColorLocationBarBorder] = {SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorNewTabPageBackground] = {kColorToolbar};
  mixer[kColorNewTabPageHeader] = {SkColorSetRGB(0x96, 0x96, 0x96)};
  mixer[kColorNewTabPageText] = {dark_mode ? gfx::kGoogleGrey200
                                           : SK_ColorBLACK};
  mixer[kColorOmniboxBackground] = {dark_mode ? gfx::kGoogleGrey900
                                              : gfx::kGoogleGrey100};
  mixer[kColorOmniboxText] =
      ui::GetColorWithMaxContrast(kColorOmniboxBackground);
  mixer[kColorTabGroupContextMenuBlue] = {dark_mode ? gfx::kGoogleBlue300
                                                    : gfx::kGoogleBlue600};
  mixer[kColorTabGroupContextMenuCyan] = {dark_mode ? gfx::kGoogleCyan300
                                                    : gfx::kGoogleCyan900};
  mixer[kColorTabGroupContextMenuGreen] = {dark_mode ? gfx::kGoogleGreen300
                                                     : gfx::kGoogleGreen700};
  mixer[kColorTabGroupContextMenuGrey] = {dark_mode ? gfx::kGoogleGrey300
                                                    : gfx::kGoogleGrey700};
  mixer[kColorTabGroupContextMenuOrange] = {dark_mode ? gfx::kGoogleOrange300
                                                      : gfx::kGoogleOrange400};
  mixer[kColorTabGroupContextMenuPink] = {dark_mode ? gfx::kGooglePink300
                                                    : gfx::kGooglePink700};
  mixer[kColorTabGroupContextMenuPurple] = {dark_mode ? gfx::kGooglePurple300
                                                      : gfx::kGooglePurple500};
  mixer[kColorTabGroupContextMenuRed] = {dark_mode ? gfx::kGoogleRed300
                                                   : gfx::kGoogleRed600};
  mixer[kColorTabGroupContextMenuYellow] = {dark_mode ? gfx::kGoogleYellow300
                                                      : gfx::kGoogleYellow600};
  mixer[kColorToolbar] = {dark_mode ? SkColorSetRGB(0x35, 0x36, 0x3A)
                                    : SK_ColorWHITE};
  mixer[kColorToolbarButtonIcon] = ui::HSLShift(
      {gfx::kChromeIconGrey}, GetThemeTint(ThemeProperties::TINT_BUTTONS, key));
  mixer[kColorToolbarContentAreaSeparator] = {ui::kColorSeparator};
  mixer[kColorToolbarSeparator] = ui::SetAlpha({kColorToolbarButtonIcon}, 0x4D);
  mixer[kColorToolbarText] = {dark_mode ? SK_ColorWHITE : gfx::kGoogleGrey800};
}
