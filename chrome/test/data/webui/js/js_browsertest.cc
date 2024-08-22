// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using WebUiJsTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(WebUiJsTest, CrRouter) {
  RunTest("js/cr_router_test.js", "runMochaSuite('CrRouterTest');");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, SendWithPromise) {
  RunTest("js/cr_test.js", "runMochaSuite('CrSendWithPromiseTest');");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, WebUiListeners) {
  RunTest("js/cr_test.js", "runMochaSuite('CrWebUiListenersTest');");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, Icon) {
  RunTest("js/icon_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, PromiseResolver) {
  RunTest("js/promise_resolver_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, ParseHtmlSubset) {
  RunTest("js/parse_html_subset_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, ParseHtmlSubsetTrustedTypes) {
  RunTest("js/parse_html_subset_trusted_types_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, Util) {
  RunTest("js/util_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, LoadTimeData) {
  RunTest("js/load_time_data_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, ColorUtils) {
  RunTest("js/color_utils_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, CustomElement) {
  RunTest("js/custom_element_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, Static) {
  RunTest("js/static_types_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, Store) {
  RunTest("js/store_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, MetricsReporter) {
  // MetricsReporter needs a host that enables BindingsPolicyValue::kMojoWebUi.
  // Any WebUI host should work, except chrome://webui-test since it is just a
  // WebUIDataSource.
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("js/metrics_reporter/metrics_reporter_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, MockTimer) {
  RunTest("mock_timer_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(WebUiJsTest, MojoTypeUtil) {
  RunTest("js/mojo_type_util_test.js", "mocha.run();");
}
