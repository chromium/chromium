// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest CrModuleTest;

IN_PROC_BROWSER_TEST_F(CrModuleTest, SendWithPromise) {
  RunTest("js/cr_test.js", "runMochaSuite('CrModuleSendWithPromiseTest');");
}

IN_PROC_BROWSER_TEST_F(CrModuleTest, WebUiListeners) {
  RunTest("js/cr_test.js", "runMochaSuite('CrModuleWebUiListenersTest');");
}

typedef WebUIMochaBrowserTest IconModuleTest;
IN_PROC_BROWSER_TEST_F(IconModuleTest, All) {
  RunTest("js/icon_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest PromiseResolverModuleTest;
IN_PROC_BROWSER_TEST_F(PromiseResolverModuleTest, All) {
  RunTest("js/promise_resolver_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest ParseHtmlSubsetModuleTest;
IN_PROC_BROWSER_TEST_F(ParseHtmlSubsetModuleTest, All) {
  RunTest("js/parse_html_subset_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest ParseHtmlSubsetTrustedTypesTest;
IN_PROC_BROWSER_TEST_F(ParseHtmlSubsetTrustedTypesTest, All) {
  RunTest("js/parse_html_subset_trusted_types_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest UtilModuleTest;
IN_PROC_BROWSER_TEST_F(UtilModuleTest, All) {
  RunTest("js/util_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest LoadTimeDataModuleTest;
IN_PROC_BROWSER_TEST_F(LoadTimeDataModuleTest, All) {
  RunTest("js/load_time_data_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest ColorUtilsModuleTest;
IN_PROC_BROWSER_TEST_F(ColorUtilsModuleTest, All) {
  RunTest("js/color_utils_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest CustomElementModuleTest;
IN_PROC_BROWSER_TEST_F(CustomElementModuleTest, All) {
  RunTest("js/custom_element_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest StaticTypesTest;
IN_PROC_BROWSER_TEST_F(StaticTypesTest, All) {
  RunTest("js/static_types_test.js", "mocha.run();");
}

typedef WebUIMochaBrowserTest MockTimerTest;
IN_PROC_BROWSER_TEST_F(MockTimerTest, All) {
  RunTest("mock_timer_test.js", "mocha.run();");
}
