// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using SeaPenBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(SeaPenBrowserTest, SeaPenUtils) {
  RunTest("chromeos/ash_common/sea_pen/sea_pen_utils_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SeaPenBrowserTest, SeaPenConstants) {
  RunTest("chromeos/ash_common/sea_pen/sea_pen_constants_test.js",
          "mocha.run()");
}
