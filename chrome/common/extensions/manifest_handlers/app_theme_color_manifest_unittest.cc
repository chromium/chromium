// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ThemeColorMatchesManifestTest : public ChromeManifestTest {};

TEST_F(ThemeColorMatchesManifestTest, ThemeColor) {
  Testcase testcases[] = {
      Testcase("theme_color.json", std::string(),
               extensions::mojom::ManifestLocation::kInternal,
               Extension::FROM_BOOKMARK),
  };
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_SUCCESS);

  Testcase failure_testcases[] = {
      Testcase("theme_color_wrong_type.json",
               extensions::manifest_errors::kInvalidAppThemeColor),
  };
  RunTestcases(failure_testcases, base::size(failure_testcases),
               EXPECT_TYPE_ERROR);

  Testcase warning_testcases[] = {
      Testcase("theme_color.json",
               extensions::manifest_errors::kInvalidThemeColorAppType),
  };
  RunTestcases(warning_testcases, base::size(warning_testcases),
               EXPECT_TYPE_WARNING);
}

}  // namespace extensions
