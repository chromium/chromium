// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace errors = manifest_errors;

using ThemeManifestTest = ChromeManifestTest;

TEST_F(ThemeManifestTest, FailLoadingNonPngThemeImage) {
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      "mime_type/bad_theme_image.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidThemeImageMimeType,
                                     "files/bad_icon.txt"));
  EXPECT_TRUE(ThemeInfo::GetImages(extension.get())->empty());
}

TEST_F(ThemeManifestTest, LoadImagesWithoutExtensionWithWarning) {
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      "mime_type/bad_theme_image_no_extension.json",
      ErrorUtils::FormatErrorMessage(errors::kThemeImageMissingFileExtension,
                                     "files/file_name_without_extension"));
  EXPECT_EQ(1u, ThemeInfo::GetImages(extension.get())->size());
}

TEST_F(ThemeManifestTest, MissingThemeImageVariantValidationWarning) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("theme_missing_image_variant.json");
  EXPECT_EQ(1u, ThemeInfo::GetImages(extension.get())->size());

  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(
      1, std::ranges::count_if(warnings, [](const InstallWarning& warning) {
        return warning.message == ErrorUtils::FormatErrorMessage(
                                      errors::kInvalidThemeDictImagePath,
                                      "theme_toolbar", "100",
                                      "images/100_percent/theme_toolbar.png");
      }));
}

}  // namespace extensions
