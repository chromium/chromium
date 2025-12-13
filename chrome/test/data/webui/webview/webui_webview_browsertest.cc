// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#endif

// Turn these tests off on Mac while we collect data on windows server crashes
// on mac chromium builders.
// http://crbug.com/653353
#if !BUILDFLAG(IS_MAC)

class WebUIWebViewBrowserTest : public WebUIMochaBrowserTest {
 public:
  WebUIWebViewBrowserTest() = default;

  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS)
    // Wait for the OOBE WebUI to be shown.
    ash::OobeScreenWaiter(ash::WelcomeView::kScreenId).Wait();
#else
    set_test_loader_host(GetWebViewEnabledWebUIURL().GetHost());
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL()));
#endif
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

#if BUILDFLAG(IS_CHROMEOS)
  Profile* GetProfileForSetup() override {
    return Profile::FromBrowserContext(
        GetWebContentsForTesting()->GetBrowserContext());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
    // Force showing OOBE WebUI on the ChromeOS ASH configuration.
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }
#endif

  GURL GetTestUrl(const std::string& path) const {
    return embedded_test_server()->base_url().Resolve(path);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  GURL GetWebViewEnabledWebUIURL() const {
    return GURL(signin::GetEmbeddedPromoURL(
        signin_metrics::AccessPoint::kStartPage,
        signin_metrics::Reason::kForcedSigninPrimaryAccount, false));
  }
#endif

  content::WebContents* GetWebContentsForTesting() {
#if BUILDFLAG(IS_CHROMEOS)
    return ash::LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->web_ui()
        ->GetWebContents();
#else
    return browser()->tab_strip_model()->GetActiveWebContents();
#endif
  }

  testing::AssertionResult RunContentScriptTestCase(
      const std::string& test_case,
      const std::string& url) {
    return RunTestOnWebContents(
        GetWebContentsForTesting(), "webview/webview_content_script_test.js",
        base::StringPrintf("window.webviewUrl = '%s'; "
                           "runMochaTest('WebviewContentScriptTest', '%s');",
                           url.c_str(), test_case.c_str()),
        true);
  }

  void RunBasicTestCase(const std::string& test_case, const std::string& url) {
    RunTest("webview/webview_basic_test.js",
            base::StringPrintf("window.webviewUrl = '%s'; "
                               "runMochaTest('WebviewBasicTest', '%s');",
                               url.c_str(), test_case.c_str()),
            false);
  }

#if BUILDFLAG(ENABLE_GLIC)
  // Required to enable chrome://glic.
  glic::GlicTestEnvironment glic_test_env_;
#endif
};

// Checks that hiding and showing the WebUI host page doesn't break guests in
// it.
// Regression test for http://crbug.com/515268
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, DisplayNone) {
  ASSERT_TRUE(RunTestOnWebContents(
      GetWebContentsForTesting(), "webview/webview_basic_test.js",
      "runMochaTest('WebviewBasicTest', 'DisplayNone')", true));
}

#if BUILDFLAG(ENABLE_GLIC)
// TODO(crbug.com/460836171): Enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MediaRequestAllowOnGlic DISABLED_MediaRequestAllowOnGlic
#else
#define MAYBE_MediaRequestAllowOnGlic MediaRequestAllowOnGlic
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, MAYBE_MediaRequestAllowOnGlic) {
  set_test_loader_host("glic");
  RunBasicTestCase("MediaRequestAllowOnGlic",
                   GetTestUrl("webview/mediarequest.html").spec());
}

// TODO(crbug.com/460836171): Enable on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MediaRequestDenyOnGlic DISABLED_MediaRequestDenyOnGlic
#else
#define MAYBE_MediaRequestDenyOnGlic MediaRequestDenyOnGlic
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, MAYBE_MediaRequestDenyOnGlic) {
  set_test_loader_host("glic");
  RunBasicTestCase("MediaRequestDenyOnGlic",
                   GetTestUrl("webview/mediarequest.html").spec());
}

// TODO(crbug.com/444024595): Flaky on Linux and Windows
// TODO(crbug.com/460836171): Enable on ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MediaRequestAllowOnSignIn DISABLED_MediaRequestAllowOnSignIn
#else
#define MAYBE_MediaRequestAllowOnSignIn MediaRequestAllowOnSignIn
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       MAYBE_MediaRequestAllowOnSignIn) {
  RunBasicTestCase("MediaRequestAllowOnSignIn",
                   GetTestUrl("webview/mediarequest.html").spec());
}
#endif  // BUILDFLAG(ENABLE_GLIC)

