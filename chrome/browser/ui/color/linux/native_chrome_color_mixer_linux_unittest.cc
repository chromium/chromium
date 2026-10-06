// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/color_utils.h"

namespace {

using ColorMode = ui::ColorProviderKey::ColorMode;

// Stand-in for the colors a toolkit (GTK/Qt) mixer provides for menus.
struct ToolkitPalette {
  SkColor menu_background;
  SkColor menu_foreground;
  SkColor menu_foreground_selected;
  SkColor menu_separator;
  SkColor prominent_background;
  SkColor prominent_foreground;
};

// Approximates a dark-only GTK theme such as Adwaita-dark.
constexpr ToolkitPalette kDarkToolkit = {
    .menu_background = SkColorSetRGB(0x2D, 0x2D, 0x2D),
    .menu_foreground = SkColorSetRGB(0xFF, 0xFF, 0xFF),
    .menu_foreground_selected = SkColorSetRGB(0xFF, 0xFF, 0xFF),
    .menu_separator = SkColorSetRGB(0x45, 0x45, 0x45),
    .prominent_background = SkColorSetRGB(0x1C, 0x71, 0xD8),
    .prominent_foreground = SkColorSetRGB(0xFF, 0xFF, 0xFF),
};

// Approximates a light GTK theme such as Adwaita.
constexpr ToolkitPalette kLightToolkit = {
    .menu_background = SkColorSetRGB(0xFA, 0xFA, 0xFA),
    .menu_foreground = SkColorSetRGB(0x1F, 0x1F, 0x1F),
    .menu_foreground_selected = SkColorSetRGB(0x1F, 0x1F, 0x1F),
    .menu_separator = SkColorSetRGB(0xDE, 0xDE, 0xDE),
    .prominent_background = SkColorSetRGB(0x1C, 0x71, 0xD8),
    .prominent_foreground = SkColorSetRGB(0xFF, 0xFF, 0xFF),
};

// App menu color IDs overridden by AddNativeChromeColorMixer() when the
// toolkit palette doesn't match the color mode.
constexpr ui::ColorId kAppMenuSurfaceIds[] = {
    kColorAppMenuYourChromeBackground,
    kColorAppMenuToolsAndActionsBackground,
    kColorAppMenuZoomButtonBackground,
    kColorAppMenuZoomButtonHover,
    kColorAppMenuZoomSeparator,
    ui::kColorAppMenuProfileRowBackground,
    ui::kColorMenuButtonBackground,
    ui::kColorAppMenuUpgradeRowBackground,
    ui::kColorAppMenuUpgradeRowSubstringForeground,
    ui::kColorAppMenuRowBackgroundHovered,
    kColorAppMenuBlockButtonBackground,
    kColorAppMenuBlockButtonBackgroundHovered,
    kColorAppMenuBlockButtonBorder,
    kColorAppMenuBlockButtonForeground,
    kColorAppMenuFooterButtonForeground,
    kColorAppMenuFooterButtonForegroundHovered,
    kColorAppMenuFooterButtonBackgroundHovered,
    kColorAppMenuChipBackground,
    kColorAppMenuChipForeground,
    ui::kColorAppMenuProfileRowChipBackground,
};

class NativeChromeColorMixerLinuxTest : public testing::Test {
 protected:
  // Builds a provider equivalent to the production pipeline: the core UI
  // mixers, then (optionally) a toolkit mixer, then the Chrome mixers, which
  // end with AddNativeChromeColorMixer().
  std::unique_ptr<ui::ColorProvider> CreateProvider(
      ColorMode color_mode,
      ui::SystemTheme system_theme,
      const ToolkitPalette& palette) {
    ui::ColorProviderKey key;
    key.color_mode = color_mode;
    key.system_theme = system_theme;

    auto provider = std::make_unique<ui::ColorProvider>();
    ui::AddColorMixers(provider.get(), key);

    // Mirrors the GTK/Qt mixers, which are appended after the core mixers and
    // before the Chrome mixers (see
    // ChromeBrowserMainParts::ToolkitInitialized).
    ui::ColorMixer& toolkit_mixer = provider->AddMixer();
    toolkit_mixer[ui::kColorMenuBackground] = {palette.menu_background};
    toolkit_mixer[ui::kColorMenuItemForeground] = {palette.menu_foreground};
    toolkit_mixer[ui::kColorMenuItemForegroundSelected] = {
        palette.menu_foreground_selected};
    toolkit_mixer[ui::kColorMenuSeparator] = {palette.menu_separator};
    toolkit_mixer[ui::kColorButtonBackgroundProminent] = {
        palette.prominent_background};
    toolkit_mixer[ui::kColorButtonForegroundProminent] = {
        palette.prominent_foreground};

    AddChromeColorMixers(provider.get(), key);
    return provider;
  }

