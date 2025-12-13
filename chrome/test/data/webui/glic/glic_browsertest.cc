// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

class GlicWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  GlicWebUIBrowserTest() { set_test_loader_host(chrome::kChromeUIGlicHost); }

  void SetUp() override { WebUIMochaBrowserTest::SetUp(); }

 private:
  glic::GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicWebUIBrowserTest, UnitTestWebview) {
  RunTest("glic/unit_tests/webview_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(GlicWebUIBrowserTest, UnitTestObservable) {
  RunTest("glic/unit_tests/observable_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(GlicWebUIBrowserTest, UnitTestGlicApiHost) {
  RunTest("glic/unit_tests/glic_api_host_test.js", "mocha.run()");
}
