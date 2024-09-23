// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://sanitize.
 */

namespace ash {

namespace {

class SanitizeUIBrowserTest : public WebUIMochaBrowserTest {
 public:
  SanitizeUIBrowserTest() {
    set_test_loader_host(::ash::kChromeUISanitizeAppHost);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kSanitize},
        /*disabled_featuers=*/{});
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath =
        base::StringPrintf("chromeos/sanitize_ui/%s", testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SanitizeUIBrowserTest, SanitizeInitialize) {
  RunTestAtPath("sanitize_ui_test.js");
}

}  // namespace

}  // namespace ash
