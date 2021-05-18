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
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

constexpr char kGetScreens[] = R"(
  (async () => {
    try { const screens = await self.getScreens(); } catch {}
    return (await navigator.permissions.query({name:'window-placement'})).state;
  })();
)";

// Tests of WindowPlacementPermissionContext behavior.
class WindowPlacementPermissionContextTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Window placement features are only available on secure contexts, and so
    // we need to create an HTTPS test server here to serve those pages rather
    // than using the default embedded_test_server().
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    // Support sites like a.test, b.test, c.test etc
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_test_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    net::test_server::RegisterDefaultHandlers(https_test_server_.get());
    content::SetupCrossSiteRedirector(https_test_server_.get());
    ASSERT_TRUE(https_test_server_->Start());
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

// Tests user activation after dimissing and denying the permission request.
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, DismissAndDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Auto-dismiss the permission request; user activation should not be granted.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::DISMISS);
  EXPECT_EQ("prompt",
            EvalJs(tab, kGetScreens, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  // Auto-deny the permission request; user activation should not be granted.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::DENY_ALL);
  EXPECT_EQ("denied",
            EvalJs(tab, kGetScreens, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
}

// Tests user activation after accepting the permission request.
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, Accept) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Auto-accept the permission request; user activation should be granted.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_EQ("granted",
            EvalJs(tab, kGetScreens, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest,
                       IFrameSameOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_EQ("granted", EvalJs(child, kGetScreens,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_TRUE(child->GetMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest,
                       IFrameCrossOriginDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // PermissionRequestManager will accept any window placement permission
  // dialogs that appear. However, the window-placement permission is not
  // explicitly allowed on the iframe, so requests made by the child frame will
  // be automatically denied before a prompt might be issued
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_EQ("denied", EvalJs(child, kGetScreens,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest,
                       IFrameCrossOriginExplicitAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // See https://w3c.github.io/webappsec-permissions-policy/ for more
  // information on permissions policies and allowing cross-origin iframes
  // to have particular permissions.
  //
  // TODO(enne): This code causes a user activation, so can't check that below
  // like other tests.  Figure out why this is and try to clear it / address it.
  EXPECT_TRUE(ExecJs(tab, R"(const frame = document.getElementById('test');
    frame.setAttribute('allow', 'window-placement');)"));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_EQ("granted", EvalJs(child, kGetScreens,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

}  // namespace
