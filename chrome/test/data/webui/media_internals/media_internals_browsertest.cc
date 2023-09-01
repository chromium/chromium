// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"

class MediaInternalsUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  MediaInternalsUIBrowserTest() {
    set_test_loader_host(content::kChromeUIMediaInternalsHost);
  }
};

IN_PROC_BROWSER_TEST_F(MediaInternalsUIBrowserTest, Integration) {
  RunTestWithoutTestLoader("media_internals/integration_test.js",
                           "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(MediaInternalsUIBrowserTest, Manager) {
  RunTestWithoutTestLoader("media_internals/manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(MediaInternalsUIBrowserTest, PlayerInfo) {
  RunTestWithoutTestLoader("media_internals/player_info_test.js",
                           "mocha.run()");
}
