// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/font_util.h"
#include "ui/strings/grit/app_locale_settings.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/gfx/system_fonts_win.h"
#endif

namespace {

// The default system font name.
#if defined(OS_WIN)
const char kDefaultFontName[] = "Segoe UI";
#endif

// Constant from the Harmony spec.
constexpr int kHarmonyTitleSize = 15;
}  // namespace

class LayoutProviderTest : public testing::Test {
 public:
  LayoutProviderTest() {}

 protected:
  static void SetUpTestSuite() {
#if defined(OS_WIN)
    base::win::EnableHighDPISupport();
#endif
    gfx::InitializeFonts();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutProviderTest);
};

// Check whether the system is in the default configuration. This test will fail
// if some system-wide settings are changed. Other tests rely on these default
// settings and were the cause of many flaky tests.
TEST_F(LayoutProviderTest, EnsuresDefaultSystemSettings) {
#if defined(OS_WIN)
  // Ensures anti-aliasing is activated.
  BOOL antialiasing = TRUE;
  BOOL result = SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &antialiasing, 0);
  EXPECT_NE(result, FALSE);
  EXPECT_NE(antialiasing, FALSE)
      << "The test requires that fonts smoothing (anti-aliasing) is "
         "activated. If this assert is failing you need to manually activate "
         "the flag in your system fonts settings.";

  // Ensures that the screen resolution is at the default value.
  float system_dpi_scale = display::win::GetDPIScale();
  EXPECT_EQ(system_dpi_scale, 1.0)
      << "The test requires default display settings. The DPI of the display "
         "is not 100%. dpi_scale="
      << system_dpi_scale;

  double accessibility_font_scale = display::win::GetAccessibilityFontScale();
  EXPECT_EQ(accessibility_font_scale, 1.0)
      << "The test requires default display settings. The fonts are scaled "
         "due to accessibility settings. font_scale="
      << accessibility_font_scale;

  // Ensures that the default UI fonts have the original settings.
  gfx::Font caption_font =
      gfx::win::GetSystemFont(gfx::win::SystemFont::kCaption);
  gfx::Font small_caption_font =
      gfx::win::GetSystemFont(gfx::win::SystemFont::kSmallCaption);
  gfx::Font menu_font = gfx::win::GetSystemFont(gfx::win::SystemFont::kMenu);
  gfx::Font status_font =
      gfx::win::GetSystemFont(gfx::win::SystemFont::kStatus);
  gfx::Font message_font =
      gfx::win::GetSystemFont(gfx::win::SystemFont::kMessage);

  EXPECT_EQ(caption_font.GetFontName(), kDefaultFontName);
  EXPECT_EQ(small_caption_font.GetFontName(), kDefaultFontName);
  EXPECT_EQ(menu_font.GetFontName(), kDefaultFontName);
  EXPECT_EQ(status_font.GetFontName(), kDefaultFontName);
  EXPECT_EQ(message_font.GetFontName(), kDefaultFontName);

  EXPECT_EQ(caption_font.GetFontSize(), 12);
  EXPECT_EQ(small_caption_font.GetFontSize(), 12);
  EXPECT_EQ(menu_font.GetFontSize(), 12);
  EXPECT_EQ(status_font.GetFontSize(), 12);
  EXPECT_EQ(message_font.GetFontSize(), 12);
#endif
}

// Check legacy font sizes. No new code should be using these constants, but if
// these tests ever fail it probably means something in the old UI will have
// changed by mistake.
// https://crbug.com/961938
#if defined(OS_MACOSX)
#define MAYBE_LegacyFontSizeConstants DISABLED_LegacyFontSizeConstants
#else
#define MAYBE_LegacyFontSizeConstants LegacyFontSizeConstants
#endif
TEST_F(LayoutProviderTest, MAYBE_LegacyFontSizeConstants) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::FontList label_font = rb.GetFontListWithDelta(ui::kLabelFontSizeDelta);

#if defined(OS_WIN)
  EXPECT_EQ(16, label_font.GetHeight());
  EXPECT_EQ(13, label_font.GetBaseline());
#else
  EXPECT_EQ(15, label_font.GetHeight());
  EXPECT_EQ(12, label_font.GetBaseline());
