// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

using URLOverridesManifestTest = ChromeManifestTest;

TEST_F(URLOverridesManifestTest, Override) {
  RunTestcase(
      Testcase("override_newtab_and_history.json", errors::kMultipleOverrides),
      EXPECT_TYPE_ERROR);

  scoped_refptr<extensions::Extension> extension;

  extension = LoadAndExpectSuccess("override_new_tab.json");
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension.get())
                .find("newtab")->second.spec());

  extension = LoadAndExpectSuccess("override_history.json");
  EXPECT_EQ(extension->url().spec() + "history.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension.get())
                .find("history")->second.spec());

  // An extension which specifies an invalid override should still load for
  // future compatibility.
  extension = LoadAndExpectSuccess("override_invalid_page.json");
  EXPECT_TRUE(
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()).empty());

  // "keyboard" property is only available on ChromeOS Ash.
  extension = LoadAndExpectSuccess("override_keyboard_page.json");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(extension->url().spec() + "a_page.html",
            extensions::URLOverrides::GetChromeURLOverrides(extension.get())
                .find("keyboard")
                ->second.spec());
#else
  EXPECT_TRUE(
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()).empty());
#endif
}
