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
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace errors = manifest_errors;

using BackgroundScriptsManifestTest = ChromeManifestTest;

TEST_F(BackgroundScriptsManifestTest, FailLoadingNonJsScripts) {
  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      "mime_type/bad_background_script.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidBackgroundScriptMimeType,
                                     base::NumberToString(0)));
  EXPECT_FALSE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));
}

}  // namespace extensions
