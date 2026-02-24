// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
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
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#endif

// Turn these tests off on Mac while we collect data on windows server crashes
// on mac chromium builders.
// http://crbug.com/41279287
#if !BUILDFLAG(IS_MAC)

namespace {

// Use `kChromeUIContextualTasksURL` because it is allow-listed for web view
// use in `chrome/common/extensions/api/_api_features.json`.
const char* kTestWebViewURL = chrome::kChromeUIContextualTasksURL;
const char* kTestWebViewHost = chrome::kChromeUIContextualTasksHost;

// A simple WebUI controller that serves a blank page with a <webview> tag, and
// supports loading files through the chrome://webui-test/ URL.
// Responds with a content of "%DIR_TEST_DATA%/webui/<filename>" if the request
// path has "/test/<filename>" format.
class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : content::WebUIController(web_ui) {
    web_ui->SetBindings(
        content::BindingsPolicySet({content::BindingsPolicyValue::kWebUi}));
    content::WebContents* web_contents = web_ui->GetWebContents();
    // Necessary for web view to be allowed.
    extensions::TabHelper::CreateForWebContents(web_contents);
    content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
        web_contents->GetBrowserContext(), kTestWebViewHost);
    source->SetRequestFilter(
        base::BindRepeating(&TestWebUIController::ShouldHandleRequestCallback),
        base::BindRepeating(&TestWebUIController::HandleRequestCallback));
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src chrome://webui-test;");
  }

 private:
  static bool ShouldHandleRequestCallback(const std::string& path) {
    // Only handle the root path (main HTML) or files the test framework
    // recognizes.
    return path.empty() || ShouldHandleTestFileRequestCallback(path);
  }

  // Whether the request corresponds to a test file.
  // See `HandleRequestCallback()` below for details.
  static bool ShouldHandleTestFileRequestCallback(const std::string& path) {
    std::vector<std::string> url_substr = base::SplitString(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (url_substr.size() != 2 || url_substr[0] != "test") {
      return false;
    }

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    return base::PathExists(
        test_data_dir.AppendASCII("webui").AppendASCII(url_substr[1]));
  }

  static void HandleRequestCallback(
      const std::string& path,
      content::WebUIDataSource::GotDataCallback callback) {
    if (path.empty()) {
      // Main document.
      std::move(callback).Run(new base::RefCountedString(R"(
          <!DOCTYPE html>
          <html>
            <body>
              <webview src="about:blank"></webview>
            </body>
          </html>)"));
      return;
    }

    // Test resources.
    // Responds with a content of "%DIR_TEST_DATA%/webui/<filename>" if the
    // request path has "/test/<filename>" format.
    CHECK(ShouldHandleTestFileRequestCallback(path));
    base::ScopedAllowBlockingForTesting allow_blocking;

    std::vector<std::string> url_substr = base::SplitString(
        path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::string contents;
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    CHECK(base::ReadFileToString(
        test_data_dir.AppendASCII("webui").AppendASCII(url_substr[1]),
        &contents));

    base::RefCountedString* ref_contents = new base::RefCountedString();
    ref_contents->as_string() = contents;
    std::move(callback).Run(ref_contents);
  }

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIController)

class TestWebUIConfig
    : public content::DefaultWebUIConfig<TestWebUIController> {
 public:
  TestWebUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kTestWebViewHost) {}
};

}  // namespace

class WebUIWebViewBrowserTest : public WebUIMochaBrowserTest {
 public:
  WebUIWebViewBrowserTest() = default;

  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

    web_ui_config_registration_ =
        std::make_unique<content::ScopedWebUIConfigRegistration>(
            std::make_unique<TestWebUIConfig>());
    set_test_loader_host(kTestWebViewHost);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestWebViewURL)));
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

  GURL GetTestUrl(const std::string& path) const {
    return embedded_test_server()->base_url().Resolve(path);
  }

  content::WebContents* GetWebContentsForTesting() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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

  std::unique_ptr<content::ScopedWebUIConfigRegistration>
      web_ui_config_registration_;
};

// Checks that hiding and showing the WebUI host page doesn't break guests in
// it.
// Regression test for http://crbug.com/40429108
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

#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
// TODO(crbug.com/40583245) Fails on CrOS dbg with --enable-features=Mash.
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
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
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

// https://crbug.com/41286338.
IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    DISABLED_ContentScriptIsInjectedAfterTerminateAndReloadWebView) {
  ASSERT_TRUE(RunContentScriptTestCase(
      "ContentScriptIsInjectedAfterTerminateAndReloadWebView",
      GetTestUrl("empty.html").spec()));
}

// TODO(crbug.com/41284814) Flaky on CrOS trybots.
#if BUILDFLAG(IS_CHROMEOS)
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
      browser()->tab_strip_model()->GetActiveWebContents();
  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

#endif  // !BUILDFLAG(IS_MAC)
