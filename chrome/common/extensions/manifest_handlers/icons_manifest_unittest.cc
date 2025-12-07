// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace errors = manifest_errors;

using IconsManifestTest = ChromeManifestTest;

TEST_F(IconsManifestTest, FailLoadingNonImageIcon) {
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      "mime_type/bad_icon.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidIconMimeType,
                                     base::NumberToString(32)));
  EXPECT_TRUE(IconsInfo::GetIcons(extension.get()).empty());
}

}  // namespace extensions
