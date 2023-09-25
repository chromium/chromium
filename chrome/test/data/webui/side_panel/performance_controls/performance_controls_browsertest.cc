// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"

class SidePanelPerformanceControlsTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelPerformanceControlsTest() {
    set_test_loader_host(chrome::kChromeUIPerformanceSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      performance_manager::features::kPerformanceControlsSidePanel};
};

IN_PROC_BROWSER_TEST_F(SidePanelPerformanceControlsTest, App) {
  RunTest("side_panel/performance_controls/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPerformanceControlsTest, PerformanceApiProxy) {
  RunTest("side_panel/performance_controls/performance_api_proxy_test.js",
          "mocha.run()");
}