#endif
  EXPECT_EQ(12, label_font.GetFontSize());
  EXPECT_EQ(9, label_font.GetCapHeight());

#if defined(OS_MACOSX)
  EXPECT_EQ(7, label_font.GetExpectedTextWidth(1));
#else
  EXPECT_EQ(6, label_font.GetExpectedTextWidth(1));
#endif

  gfx::FontList title_font = rb.GetFontListWithDelta(ui::kTitleFontSizeDelta);

#if defined(OS_WIN)
  EXPECT_EQ(15, title_font.GetFontSize());
  EXPECT_EQ(20, title_font.GetHeight());
  EXPECT_EQ(17, title_font.GetBaseline());
  EXPECT_EQ(11, title_font.GetCapHeight());
#elif defined(OS_MACOSX)
  EXPECT_EQ(14, title_font.GetFontSize());
  EXPECT_EQ(17, title_font.GetHeight());
  EXPECT_EQ(14, title_font.GetBaseline());
  EXPECT_EQ(10, title_font.GetCapHeight());
#else
  EXPECT_EQ(15, title_font.GetFontSize());
  EXPECT_EQ(18, title_font.GetHeight());
  EXPECT_EQ(14, title_font.GetBaseline());
  EXPECT_EQ(11, title_font.GetCapHeight());
#endif

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(gfx::kPlatformFontSkiaOnWindows)) {
    EXPECT_EQ(7, title_font.GetExpectedTextWidth(1));
  } else {
    EXPECT_EQ(8, title_font.GetExpectedTextWidth(1));
  }
#else
  EXPECT_EQ(8, title_font.GetExpectedTextWidth(1));
#endif

  gfx::FontList small_font = rb.GetFontList(ui::ResourceBundle::SmallFont);
  gfx::FontList base_font = rb.GetFontList(ui::ResourceBundle::BaseFont);
  gfx::FontList bold_font = rb.GetFontList(ui::ResourceBundle::BoldFont);
  gfx::FontList medium_font = rb.GetFontList(ui::ResourceBundle::MediumFont);
  gfx::FontList medium_bold_font =
      rb.GetFontList(ui::ResourceBundle::MediumBoldFont);
  gfx::FontList large_font = rb.GetFontList(ui::ResourceBundle::LargeFont);

#if defined(OS_MACOSX)
  EXPECT_EQ(12, small_font.GetFontSize());
  EXPECT_EQ(13, base_font.GetFontSize());
  EXPECT_EQ(13, bold_font.GetFontSize());
  EXPECT_EQ(16, medium_font.GetFontSize());
  EXPECT_EQ(16, medium_bold_font.GetFontSize());
  EXPECT_EQ(21, large_font.GetFontSize());
#else
  EXPECT_EQ(11, small_font.GetFontSize());
  EXPECT_EQ(12, base_font.GetFontSize());
  EXPECT_EQ(12, bold_font.GetFontSize());
  EXPECT_EQ(15, medium_font.GetFontSize());
  EXPECT_EQ(15, medium_bold_font.GetFontSize());
  EXPECT_EQ(20, large_font.GetFontSize());
#endif
}