  // Expects `foreground` to be readable on `background`. Both may be
  // translucent; they're composited onto the menu background first.
  void ExpectReadable(const ui::ColorProvider& provider,
                      ui::ColorId foreground,
                      SkColor background,
                      const std::string& description) {
    const SkColor menu_background = provider.GetColor(ui::kColorMenuBackground);
    const SkColor opaque_background =
        color_utils::GetResultingPaintColor(background, menu_background);
    const SkColor opaque_foreground = color_utils::GetResultingPaintColor(
        provider.GetColor(foreground), opaque_background);
    EXPECT_GE(
        color_utils::GetContrastRatio(opaque_foreground, opaque_background),
        color_utils::kMinimumReadableContrastRatio)
        << description << ": " << ui::ColorIdName(foreground) << " "
        << ui::SkColorName(opaque_foreground) << " on "
        << ui::SkColorName(opaque_background);
  }

  // Expects item text, and hovered item text, to be readable on `card_id`.
  void ExpectCardReadable(const ui::ColorProvider& provider,
                          ui::ColorId card_id) {
    const SkColor card = provider.GetColor(card_id);
    ExpectReadable(provider, ui::kColorMenuItemForeground, card,
                   "Item text on " + ui::ColorIdName(card_id));

    // Hovered items inside cards are painted with the row hover color and
    // drawn with the toolkit's selected text color.
    const SkColor hovered_card = color_utils::GetResultingPaintColor(
        provider.GetColor(ui::kColorAppMenuRowBackgroundHovered), card);
    ExpectReadable(provider, ui::kColorMenuItemForegroundSelected, hovered_card,
                   "Hovered item text on " + ui::ColorIdName(card_id));
  }

