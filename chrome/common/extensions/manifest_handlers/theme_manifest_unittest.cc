// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_EQ(ThemeInfo::GetImages(extension.get())->size(), 1u);
}

}  // namespace extensions
