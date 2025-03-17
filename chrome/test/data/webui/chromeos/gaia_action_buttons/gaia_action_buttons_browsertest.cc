// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class GaiaActionButtonsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  GaiaActionButtonsBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }
};

IN_PROC_BROWSER_TEST_F(GaiaActionButtonsBrowserTest, AllTests) {
  RunTest("gaia_action_buttons/gaia_action_buttons_test.js", "mocha.run()");
}
