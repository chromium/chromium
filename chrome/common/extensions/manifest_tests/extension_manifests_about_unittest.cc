// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

class AboutPageManifestTest : public ChromeManifestTest {};

TEST_F(AboutPageManifestTest, AboutPageInSharedModules) {
  scoped_refptr<extensions::Extension> extension;
  extension = LoadAndExpectSuccess("shared_module_about.json");
  EXPECT_EQ(GURL("chrome-extension://" + extension->id() + "/about.html"),
            extensions::ManifestURL::GetAboutPage(extension.get()));

  Testcase testcases[] = {
      // Forbid data types other than strings.
      Testcase("shared_module_about_invalid_type.json",
               errors::kInvalidAboutPage),

      // Forbid absolute URLs.
      Testcase("shared_module_about_absolute.json",
               errors::kInvalidAboutPageExpectRelativePath)};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}
