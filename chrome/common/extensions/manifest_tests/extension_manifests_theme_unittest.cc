// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class ThemeExtensionsTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  // TODO(crbug.com/41317803): Continue removing std::string error and
  // replacing with std::u16string.
  scoped_refptr<Extension> CreateExtension(const base::Value::Dict& manifest,
                                           std::string* error) {
    base::Value::Dict manifest_base;
    manifest_base.Set("name", "test");
    manifest_base.Set("version", "1.0");
    manifest_base.Set("manifest_version", 3);
    manifest_base.Merge(manifest.Clone());
    std::u16string utf16_error;
    scoped_refptr<Extension> extension = Extension::Create(
        temp_dir_.GetPath(), mojom::ManifestLocation::kUnpacked, manifest_base,
        Extension::NO_FLAGS, "", &utf16_error);
    *error = base::UTF16ToUTF8(utf16_error);
    return extension;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ThemeExtensionsTest, InvalidThemeImagesValueType) {
  base::Value::Dict manifest = base::test::ParseJsonDict(R"({
        "theme": {
          "images": {
            "invalid_image_type": false
          }
        }
      })");

  std::string error;
  auto extension = CreateExtension(manifest, &error);
  ASSERT_FALSE(extension);
  ASSERT_EQ(base::UTF16ToUTF8(manifest_errors::kInvalidThemeImagesValueType),
            error);
}

TEST_F(ThemeExtensionsTest, InvalidThemeImagesPath) {
  static constexpr struct {
    const char* relative_path;
  } test_cases[] = {
      {""},
      {"?"},
      {"dir/"},
      {"https://example.com"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.relative_path);

    base::Value::Dict manifest = base::test::ParseJsonDict(base::StringPrintf(
        R"({
          "theme": {
            "images": {
              "invalid_image_path": "%s"
            }
          }
        })",
        test_case.relative_path));

    std::string error;
    auto extension = CreateExtension(manifest, &error);
    ASSERT_FALSE(extension);
    ASSERT_EQ(base::UTF16ToUTF8(manifest_errors::kInvalidThemeImagesPath),
              error);
  }
}

TEST_F(ThemeExtensionsTest, ThemeImagesPathDoesntExist) {
  base::Value::Dict manifest = base::test::ParseJsonDict(
      R"({
        "theme": {
          "images": {
            "non_existent_image_path": "does_not_exist.png"
          }
        }
      })");

  std::string error;
  auto extension = CreateExtension(manifest, &error);
  ASSERT_TRUE(extension);

  std::vector<InstallWarning> warnings;
  ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
  ASSERT_EQ(l10n_util::GetStringFUTF8(IDS_EXTENSION_INVALID_IMAGE_PATH,
                                      u"does_not_exist.png"),
            error);
}

TEST_F(ThemeExtensionsTest, ThemeImagesPathVariantDoesntExist) {
  base::Value::Dict manifest = base::test::ParseJsonDict(
      R"({
        "theme": {
          "images": {
            "non_existent_image_path_with_scale": {
              "100": "does_not_exist.png"
            }
          }
        }
      })");

  std::string error;
  auto extension = CreateExtension(manifest, &error);
  ASSERT_TRUE(extension);

  std::vector<InstallWarning> warnings;
  ASSERT_TRUE(
      ManifestHandler::ValidateExtension(extension.get(), &error, &warnings));
  ASSERT_EQ(1u, warnings.size());
  ASSERT_EQ(
      ErrorUtils::FormatErrorMessage(
          manifest_errors::kInvalidThemeDictImagePath,
          "non_existent_image_path_with_scale", "100", "does_not_exist.png"),
      warnings[0].message);
}

}  // namespace extensions