  // Checks every foreground/background pair the app menu renders.
  void ExpectAppMenuReadable(const ui::ColorProvider& provider) {
    ExpectCardReadable(provider, kColorAppMenuYourChromeBackground);
    ExpectCardReadable(provider, kColorAppMenuToolsAndActionsBackground);
    ExpectCardReadable(provider, kColorAppMenuZoomButtonBackground);
    ExpectCardReadable(provider, ui::kColorAppMenuProfileRowBackground);
    ExpectCardReadable(provider, ui::kColorAppMenuUpgradeRowBackground);

    ExpectReadable(provider, kColorAppMenuBlockButtonForeground,
                   provider.GetColor(kColorAppMenuBlockButtonBackground),
                   "Block button");
    ExpectReadable(provider, kColorAppMenuFooterButtonForeground,
                   provider.GetColor(ui::kColorMenuBackground),
                   "Footer button");
    ExpectReadable(provider, kColorAppMenuChipForeground,
                   provider.GetColor(kColorAppMenuChipBackground), "Chip");
  }
};

// The scenario from the bug: a dark-only toolkit theme while the color mode is
// light (e.g. portal color-scheme "no preference", crrev.com/c/8429688).
TEST_F(NativeChromeColorMixerLinuxTest, MismatchDarkToolkitLightModeReadable) {
  auto provider =
      CreateProvider(ColorMode::kLight, ui::SystemTheme::kGtk, kDarkToolkit);

  ExpectAppMenuReadable(*provider);
  EXPECT_EQ(provider->GetColor(kColorAppMenuBlockButtonForeground),
            kDarkToolkit.menu_foreground);
  EXPECT_EQ(provider->GetColor(kColorAppMenuChipBackground),
            kDarkToolkit.prominent_background);
  // Row hover must be as visible as the block and footer button hover.
  EXPECT_EQ(provider->GetColor(ui::kColorAppMenuRowBackgroundHovered),
            provider->GetColor(kColorAppMenuBlockButtonBackgroundHovered));
}

TEST_F(NativeChromeColorMixerLinuxTest, MismatchLightToolkitDarkModeReadable) {
  auto provider =
      CreateProvider(ColorMode::kDark, ui::SystemTheme::kGtk, kLightToolkit);

  ExpectAppMenuReadable(*provider);
  EXPECT_EQ(provider->GetColor(kColorAppMenuBlockButtonForeground),
            kLightToolkit.menu_foreground);
  EXPECT_EQ(provider->GetColor(kColorAppMenuChipBackground),
            kLightToolkit.prominent_background);
  EXPECT_EQ(provider->GetColor(ui::kColorAppMenuRowBackgroundHovered),
            provider->GetColor(kColorAppMenuBlockButtonBackgroundHovered));
}

// Qt goes through the same mixer, gated on `system_theme != kDefault`.
TEST_F(NativeChromeColorMixerLinuxTest, MismatchQtReadable) {
  auto provider =
      CreateProvider(ColorMode::kLight, ui::SystemTheme::kQt, kDarkToolkit);

  ExpectAppMenuReadable(*provider);
}

// When the toolkit palette matches the color mode, the original (Material)
// app menu design must be preserved. Runs once per ID in kAppMenuSurfaceIds.
class NativeChromeColorMixerLinuxMatchingPaletteTest
    : public NativeChromeColorMixerLinuxTest,
      public testing::WithParamInterface<ui::ColorId> {
 protected:
  // Expects the color ID under test to be the same with the GTK system theme
  // as with the default theme, for a `palette` that matches `color_mode`.
  void ExpectMaterialColorKept(ColorMode color_mode,
                               const ToolkitPalette& palette) {
    const ui::ColorId id = GetParam();
    auto native_provider =
        CreateProvider(color_mode, ui::SystemTheme::kGtk, palette);
    auto default_provider =
        CreateProvider(color_mode, ui::SystemTheme::kDefault, palette);
    EXPECT_EQ(native_provider->GetColor(id), default_provider->GetColor(id))
        << ui::ColorIdName(id);
  }
};

TEST_P(NativeChromeColorMixerLinuxMatchingPaletteTest,
       DarkPaletteKeepsMaterialColor) {
  ExpectMaterialColorKept(ColorMode::kDark, kDarkToolkit);
}

TEST_P(NativeChromeColorMixerLinuxMatchingPaletteTest,
       LightPaletteKeepsMaterialColor) {
  ExpectMaterialColorKept(ColorMode::kLight, kLightToolkit);
}

// Test names use the default index suffix: Chrome color IDs only get readable
// names once AddChromeColorMixers() runs, after tests are registered.
INSTANTIATE_TEST_SUITE_P(All,
                         NativeChromeColorMixerLinuxMatchingPaletteTest,
                         testing::ValuesIn(kAppMenuSurfaceIds));

// The default (non-system) theme never uses the native overrides, even if the
// toolkit palette disagrees with the color mode.
TEST_F(NativeChromeColorMixerLinuxTest, DefaultSystemThemeUnchanged) {
  auto provider = CreateProvider(ColorMode::kLight, ui::SystemTheme::kDefault,
                                 kDarkToolkit);

  EXPECT_EQ(provider->GetColor(kColorAppMenuYourChromeBackground),
            provider->GetColor(ui::kColorSysBaseContainer));
  EXPECT_EQ(provider->GetColor(kColorAppMenuToolsAndActionsBackground),
            provider->GetColor(ui::kColorSysNeutralContainer));
  EXPECT_EQ(provider->GetColor(kColorAppMenuBlockButtonBackground),
            provider->GetColor(ui::kColorSysSurface));
  EXPECT_EQ(provider->GetColor(kColorAppMenuBlockButtonForeground),
            provider->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(provider->GetColor(kColorAppMenuFooterButtonForeground),
            provider->GetColor(ui::kColorSysPrimary));
  EXPECT_EQ(provider->GetColor(kColorAppMenuChipBackground),
            provider->GetColor(ui::kColorSysTonalContainer));
  EXPECT_EQ(provider->GetColor(ui::kColorAppMenuRowBackgroundHovered),
            provider->GetColor(ui::kColorSysStateHoverOnSubtle));
}

// Stand-in for the theme supplier the GTK/Qt system themes put in the key.
class TestNativeThemeSupplier
    : public ui::ColorProviderKey::ThemeInitializerSupplier {
 public:
  TestNativeThemeSupplier() : ThemeInitializerSupplier(ThemeType::kNativeX11) {}

  // ui::ColorProviderKey::ThemeInitializerSupplier:
  void AddColorMixers(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const override {}
  bool GetColor(int id, SkColor* color) const override { return false; }
  bool GetTint(int id, color_utils::HSL* hsl) const override { return false; }
  bool GetDisplayProperty(int id, int* result) const override { return false; }
  bool HasCustomImage(int id) const override { return false; }

 private:
  ~TestNativeThemeSupplier() override = default;
};

// Builds a provider for a toolkit that supplies the toolbar color but not the
// textfield colors (like Qt), so the omnibox follows the color mode.
std::unique_ptr<ui::ColorProvider> CreateToolbarOnlyProvider(
    ColorMode color_mode,
    SkColor toolbar) {
  ui::ColorProviderKey key;
  key.color_mode = color_mode;
  key.system_theme = ui::SystemTheme::kQt;
  key.custom_theme = base::MakeRefCounted<TestNativeThemeSupplier>();

  auto provider = std::make_unique<ui::ColorProvider>();
  ui::AddColorMixers(provider.get(), key);
  ui::ColorMixer& toolkit_mixer = provider->AddMixer();
  toolkit_mixer[ui::kColorNativeToolbarBackground] = {toolbar};
  AddChromeColorMixers(provider.get(), key);
  return provider;
}

void ExpectLocationIconChipReadable(const ui::ColorProvider& provider) {
  const SkColor chip = provider.GetColor(kColorOmniboxIconBackground);
  for (ui::ColorId id : {kColorOmniboxText, kColorOmniboxIconForeground}) {
    const SkColor foreground = provider.GetColor(id);
    EXPECT_GE(color_utils::GetContrastRatio(foreground, chip),
              color_utils::kMinimumReadableContrastRatio)
        << ui::ColorIdName(id) << " " << ui::SkColorName(foreground) << " on "
        << ui::SkColorName(chip);
  }
}

// The location icon chip used the toolbar color as its background and the
// omnibox text color for its label. With a dark Qt toolbar and a light color
// mode, both were dark.
TEST(NativeChromeColorMixerLinuxChipTest, DarkToolbarLightModeReadable) {
  auto provider = CreateToolbarOnlyProvider(ColorMode::kLight,
                                            kDarkToolkit.menu_background);
  ExpectLocationIconChipReadable(*provider);
}

TEST(NativeChromeColorMixerLinuxChipTest, LightToolbarDarkModeReadable) {
  auto provider = CreateToolbarOnlyProvider(ColorMode::kDark,
                                            kLightToolkit.menu_background);
  ExpectLocationIconChipReadable(*provider);
}

// When the omnibox text is readable on the toolbar, the toolbar color is kept.
TEST(NativeChromeColorMixerLinuxChipTest, MatchingToolbarKept) {
  auto provider = CreateToolbarOnlyProvider(ColorMode::kLight,
                                            kLightToolkit.menu_background);
  ExpectLocationIconChipReadable(*provider);
  EXPECT_EQ(provider->GetColor(kColorOmniboxIconBackground),
            provider->GetColor(kColorToolbar));
}

}  // namespace
