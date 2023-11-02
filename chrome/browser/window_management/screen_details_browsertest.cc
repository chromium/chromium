// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/browser/screen_enumeration/screen_details_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using ScreenDetailsTest = InProcessBrowserTest;

// Tests the basic structure and values of the ScreenDetails API.
// TODO(crbug.com/1119974): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(ScreenDetailsTest, GetScreenDetailsBasic) {
  auto* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // Auto-accept the permission request.
  permissions::PermissionRequestManager* permission_request_manager_tab =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager_tab->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  ASSERT_TRUE(EvalJs(tab, "'getScreenDetails' in self").ExtractBool());
  content::EvalJsResult result =
      EvalJs(tab, content::test::kGetScreenDetailsScript);
  EXPECT_EQ(content::test::GetExpectedScreenDetails(), result.value);
}
