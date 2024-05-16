// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Runs the WebUI resources tests.
 */

namespace ash {

namespace {

class AshCommonResourcesBrowserTest : public WebUIMochaBrowserTest {
 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath =
        base::StringPrintf("chromeos/ash_common/%s", testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }
};

using AshCommonResourcesListPropertyUpdateBehaviorTest =
    AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesListPropertyUpdateBehaviorTest, All) {
  RunTestAtPath("list_property_update_behavior_test.js");
}

using AshCommonResourcesI18nBehaviorTest = AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesI18nBehaviorTest, All) {
  RunTestAtPath("i18n_behavior_test.js");
}

using AshCommonResourcesContainerShadowBehaviorTest =
    AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesContainerShadowBehaviorTest, All) {
  RunTestAtPath("cr_container_shadow_behavior_test.js");
}

using AshCommonResourcesPolicyIndicatorBehaviorTest =
    AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesPolicyIndicatorBehaviorTest, All) {
  RunTestAtPath("cr_policy_indicator_behavior_test.js");
}

using AshCommonResourcesScrollableBehaviorTest = AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesScrollableBehaviorTest, All) {
  RunTestAtPath("cr_scrollable_behavior_test.js");
}

using AshCommonResourcesTypescriptUtilsStrictQueryTest =
    AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesTypescriptUtilsStrictQueryTest, All) {
  RunTestAtPath("typescript_utils/strict_query_test.js");
}

using AshCommonResourcesShortcutInputKeyTest = AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesShortcutInputKeyTest, All) {
  RunTestAtPath("shortcut_input_key_test.js");
}

using AshCommonResourcesShortcutInputTest = AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesShortcutInputTest, All) {
  RunTestAtPath("shortcut_input_test.js");
}

using AshCommonResourcesShortcutUtilsTest = AshCommonResourcesBrowserTest;
IN_PROC_BROWSER_TEST_F(AshCommonResourcesShortcutUtilsTest, All) {
  RunTestAtPath("shortcut_utils_test.js");
}

}  // namespace

}  // namespace ash