// Check that asking for fonts of a given size match the Harmony spec. If these
// tests fail, the Harmony TypographyProvider needs to be updated to handle the
// new font properties. For example, when title_font.GetHeight() returns 19, the
// Harmony TypographyProvider adds 3 to obtain its target height of 22. If a
// platform starts returning 18 in a standard configuration then the
// TypographyProvider must add 4 instead. We do this so that Chrome adapts
// correctly to _non-standard_ system font configurations on user machines.
TEST_F(LayoutProviderTest, RequestFontBySize) {
#if defined(OS_MACOSX)
  constexpr int kBase = 13;
#else
  constexpr int kBase = 12;
#endif
  // Harmony spec.
  constexpr int kHeadline = 20;
  constexpr int kTitle = kHarmonyTitleSize;  // Leading 22.
  constexpr int kBody1 = 13;                 // Leading 20.
  constexpr int kBody2 = 12;                 // Leading 20.
  constexpr int kButton = 12;

#if defined(OS_WIN)
  constexpr gfx::Font::Weight kButtonWeight = gfx::Font::Weight::BOLD;
#else
  constexpr gfx::Font::Weight kButtonWeight = gfx::Font::Weight::MEDIUM;
#endif

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  gfx::FontList headline_font = rb.GetFontListWithDelta(kHeadline - kBase);
  gfx::FontList title_font = rb.GetFontListWithDelta(kTitle - kBase);
  gfx::FontList body1_font = rb.GetFontListWithDelta(kBody1 - kBase);
  gfx::FontList body2_font = rb.GetFontListWithDelta(kBody2 - kBase);
  gfx::FontList button_font = rb.GetFontListWithDelta(
      kButton - kBase, gfx::Font::NORMAL, kButtonWeight);

  // The following checks on leading don't need to match the spec. Instead, it
  // means Label::SetLineHeight() needs to be used to increase it. But what we
  // are really interested in is the delta between GetFontSize() and GetHeight()
  // since that (plus a fixed constant) determines how the leading should change
  // when a larger font is configured in the OS.

  EXPECT_EQ(kHeadline, headline_font.GetFontSize());

// Headline leading not specified (multiline should be rare).
#if defined(OS_MACOSX)
  EXPECT_EQ(25, headline_font.GetHeight());
#elif defined(OS_WIN)
  EXPECT_EQ(27, headline_font.GetHeight());
#else
  EXPECT_EQ(24, headline_font.GetHeight());
#endif

  EXPECT_EQ(kTitle, title_font.GetFontSize());

// Title font leading should be 22.
#if defined(OS_MACOSX)
  EXPECT_EQ(19, title_font.GetHeight());  // i.e. Add 3 to obtain line height.
#elif defined(OS_WIN)
  EXPECT_EQ(20, title_font.GetHeight());  // Add 2.
#else
  EXPECT_EQ(18, title_font.GetHeight());  // Add 4.
#endif

  EXPECT_EQ(kBody1, body1_font.GetFontSize());

// Body1 font leading should be 20.
#if defined(OS_MACOSX)
  EXPECT_EQ(16, body1_font.GetHeight());  // Add 4.
#elif defined(OS_WIN)
  EXPECT_EQ(18, body1_font.GetHeight());
#else  // Linux.
  EXPECT_EQ(17, body1_font.GetHeight());  // Add 3.
#endif

  EXPECT_EQ(kBody2, body2_font.GetFontSize());

// Body2 font leading should be 20.
#if defined(OS_WIN)
  EXPECT_EQ(16, body2_font.GetHeight());
#else
  EXPECT_EQ(15, body2_font.GetHeight());  // Other platforms: Add 5.
#endif

  EXPECT_EQ(kButton, button_font.GetFontSize());

// Button leading not specified (shouldn't be needed: no multiline buttons).
#if defined(OS_WIN)
  EXPECT_EQ(16, button_font.GetHeight());
#else
  EXPECT_EQ(15, button_font.GetHeight());
#endif
}

// Test that the default TypographyProvider correctly maps TextContexts relative
// to the "base" font in the manner that legacy toolkit-views code expects. This
// reads the base font configuration at runtime, and only tests font sizes, so
// should be robust against platform changes.
TEST_F(LayoutProviderTest, FontSizeRelativeToBase) {
  using views::style::GetFont;

  constexpr int kStyle = views::style::STYLE_PRIMARY;

  std::unique_ptr<views::LayoutProvider> layout_provider =
      ChromeLayoutProvider::CreateLayoutProvider();

// Everything's measured relative to a default-constructed FontList.
// On Mac, subtract one since that is 13pt instead of 12pt.
#if defined(OS_MACOSX)
  const int twelve = gfx::FontList().GetFontSize() - 1;
#else
  const int twelve = gfx::FontList().GetFontSize();
#endif

  EXPECT_EQ(twelve, GetFont(CONTEXT_BODY_TEXT_SMALL, kStyle).GetFontSize());
  EXPECT_EQ(twelve, GetFont(views::style::CONTEXT_LABEL, kStyle).GetFontSize());
  EXPECT_EQ(twelve,
            GetFont(views::style::CONTEXT_TEXTFIELD, kStyle).GetFontSize());
  EXPECT_EQ(twelve,
            GetFont(views::style::CONTEXT_BUTTON, kStyle).GetFontSize());

  // E.g. Headline should give a 20pt font.
  EXPECT_EQ(twelve + 8, GetFont(CONTEXT_HEADLINE, kStyle).GetFontSize());
  // Titles should be 15pt. Etc.
  EXPECT_EQ(twelve + 3,
            GetFont(views::style::CONTEXT_DIALOG_TITLE, kStyle).GetFontSize());
  EXPECT_EQ(twelve + 1, GetFont(CONTEXT_BODY_TEXT_LARGE, kStyle).GetFontSize());
}

