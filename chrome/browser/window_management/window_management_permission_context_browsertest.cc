// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/screen_base.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr char kGetScreensScript[] = R"(
  (async () => {
    try {
      const screenDetails = await self.getScreenDetails();
    } catch {
      return 'error';
    }
    try {
      return (await navigator.permissions.query({name:'window-management'}))
              .state;
    } catch {
      return "permission_error";
    }
  })();
)";

constexpr char kCheckPermissionScript[] = R"(
  (async () => {
    try {
      return (await navigator.permissions.query({name:'window-management'}))
              .state;
     } catch {
      return 'permission_error';
    }  })();
)";

// Tests of WindowManagementPermissionContext behavior.
class WindowManagementPermissionContextTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Window management features are only available on secure contexts, and so
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

  // Awaits expiry of the navigator.userActivation signal on the active tab.
  void WaitForUserActivationExpiry() {
    const std::string await_activation_expiry_script = R"(
      (async () => {
        while (navigator.userActivation.isActive)
          await new Promise(resolve => setTimeout(resolve, 1000));
        return navigator.userActivation.isActive;
      })();
    )";
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(false, EvalJs(tab, await_activation_expiry_script,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    EXPECT_FALSE(tab->HasRecentInteraction());
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

class MultiscreenWindowManagementPermissionContextTest
    : public WindowManagementPermissionContextTest {
 public:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ~MultiscreenWindowManagementPermissionContextTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }
#endif

  void SetScreenInstance() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Use the default, see SetUpOnMainThread.
    WindowManagementPermissionContextTest::SetScreenInstance();
#else
    display::Screen::SetScreenInstance(&screen_);
    screen_.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::PRIMARY);
    screen_.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 803)},
                                      display::DisplayList::Type::NOT_PRIMARY);
    ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // This has to happen later than SetScreenInstance as the ash shell
    // does not exist yet.
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay("100+100-801x802,901+100-802x803");
    ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
#endif
    WindowManagementPermissionContextTest::SetUpOnMainThread();
  }

 private:
  display::ScreenBase screen_;
};

// Tests gesture requirements (a gesture is only needed to prompt the user).
IN_PROC_BROWSER_TEST_F(WindowManagementPermissionContextTest, GestureToPrompt) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Auto-dismiss the permission request, iff the prompt is shown.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Calling getScreenDetails() without a gesture or pre-existing permission
  // will not prompt the user, and leaves the permission in the default "prompt"
  // state.
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_EQ("error", EvalJs(tab, kGetScreensScript,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermissionScript,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Calling getScreenDetails() with a gesture will show the prompt, and
  // auto-accept.
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_EQ("granted", EvalJs(tab, kGetScreensScript));
  EXPECT_TRUE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());

  // Calling getScreenDetails() without a gesture, but with pre-existing
  // permission, will succeed, since it does not need to prompt the user.
  WaitForUserActivationExpiry();
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_EQ("granted", EvalJs(tab, kGetScreensScript,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
}

// TODO(crbug.com/40212482): Test failing on linux-chromeos-chrome.
// TODO(crbug.com/40212443): Test failing on linux.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_DismissAndDeny DISABLED_DismissAndDeny
#else
#define MAYBE_DismissAndDeny DismissAndDeny
#endif
// Tests user activation after dimissing and denying the permission request.
IN_PROC_BROWSER_TEST_F(WindowManagementPermissionContextTest,
                       MAYBE_DismissAndDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Dismiss the prompt after activation expires, expect no activation.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Dismiss();
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermissionScript,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());

  // Deny the prompt after activation expires, expect no activation.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Deny();
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermissionScript,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
}

// Tests user activation after accepting the permission request.
IN_PROC_BROWSER_TEST_F(WindowManagementPermissionContextTest, Accept) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Accept the prompt after activation expires, expect an activation signal.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  EXPECT_EQ("granted", EvalJs(tab, kCheckPermissionScript,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowManagementPermissionContextTest,
                       IFrameSameOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* child = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Accept the prompt after activation expires, expect an activation signal.
  ExecuteScriptAsync(child, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  EXPECT_EQ("granted", EvalJs(child, kCheckPermissionScript,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_TRUE(child->GetMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowManagementPermissionContextTest,
                       IFrameCrossOriginDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // PermissionRequestManager will accept any window management permission
  // dialogs that appear. However, the window-management permission is not
  // explicitly allowed on the iframe, so requests made by the child frame will
  // be automatically denied before a prompt might be issued.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_EQ("error", EvalJs(child, kGetScreensScript));
  EXPECT_EQ("denied", EvalJs(child, kCheckPermissionScript,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(WindowManagementPermissionContextTest,
                       IFrameCrossOriginExplicitAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // See https://w3c.github.io/webappsec-permissions-policy/ for more
  // information on permissions policies and allowing cross-origin iframes
  // to have particular permissions.
  EXPECT_TRUE(ExecJs(tab,
                     R"(const frame = document.getElementById('test');
                          frame.setAttribute('allow', 'window-management');)",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Accept the prompt after activation expires, expect an activation signal.
  ExecuteScriptAsync(child, "getScreenDetails()");
  WaitForUserActivationExpiry();

  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  EXPECT_EQ("granted", EvalJs(child, kCheckPermissionScript,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetPrimaryMainFrame()->HasTransientUserActivation());
  EXPECT_TRUE(child->GetMainFrame()->HasTransientUserActivation());
}

// TODO(enne): Windows assumes that display::GetScreen() is a ScreenWin
// which is not true here.
#if !BUILDFLAG(IS_WIN)

// Verify that window.screen.isExtended returns true in a same-origin
// iframe without the window management permission policy allowed.
IN_PROC_BROWSER_TEST_F(MultiscreenWindowManagementPermissionContextTest,
                       IsExtendedSameOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* child = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);

  EXPECT_EQ(true, EvalJs(tab, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(true, EvalJs(child, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Verify that window.screen.isExtended returns false in a cross-origin
// iframe without the window management permission policy allowed.
IN_PROC_BROWSER_TEST_F(MultiscreenWindowManagementPermissionContextTest,
                       IsExtendedCrossOriginDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);

  EXPECT_EQ(true, EvalJs(tab, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, EvalJs(child, R"(window.screen.isExtended)",
                          content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Verify that window.screen.isExtended returns true in a cross-origin
// iframe with the window management permission policy allowed.
IN_PROC_BROWSER_TEST_F(MultiscreenWindowManagementPermissionContextTest,
                       IsExtendedCrossOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // See https://w3c.github.io/webappsec-permissions-policy/ for more
  // information on permissions policies and allowing cross-origin iframes
  // to have particular permissions.
  EXPECT_TRUE(ExecJs(tab, R"(const frame = document.getElementById('test');
                          frame.setAttribute('allow', 'window-management');)",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child);

  EXPECT_EQ(true, EvalJs(tab, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(true, EvalJs(child, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

#endif  // !BUILDFLAG(IS_WIN)

}  // namespace
