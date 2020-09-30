// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Tests of WindowPlacementPermissionContext behavior.
class WindowPlacementPermissionContextTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }
};

// Tests user activation after dimissing and denying the permission request.
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, DismissAndDeny) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Auto-dismiss the permission request; user activation should not be granted.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::DISMISS);
  EXPECT_FALSE(
      EvalJs(tab, "self.getScreens()", content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .error.empty());
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  // Auto-deny the permission request; user activation should not be granted.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::DENY_ALL);
  EXPECT_FALSE(
      EvalJs(tab, "self.getScreens()", content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .error.empty());
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
}

// Tests user activation after accepting the permission request.
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, Accept) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Auto-accept the permission request; user activation should be granted.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(
      EvalJs(tab, "self.getScreens()", content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .error.empty());
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());
}
