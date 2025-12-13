// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/theme_handler.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

class ThemeHandlerTest : public testing::Test {
 protected:
  // Creates a dummy extension for the given theme dictionary.
  // TODO(crbug.com/41317803): Continue removing std::string error and
  // replacing with std::u16string. Once this is done, consider changing the
  // return type to base::expected<scoped_refptr<Extension>, std::u16string>.
  scoped_refptr<Extension> CreateExtension(base::Value::Dict&& theme_dict,
                                           std::string& error) {
    base::Value::Dict manifest;
    manifest.Set(keys::kManifestVersion, 3);
    manifest.Set(keys::kName, "My Theme");
    manifest.Set(keys::kVersion, "1.0");
    manifest.Set(keys::kTheme, std::move(theme_dict));

    std::u16string utf16_error;
    scoped_refptr<Extension> extension =
        Extension::Create(base::FilePath(), mojom::ManifestLocation::kInternal,
                          manifest, Extension::NO_FLAGS, &utf16_error);
    error = base::UTF16ToUTF8(utf16_error);
    return extension;
  }
};

TEST_F(ThemeHandlerTest, EmptyThemeDictionary) {
  // Empty |theme| dictionary should be considered valid and thus create an
  // |extension|.
  base::Value::Dict theme = base::Value::Dict();
  std::string error;
  scoped_refptr<Extension> extension = CreateExtension(std::move(theme), error);
  EXPECT_TRUE(extension);
}

TEST_F(ThemeHandlerTest, ValidInputWithCustomizeTabGroupColorPaletteEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCustomizeTabGroupColorPalette);

  // Integer values for keys inside `tab_group_color_palette` should be
  // considered valid and thus create an |extension|.
  base::Value::Dict theme = base::Value::Dict().Set(
      "tab_group_color_palette", base::Value::Dict().Set("red_override", 50));
  std::string error;
  scoped_refptr<Extension> extension = CreateExtension(std::move(theme), error);
  EXPECT_TRUE(extension);

  const base::Value::Dict* tab_group_color_palette_dict =
      ThemeInfo::GetTabGroupColorPalette(extension.get());
  EXPECT_TRUE(tab_group_color_palette_dict);

  EXPECT_EQ(tab_group_color_palette_dict->FindInt("red_override"), 50);
}

TEST_F(ThemeHandlerTest, InvalidInputWithCustomizeTabGroupColorPaletteEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kCustomizeTabGroupColorPalette);

  // Non-integer values inside `tab_group_color_palette` should be considered
  // invalid and thus |extension| will be nullptr.
  base::Value::Dict theme = base::Value::Dict().Set(
      "tab_group_color_palette",
      base::Value::Dict().Set("red_override", "invalid value"));
  std::string error;
  scoped_refptr<Extension> extension = CreateExtension(std::move(theme), error);
  EXPECT_FALSE(extension);
  EXPECT_EQ(error,
            base::UTF16ToUTF8(errors::kInvalidThemeTabGroupColorPalette));
}

TEST_F(ThemeHandlerTest,
       InvalidInputWithCustomizeTabGroupColorPaletteDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCustomizeTabGroupColorPalette);

  // Due to the feature flag being disabled, the `tab_group_color_palette` key
  // will be ignored. So, even though the values inside the
  // `tab_group_color_palette` key are invalid, the overall theme will still be
  // considered valid.
  base::Value::Dict theme = base::Value::Dict().Set(
      "tab_group_color_palette",
      base::Value::Dict().Set("red_override", "invalid value"));
  std::string error;
  scoped_refptr<Extension> extension = CreateExtension(std::move(theme), error);
  EXPECT_TRUE(extension);
}

}  // namespace extensions
