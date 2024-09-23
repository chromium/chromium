// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class NewTabPageFocusTest : public WebUIMochaFocusTest {
 protected:
  NewTabPageFocusTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }
};

IN_PROC_BROWSER_TEST_F(NewTabPageFocusTest, DoodleShareDialogFocus) {
  RunTest("new_tab_page/doodle_share_dialog_focus_test.js", "mocha.run()");
}
