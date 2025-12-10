// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class NewTabPageFocusTest : public WebUIMochaFocusTest {
 protected:
  NewTabPageFocusTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_composebox::kNtpComposebox,
                              ntp_realbox::kNtpRealboxNext},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageFocusTest, DoodleShareDialogFocus) {
  RunTest("new_tab_page/doodle_share_dialog_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageFocusTest, AppFocus) {
  RunTest("new_tab_page/app_focus_test.js", "mocha.run()");
}