// TODO(crbug.com/41400417) Flaky on CrOS trybots.
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_ExecuteScriptCode DISABLED_ExecuteScriptCode
#else
#define MAYBE_ExecuteScriptCode ExecuteScriptCode
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, MAYBE_ExecuteScriptCode) {
  ASSERT_TRUE(RunContentScriptTestCase("ExecuteScriptCode",
                                       GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ExecuteScriptCodeFromFile) {
  ASSERT_TRUE(RunContentScriptTestCase("ExecuteScriptCodeFromFile",
                                       GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScript) {
  ASSERT_TRUE(RunContentScriptTestCase("AddContentScript",
                                       GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddMultiContentScripts) {
  ASSERT_TRUE(RunContentScriptTestCase("AddMultiContentScripts",
                                       GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    AddContentScriptWithSameNameShouldOverwriteTheExistingOne) {
  ASSERT_TRUE(RunContentScriptTestCase(
      "AddContentScriptWithSameNameShouldOverwriteTheExistingOne",
      GetTestUrl("empty.html").spec()));
}

#if (BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)) || \
    BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/40583245) Fails on CrOS dbg with --enable-features=Mash.
// TODO(crbug.com/41496635): Webviews don't work properly with JS coverage.
#define MAYBE_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView \
  DISABLED_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView
#else
#define MAYBE_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView \
  AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView
#endif
IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    MAYBE_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView) {
  ASSERT_TRUE(RunContentScriptTestCase(
      "AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView",
      GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddAndRemoveContentScripts) {
  ASSERT_TRUE(RunContentScriptTestCase("AddAndRemoveContentScripts",
                                       GetTestUrl("empty.html").spec()));
}

// Disable code coverage for the NewWindowAPI test. Currently code coverage
// seems to break for tests that open a new window to run extra scripts,
// which this test does.
// See https://crbug.com/1489565
class WebUIWebViewCoverageDisabledBrowserTest : public WebUIWebViewBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIWebViewBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kDevtoolsCodeCoverage);
  }
};

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_CHROMEOS) && \
                          (!defined(NDEBUG) || defined(ADDRESS_SANITIZER)))
// TODO(crbug.com/40583245) Fails on CrOS dbg with --enable-features=Mash.
// TODO(crbug.com/41419648) Flaky on CrOS ASan LSan
// TODO(crbug.com/454729976): Fails on chromium/ci/win11-arm64-rel-tests.
#define MAYBE_AddContentScriptsWithNewWindowAPI \
  DISABLED_AddContentScriptsWithNewWindowAPI
#else
#define MAYBE_AddContentScriptsWithNewWindowAPI \
  AddContentScriptsWithNewWindowAPI
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewCoverageDisabledBrowserTest,
                       MAYBE_AddContentScriptsWithNewWindowAPI) {
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    // The case below currently is flaky on the linux-bfcache-rel bot with
    // back/forward cache disabled, so return early.
    // TODO(crbug.com/40947671): re-enable this test.
    return;
  }
  ASSERT_TRUE(
      RunContentScriptTestCase("AddContentScriptsWithNewWindowAPI",
                               GetTestUrl("guest_from_opener.html").spec()));
}

// https://crbug.com/665512.
IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    DISABLED_ContentScriptIsInjectedAfterTerminateAndReloadWebView) {
  ASSERT_TRUE(RunContentScriptTestCase(
      "ContentScriptIsInjectedAfterTerminateAndReloadWebView",
      GetTestUrl("empty.html").spec()));
}

// TODO(crbug.com/41284814) Flaky on CrOS trybots.
// TODO(crbug.com/40937256): Fails due to reattaching webview, need to fix on JS
// coverage builders.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
#define MAYBE_ContentScriptExistsAsLongAsWebViewTagExists \
  DISABLED_ContentScriptExistsAsLongAsWebViewTagExists
#else
#define MAYBE_ContentScriptExistsAsLongAsWebViewTagExists \
  ContentScriptExistsAsLongAsWebViewTagExists
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       MAYBE_ContentScriptExistsAsLongAsWebViewTagExists) {
  ASSERT_TRUE(
      RunContentScriptTestCase("ContentScriptExistsAsLongAsWebViewTagExists",
                               GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScriptWithCode) {
  ASSERT_TRUE(RunContentScriptTestCase("AddContentScriptWithCode",
                                       GetTestUrl("empty.html").spec()));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ContextMenuInspectElement) {
  content::ContextMenuParams params;
  content::WebContents* web_contents =
#if BUILDFLAG(IS_CHROMEOS)
      // OOBE WebUI.
      ash::LoginDisplayHost::default_host()->GetOobeWebContents();
#else
      browser()->tab_strip_model()->GetActiveWebContents();
#endif

  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

#endif  // !BUILDFLAG(IS_MAC)
