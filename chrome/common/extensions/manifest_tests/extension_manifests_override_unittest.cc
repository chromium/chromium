// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cxx17_backports.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

class URLOverridesManifestTest : public ChromeManifestTest {
};

TEST_F(URLOverridesManifestTest, Override) {
  Testcase testcases[] = {
    Testcase("override_newtab_and_history.json", errors::kMultipleOverrides),
    Testcase("override_invalid_page.json", errors::kInvalidChromeURLOverrides)
  };
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);

  scoped_refptr<extensions::Extension> extension;

  extension = LoadAndExpectSuccess("override_new_tab.json");
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension.get())
                .find("newtab")->second.spec());

  extension = LoadAndExpectSuccess("override_history.json");
  EXPECT_EQ(extension->url().spec() + "history.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension.get())
                .find("history")->second.spec());
}
