// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#endif

// Turn these tests off on Mac while we collect data on windows server crashes
// on mac chromium builders.
// http://crbug.com/653353
#if !BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA)
#include "ui/aura/window.h"

namespace {

class WebUIMessageListener : public base::SupportsWeakPtr<WebUIMessageListener>{
 public:
  WebUIMessageListener(content::WebUI* web_ui, const std::string& message)
      : message_loop_(new content::MessageLoopRunner) {
    web_ui->RegisterMessageCallback(
        message,
        base::BindRepeating(&WebUIMessageListener::HandleMessage, AsWeakPtr()));
  }

  WebUIMessageListener(const WebUIMessageListener&) = delete;
  WebUIMessageListener& operator=(const WebUIMessageListener&) = delete;

  bool Wait() {
    message_loop_->Run();
    return true;
  }

 private:
  void HandleMessage(const base::Value::List& test_result) {
    message_loop_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_;
};

class DNDToInputNavigationObserver : public content::WebContentsObserver {
 public:
  explicit DNDToInputNavigationObserver(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  DNDToInputNavigationObserver(const DNDToInputNavigationObserver&) = delete;
  DNDToInputNavigationObserver& operator=(const DNDToInputNavigationObserver&) =
      delete;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigated = true;
  }
  bool Navigated() const { return navigated; }

 private:
  bool navigated = false;
};

int ExecuteHostScriptAndExtractInt(content::WebContents* web_contents,
                                   const std::string& script) {
  int result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents, "window.domAutomationController.send(" + script + ");",
      &result));
  return result;
}

int ExecuteGuestScriptAndExtractInt(content::WebContents* web_contents,
                                    const std::string& web_view_id,
                                    const std::string& script) {
  int result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "document.getElementById('" + web_view_id + "').executeScript({ "
          "code: '" + script + "' }, function (results) {"
          " window.domAutomationController.send(results[0]);});",
      &result));
  return result;
}
}  // namespace
#endif

class WebUIWebViewBrowserTest : public WebUIBrowserTest {
 public:
  WebUIWebViewBrowserTest() {}

  WebUIWebViewBrowserTest(const WebUIWebViewBrowserTest&) = delete;
  WebUIWebViewBrowserTest& operator=(const WebUIWebViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    WebUIBrowserTest::SetUpOnMainThread();
    AddLibrary(
        base::FilePath(FILE_PATH_LITERAL("webview_content_script_test.js")));
    AddLibrary(
        base::FilePath(FILE_PATH_LITERAL("webview_basic.js")));

    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Wait for the OOBE WebUI to be shown.
    ash::OobeScreenWaiter(ash::WelcomeView::kScreenId).Wait();
    SetWebUIInstance(
        ash::LoginDisplayHost::default_host()->GetOobeUI()->web_ui());
#else
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL()));
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIBrowserTest::SetUpCommandLine(command_line);
    // Force showing OOBE WebUI on the ChromeOS ASH configuration.
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }
#endif

  GURL GetTestUrl(const std::string& path) const {
    return embedded_test_server()->base_url().Resolve(path);
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  GURL GetWebViewEnabledWebUIURL() const {
    return GURL(signin::GetEmbeddedPromoURL(
        signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
        signin_metrics::Reason::kForcedSigninPrimaryAccount, false));
  }
#endif
};

// Checks that hiding and showing the WebUI host page doesn't break guests in
// it.
// Regression test for http://crbug.com/515268
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, DisplayNone) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testDisplayNone", base::Value(GetTestUrl("empty.html").spec())));
}

// TODO(crbug.com/861600) Flaky on CrOS trybots.
#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(NDEBUG)
#define MAYBE_ExecuteScriptCode DISABLED_ExecuteScriptCode
#else
#define MAYBE_ExecuteScriptCode ExecuteScriptCode
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, MAYBE_ExecuteScriptCode) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testExecuteScriptCode", base::Value(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ExecuteScriptCodeFromFile) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testExecuteScriptCodeFromFile",
      base::Value(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScript) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScript", base::Value(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddMultiContentScripts) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddMultiContentScripts",
      base::Value(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    AddContentScriptWithSameNameShouldOverwriteTheExistingOne) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptWithSameNameShouldOverwriteTheExistingOne",
      base::Value(GetTestUrl("empty.html").spec())));
}

