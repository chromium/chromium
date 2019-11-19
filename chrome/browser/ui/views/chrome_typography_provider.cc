// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_typography_provider.h"

#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/platform_font.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/native_theme/native_theme_win.h"
#endif

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/ash_typography.h"  // nogncheck
#endif

const gfx::FontList& ChromeTypographyProvider::GetFont(int context,
                                                       int style) const {
  // "Target" font size constants.
  constexpr int kHeadlineSize = 20;
  constexpr int kTitleSize = 15;
  constexpr int kTouchableLabelSize = 14;
  constexpr int kBodyTextLargeSize = 13;
  constexpr int kDefaultSize = 12;

  std::string typeface;
  int size_delta = kDefaultSize - gfx::PlatformFont::kDefaultBaseFontSize;
  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;

#if defined(OS_CHROMEOS)
  ash::ApplyAshFontStyles(context, style, &size_delta, &font_weight);
#endif

  ApplyCommonFontStyles(context, style, &size_delta, &font_weight);

  switch (context) {
    case views::style::CONTEXT_BUTTON_MD:
      font_weight = MediumWeightForUI();
      break;
    case views::style::CONTEXT_DIALOG_TITLE:
      size_delta = kTitleSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case views::style::CONTEXT_TOUCH_MENU:
      size_delta =
          kTouchableLabelSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_BODY_TEXT_LARGE:
    case CONTEXT_TAB_HOVER_CARD_TITLE:
    case views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT:
      size_delta = kBodyTextLargeSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_HEADLINE:
      size_delta = kHeadlineSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    default:
      break;
  }

  if (context == CONTEXT_TAB_HOVER_CARD_TITLE) {
    DCHECK_EQ(views::style::STYLE_PRIMARY, style);
    font_weight = gfx::Font::Weight::SEMIBOLD;
  }

  // Use a semibold style for emphasized text in body contexts, and ignore
  // |style| otherwise.
  if (style == STYLE_EMPHASIZED || style == STYLE_EMPHASIZED_SECONDARY) {
    switch (context) {
      case CONTEXT_BODY_TEXT_SMALL:
      case CONTEXT_BODY_TEXT_LARGE:
      case views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT:
        font_weight = gfx::Font::Weight::SEMIBOLD;
        break;

      default:
        break;
    }
  }

  if (style == STYLE_PRIMARY_MONOSPACED ||
      style == STYLE_SECONDARY_MONOSPACED) {
#if defined(OS_MACOSX)
    typeface = "Menlo";
#elif defined(OS_WIN)
    typeface = "Consolas";
#else
    typeface = "DejaVu Sans Mono";
#endif
  }

  return ui::ResourceBundle::GetSharedInstance()
      .GetFontListWithTypefaceAndDelta(typeface, size_delta, gfx::Font::NORMAL,
                                       font_weight);
}

SkColor ChromeTypographyProvider::GetColor(const views::View& view,
                                           int context,
                                           int style) const {
  // Monospaced styles have the same colors as their normal counterparts.
  if (style == STYLE_PRIMARY_MONOSPACED) {
    style = views::style::STYLE_PRIMARY;
  } else if (style == STYLE_SECONDARY_MONOSPACED) {
    style = views::style::STYLE_SECONDARY;
  }

  ui::NativeTheme::ColorId color_id;
  switch (style) {
    case STYLE_HINT:
      color_id = ui::NativeTheme::kColorId_LabelDisabledColor;
      break;
    case STYLE_RED:
      color_id = ui::NativeTheme::kColorId_AlertSeverityHigh;
      break;
    case STYLE_GREEN:
      color_id = ui::NativeTheme::kColorId_AlertSeverityLow;
      break;
    default:
      return TypographyProvider::GetColor(view, context, style);
  }
  return view.GetNativeTheme()->GetSystemColor(color_id);
}

int ChromeTypographyProvider::GetLineHeight(int context, int style) const {
  // "Target" line height constants from the Harmony spec. A default OS
  // configuration should use these heights. However, if the user overrides OS
  // defaults, then GetLineHeight() should return the height that would add the
  // same extra space between lines as the default configuration would have.
  constexpr int kHeadlineHeight = 32;
  constexpr int kTitleHeight = 22;
  constexpr int kBodyHeight = 20;  // For both large and small.

  // Button text should always use the minimum line height for a font to avoid
  // unnecessarily influencing the height of a button.
  constexpr int kButtonAbsoluteHeight = 0;

// The platform-specific heights (i.e. gfx::Font::GetHeight()) that result when
// asking for the target size constants in ChromeTypographyProvider::GetFont()
// in a default OS configuration.
#if defined(OS_MACOSX)
  constexpr int kHeadlinePlatformHeight = 25;
  constexpr int kTitlePlatformHeight = 19;
  constexpr int kBodyTextLargePlatformHeight = 16;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#elif defined(OS_WIN)
  constexpr int kHeadlinePlatformHeight = 27;
  constexpr int kTitlePlatformHeight = 20;
  constexpr int kBodyTextLargePlatformHeight = 18;
  constexpr int kBodyTextSmallPlatformHeight = 16;
#else
  constexpr int kHeadlinePlatformHeight = 24;
  constexpr int kTitlePlatformHeight = 18;
  constexpr int kBodyTextLargePlatformHeight = 17;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#endif

  // The style of the system font used to determine line heights.
  constexpr int kTemplateStyle = views::style::STYLE_PRIMARY;

  // TODO(tapted): These statics should be cleared out when something invokes
  // ui::ResourceBundle::ReloadFonts(). Currently that only happens on ChromeOS.
  // See http://crbug.com/708943.
  static const int headline_height =
      GetFont(CONTEXT_HEADLINE, kTemplateStyle).GetHeight() -
      kHeadlinePlatformHeight + kHeadlineHeight;
  static const int title_height =
      GetFont(views::style::CONTEXT_DIALOG_TITLE, kTemplateStyle).GetHeight() -
      kTitlePlatformHeight + kTitleHeight;
  static const int body_large_height =
      GetFont(CONTEXT_BODY_TEXT_LARGE, kTemplateStyle).GetHeight() -
      kBodyTextLargePlatformHeight + kBodyHeight;
  static const int default_height =
      GetFont(CONTEXT_BODY_TEXT_SMALL, kTemplateStyle).GetHeight() -
      kBodyTextSmallPlatformHeight + kBodyHeight;

  switch (context) {
    case views::style::CONTEXT_BUTTON:
    case views::style::CONTEXT_BUTTON_MD:
    case CONTEXT_TOOLBAR_BUTTON:
      return kButtonAbsoluteHeight;
    case views::style::CONTEXT_DIALOG_TITLE:
      return title_height;
    case CONTEXT_BODY_TEXT_LARGE:
    case CONTEXT_TAB_HOVER_CARD_TITLE:
    case views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT:
    case views::style::CONTEXT_TABLE_ROW:
      return body_large_height;
    case CONTEXT_HEADLINE:
      return headline_height;
    default:
      return default_height;
  }
}