// Ensure that line height can be overridden by Chrome's TypographyProvider for
// for the standard set of styles. This varies by platform and test machine
// configuration. Generally, for a particular platform configuration, there
// should be a consistent increase in line height when compared to the height of
// a given font.
TEST_F(LayoutProviderTest, TypographyLineHeight) {
  constexpr int kStyle = views::style::STYLE_PRIMARY;

  std::unique_ptr<views::LayoutProvider> layout_provider =
      ChromeLayoutProvider::CreateLayoutProvider();

  constexpr struct {
    int context;
    int min;
    int max;
  } kExpectedIncreases[] = {{CONTEXT_HEADLINE, 4, 8},
                            {views::style::CONTEXT_DIALOG_TITLE, 1, 4},
                            {CONTEXT_BODY_TEXT_LARGE, 2, 4},
                            {CONTEXT_BODY_TEXT_SMALL, 4, 5}};

  for (size_t i = 0; i < base::size(kExpectedIncreases); ++i) {
    SCOPED_TRACE(testing::Message() << "Testing index: " << i);
    const auto& increase = kExpectedIncreases[i];
    const gfx::FontList& font = views::style::GetFont(increase.context, kStyle);
    int line_spacing = views::style::GetLineHeight(increase.context, kStyle);
    EXPECT_GE(increase.max, line_spacing - font.GetHeight());
    EXPECT_LE(increase.min, line_spacing - font.GetHeight());
  }

  // Buttons should specify zero line height (i.e. use the font's height) so
  // buttons have flexibility to configure their own spacing.
  EXPECT_EQ(0,
            views::style::GetLineHeight(views::style::CONTEXT_BUTTON, kStyle));
  EXPECT_EQ(
      0, views::style::GetLineHeight(views::style::CONTEXT_BUTTON_MD, kStyle));
}

// Ensure that line heights reported in a default bot configuration match the
// Harmony spec. This test will only run if it detects that the current machine
// has the default OS configuration.
TEST_F(LayoutProviderTest, ExplicitTypographyLineHeight) {
  std::unique_ptr<views::LayoutProvider> layout_provider =
      ChromeLayoutProvider::CreateLayoutProvider();

  constexpr int kStyle = views::style::STYLE_PRIMARY;
  if (views::style::GetFont(views::style::CONTEXT_DIALOG_TITLE, kStyle)
          .GetFontSize() != kHarmonyTitleSize) {
    LOG(WARNING) << "Skipping: Test machine not in default configuration.";
    return;
  }

  // Line heights from the Harmony spec.
  constexpr int kBodyLineHeight = 20;
  constexpr struct {
    int context;
    int line_height;
  } kHarmonyHeights[] = {{CONTEXT_HEADLINE, 32},
                         {views::style::CONTEXT_DIALOG_TITLE, 22},
                         {CONTEXT_BODY_TEXT_LARGE, kBodyLineHeight},
                         {CONTEXT_BODY_TEXT_SMALL, kBodyLineHeight}};

  for (size_t i = 0; i < base::size(kHarmonyHeights); ++i) {
    SCOPED_TRACE(testing::Message() << "Testing index: " << i);
    EXPECT_EQ(kHarmonyHeights[i].line_height,
              views::style::GetLineHeight(kHarmonyHeights[i].context, kStyle));

    views::Label label(base::ASCIIToUTF16("test"), kHarmonyHeights[i].context);
    label.SizeToPreferredSize();
    EXPECT_EQ(kHarmonyHeights[i].line_height, label.height());
  }

  // TODO(tapted): Pass in contexts to StyledLabel instead. Currently they are
  // stuck on style::CONTEXT_LABEL. That only matches the default line height in
  // ChromeTypographyProvider::GetLineHeight(), which is body text.
  EXPECT_EQ(kBodyLineHeight,
            views::style::GetLineHeight(views::style::CONTEXT_LABEL, kStyle));
  views::StyledLabel styled_label(base::ASCIIToUTF16("test"), nullptr);
  constexpr int kStyledLabelWidth = 200;  // Enough to avoid wrapping.
  styled_label.SizeToFit(kStyledLabelWidth);
  EXPECT_EQ(kBodyLineHeight, styled_label.height());

  // Adding a link should not change the size.
  styled_label.AddStyleRange(
      gfx::Range(0, 2), views::StyledLabel::RangeStyleInfo::CreateForLink());
  styled_label.SizeToFit(kStyledLabelWidth);
  EXPECT_EQ(kBodyLineHeight, styled_label.height());
}

