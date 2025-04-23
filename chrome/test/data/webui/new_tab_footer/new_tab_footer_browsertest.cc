// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

class NewTabFooterBrowserTest : public WebUIMochaBrowserTest {
 protected:
  NewTabFooterBrowserTest() {
    set_test_loader_host(chrome::kChromeUINewTabFooterHost);
  }

  base::test::ScopedFeatureList scoped_feature_list_{ntp_features::kNtpFooter};
};

IN_PROC_BROWSER_TEST_F(NewTabFooterBrowserTest, App) {
  RunTest("new_tab_footer/app_test.js", "mocha.run()");
}
