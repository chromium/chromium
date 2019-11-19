// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/stl_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"

namespace extensions {

class DisplayModeMatchesManifestTest : public ChromeManifestTest {};

TEST_F(DisplayModeMatchesManifestTest, DisplayMode) {
  Testcase testcases[] = {
      Testcase("display_mode.json", std::string(),
               extensions::Manifest::INTERNAL, Extension::FROM_BOOKMARK),
  };
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_SUCCESS);

  Testcase failure_testcases[] = {
      Testcase("display_mode_wrong_type.json",
               extensions::manifest_errors::kInvalidAppDisplayMode),
  };
  RunTestcases(failure_testcases, base::size(failure_testcases),
               EXPECT_TYPE_ERROR);

  Testcase warning_testcases[] = {
      Testcase("display_mode.json",
               extensions::manifest_errors::kInvalidDisplayModeAppType),
  };
  RunTestcases(warning_testcases, base::size(warning_testcases),
               EXPECT_TYPE_WARNING);
}

}  // namespace extensions
