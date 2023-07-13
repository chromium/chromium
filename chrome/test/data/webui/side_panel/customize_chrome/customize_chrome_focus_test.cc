// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

class SidePanelCustomizeChromeFocusTest : public WebUIMochaFocusTest {
 protected:
  SidePanelCustomizeChromeFocusTest() {
    set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ntp_features::kCustomizeChromeSidePanel};
};

IN_PROC_BROWSER_TEST_F(SidePanelCustomizeChromeFocusTest, ColorsFocus) {
  RunTest("side_panel/customize_chrome/colors_focus_test.js", "mocha.run()");
}