// Only run explicit font-size checks on ChromeOS. Elsewhere, font sizes can be
// affected by bot configuration, but ChromeOS controls this in the
// ResourceBundle. Also on other platforms font metrics change a lot across OS
// versions, but on ChromeOS, there is only one OS version, so we can rely on
// consistent behavior. Also ChromeOS is the only place where
// IDS_UI_FONT_FAMILY_CROS works, which this test uses to control results.
#if defined(OS_CHROMEOS)

// Ensure the omnibox font is always 14pt, even in Hebrew. On ChromeOS, Hebrew
// has a larger default font size applied from the resource bundle, but the
// Omnibox font configuration ignores it.
TEST_F(LayoutProviderTest, OmniboxFontAlways14) {
  constexpr int kOmniboxHeight = 24;
  constexpr int kDecorationHeight = 14;
  constexpr int kOmniboxDesiredSize = 14;
  constexpr int kDecorationRequestedSize = 11;

  auto& bundle = ui::ResourceBundle::GetSharedInstance();

  auto set_system_font = [&bundle](const char* font) {
    bundle.OverrideLocaleStringResource(IDS_UI_FONT_FAMILY_CROS,
                                        base::ASCIIToUTF16(font));
    bundle.ReloadFonts();
    return gfx::FontList().GetFontSize();
  };

  int base_font_size = set_system_font("Roboto, 12px");
  EXPECT_EQ(12, base_font_size);
  EXPECT_EQ(base_font_size, bundle.GetFontListWithDelta(0).GetFontSize());
  EXPECT_EQ(14 - base_font_size, GetFontSizeDeltaBoundedByAvailableHeight(
                                     kOmniboxHeight, kOmniboxDesiredSize));
  EXPECT_EQ(11 - base_font_size,
            GetFontSizeDeltaBoundedByAvailableHeight(kDecorationHeight,
                                                     kDecorationRequestedSize));

  // Ensure there is a threshold where the font actually shrinks.
  int latin_height_threshold = kOmniboxHeight;
  for (; latin_height_threshold > 0; --latin_height_threshold) {
    if (kOmniboxDesiredSize - base_font_size !=
        GetFontSizeDeltaBoundedByAvailableHeight(latin_height_threshold,
                                                 kOmniboxDesiredSize))
      break;
  }
  // The threshold should always be the same, but the value depends on font
  // metrics. Check for some sane value. This should only change if Roboto
  // itself changes.
  EXPECT_EQ(16, latin_height_threshold);

  // Switch to Hebrew settings.
  base_font_size = set_system_font("Roboto, Noto Sans Hebrew, 13px");
  EXPECT_EQ(13, gfx::FontList().GetFontSize());
  EXPECT_EQ(base_font_size, bundle.GetFontListWithDelta(0).GetFontSize());

  // The base font size has increased, but the delta returned should still
  // result in a 14pt font.
  EXPECT_EQ(14 - base_font_size, GetFontSizeDeltaBoundedByAvailableHeight(
                                     kOmniboxHeight, kOmniboxDesiredSize));
  EXPECT_EQ(11 - base_font_size,
            GetFontSizeDeltaBoundedByAvailableHeight(kDecorationHeight,
                                                     kDecorationRequestedSize));
}

#endif  // OS_CHROMEOS