#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(NDEBUG)
// TODO(crbug.com/859320) Fails on CrOS dbg with --enable-features=Mash.
#define MAYBE_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView \
  DISABLED_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView
#else
#define MAYBE_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView \
  AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView
#endif
IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    MAYBE_AddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView",
      base::Value(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddAndRemoveContentScripts) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddAndRemoveContentScripts",
      base::Value(GetTestUrl("empty.html").spec())));
}

#if BUILDFLAG(IS_CHROMEOS_ASH) && \
    (!defined(NDEBUG) || defined(ADDRESS_SANITIZER))
// TODO(crbug.com/859320) Fails on CrOS dbg with --enable-features=Mash.
// TODO(crbug.com/893472) Flaky on CrOS ASan LSan
#define MAYBE_AddContentScriptsWithNewWindowAPI \
  DISABLED_AddContentScriptsWithNewWindowAPI
#else
#define MAYBE_AddContentScriptsWithNewWindowAPI \
  AddContentScriptsWithNewWindowAPI
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       MAYBE_AddContentScriptsWithNewWindowAPI) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptsWithNewWindowAPI",
      base::Value(GetTestUrl("guest_from_opener.html").spec())));
}

// https://crbug.com/665512.
IN_PROC_BROWSER_TEST_F(
    WebUIWebViewBrowserTest,
    DISABLED_ContentScriptIsInjectedAfterTerminateAndReloadWebView) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testContentScriptIsInjectedAfterTerminateAndReloadWebView",
      base::Value(GetTestUrl("empty.html").spec())));
}

// TODO(crbug.com/662673) Flaky on CrOS trybots.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ContentScriptExistsAsLongAsWebViewTagExists \
  DISABLED_ContentScriptExistsAsLongAsWebViewTagExists
#else
#define MAYBE_ContentScriptExistsAsLongAsWebViewTagExists \
  ContentScriptExistsAsLongAsWebViewTagExists
#endif
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       MAYBE_ContentScriptExistsAsLongAsWebViewTagExists) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testContentScriptExistsAsLongAsWebViewTagExists",
      base::Value(GetTestUrl("empty.html").spec())));
}

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, AddContentScriptWithCode) {
  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScriptWithCode",
      base::Value(GetTestUrl("empty.html").spec())));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Incognito on CrOS is tested by the usual AddContentScript - OOBE context is
// running inside incognito (signin) profile.

// TODO(crbug.com/662673) Flaky
#define MAYBE_AddContentScriptIncognito DISABLED_AddContentScriptIncognito
// Right now we only have incognito WebUI on CrOS, but this should
// theoretically work for all platforms.
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest,
                       MAYBE_AddContentScriptIncognito) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GetWebViewEnabledWebUIURL());

  SetWebUIInstance(
      incognito_browser->tab_strip_model()->GetActiveWebContents()->GetWebUI());

  ASSERT_TRUE(WebUIBrowserTest::RunJavascriptAsyncTest(
      "testAddContentScript", base::Value(GetTestUrl("empty.html").spec())));
}
#endif

IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, ContextMenuInspectElement) {
  content::ContextMenuParams params;
  content::WebContents* web_contents =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // OOBE WebUI.
      ash::LoginDisplayHost::default_host()->GetOobeWebContents();
#else
      browser()->tab_strip_model()->GetActiveWebContents();
#endif

  TestRenderViewContextMenu menu(*web_contents->GetPrimaryMainFrame(), params);
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(WebUIWebViewBrowserTest, DISABLED_DragAndDropToInput) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetWebViewEnabledWebUIURL()));
  ASSERT_TRUE(
      WebUIBrowserTest::RunJavascriptAsyncTest("testDragAndDropToInput"));

  content::WebContents* const embedder_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();
  content::RenderViewHost* const render_view_host =
      embedder_web_contents->GetPrimaryMainFrame()->GetRenderViewHost();

  gfx::NativeView view = embedder_web_contents->GetNativeView();
  view->SetBounds(gfx::Rect(0, 0, 400, 400));

  const gfx::Rect webview_rect(
      ExecuteHostScriptAndExtractInt(embedder_web_contents,
                                     "webview.offsetLeft"),
      ExecuteHostScriptAndExtractInt(embedder_web_contents,
                                     "webview.offsetTop"),
      ExecuteHostScriptAndExtractInt(embedder_web_contents,
                                     "webview.offsetWidth"),
      ExecuteHostScriptAndExtractInt(embedder_web_contents,
                                     "webview.offsetHeight"));

  const gfx::Rect guest_dest_rect(
      ExecuteGuestScriptAndExtractInt(embedder_web_contents, "webview",
                                      "destNode.offsetLeft"),
      ExecuteGuestScriptAndExtractInt(embedder_web_contents, "webview",
                                      "destNode.offsetTop"),
      ExecuteGuestScriptAndExtractInt(embedder_web_contents, "webview",
                                      "destNode.offsetWidth"),
      ExecuteGuestScriptAndExtractInt(embedder_web_contents, "webview",
                                      "destNode.offsetHeight"));
  const gfx::PointF client_pt(
      guest_dest_rect.x() + guest_dest_rect.width() / 2.0 + webview_rect.x(),
      guest_dest_rect.y() + guest_dest_rect.height() / 2.0 + webview_rect.y());
  gfx::Rect container_bounds = embedder_web_contents->GetContainerBounds();
  const gfx::PointF screen_pt(container_bounds.x(), container_bounds.y());
  const blink::DragOperationsMask drag_operation_mask =
      static_cast<blink::DragOperationsMask>(blink::kDragOperationCopy |
                                             blink::kDragOperationLink |
                                             blink::kDragOperationMove);
  content::DropData dropdata;
  dropdata.did_originate_from_renderer = true;
  dropdata.url = GURL(url::kAboutBlankURL);
  dropdata.url_title = std::u16string(u"Drop me");

  // Drag url into input in webview.

  // TODO(paulmeyer): The following drag-and-drop calls on
  // render_view_host->GetWidget() will need to be targeted to specific
  // RenderWidgetHosts in order to work with OOPIFs. See crbug.com/647249.

  {
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents,
                                       "console.log('step1: Drag Enter')"));

    WebUIMessageListener listener(embedder_web_contents->GetWebUI(),
                                  "Step1: destNode gets dragenter");
    render_view_host->GetWidget()->FilterDropData(&dropdata);
    render_view_host->GetWidget()->DragTargetDragEnter(
        dropdata, client_pt, screen_pt, drag_operation_mask,
        blink::WebInputEvent::kLeftButtonDown, base::DoNothing());
    ASSERT_TRUE(listener.Wait());
  }

  {
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents,
                                       "console.log('step2: Drag Over')"));

    WebUIMessageListener listener(embedder_web_contents->GetWebUI(),
                                  "Step2: destNode gets dragover");
    render_view_host->GetWidget()->DragTargetDragOver(
        client_pt, screen_pt, drag_operation_mask,
        blink::WebInputEvent::kLeftButtonDown, base::DoNothing());
    ASSERT_TRUE(listener.Wait());
  }

  {
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents,
                                       "console.log('step3: Drop')"));

    DNDToInputNavigationObserver observer(embedder_web_contents);
    WebUIMessageListener listener(embedder_web_contents->GetWebUI(),
                                  "Step3: destNode gets drop");
    render_view_host->GetWidget()->DragTargetDrop(
        dropdata, client_pt, screen_pt, 0, base::DoNothing());
    ASSERT_TRUE(listener.Wait());
    // Confirm no navigation
    EXPECT_FALSE(observer.Navigated());
    EXPECT_EQ(GetWebViewEnabledWebUIURL(), embedder_web_contents->GetURL());
  }
}
#endif

#endif  // !BUILDFLAG(IS_MAC)
