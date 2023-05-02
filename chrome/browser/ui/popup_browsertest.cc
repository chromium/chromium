// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "ui/display/mac/test/virtual_display_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

// Tests of window placement for popup browser windows.
class PopupBrowserTest : public InProcessBrowserTest {
 public:
  PopupBrowserTest() = default;
  PopupBrowserTest(const PopupBrowserTest&) = delete;
  PopupBrowserTest& operator=(const PopupBrowserTest&) = delete;

 protected:
  ~PopupBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
  }

  display::Display GetDisplayNearestBrowser(const Browser* browser) const {
    return display::Screen::GetScreen()->GetDisplayNearestWindow(
        browser->window()->GetNativeWindow());
  }

  Browser* OpenPopup(Browser* browser, const std::string& script) const {
    return OpenPopup(browser->tab_strip_model()->GetActiveWebContents(),
                     script);
  }

  Browser* OpenPopup(const content::ToRenderFrameHost& adapter,
                     const std::string& script) const {
    content::ExecuteScriptAsync(adapter, script);
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    // The popup's bounds are initialized after the synchronous window.open().
    // Ideally, this might wait for browser->renderer window bounds init via:
    // blink::mojom::Widget.UpdateVisualProperties, but it seems sufficient to
    // wait for WebContents to load the URL after the initial about:blank doc,
    // and then for that Document's readyState to be 'complete'. Anecdotally,
    // initial bounds seem settled once outerWidth and outerHeight are non-zero.
    EXPECT_TRUE(WaitForLoadStop(popup_contents));
    EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetPrimaryMainFrame()));
    EXPECT_NE("0x0", EvalJs(popup_contents, "outerWidth + 'x' + outerHeight"));
    return popup;
  }
};

// A helper class to wait for bounds changes beyond given thresholds.
class BoundsChangeWaiter final : public views::WidgetObserver {
 public:
  BoundsChangeWaiter(Browser* browser, int move_by, int resize_by)
      : widget_(views::Widget::GetWidgetForNativeWindow(
            browser->window()->GetNativeWindow())),
        move_by_(move_by),
        resize_by_(resize_by),
        initial_bounds_(widget_->GetWindowBoundsInScreen()) {
    widget_->AddObserver(this);
  }

  BoundsChangeWaiter(const BoundsChangeWaiter&) = delete;
  BoundsChangeWaiter& operator=(const BoundsChangeWaiter&) = delete;
  ~BoundsChangeWaiter() final { widget_->RemoveObserver(this); }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& rect) final {
    if (BoundsChangeMeetsThreshold(rect)) {
      widget_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  // Wait for changes to occur, or return immediately if they already have.
  void Wait() {
    if (!BoundsChangeMeetsThreshold(widget_->GetWindowBoundsInScreen())) {
      run_loop_.Run();
    }
  }

 private:
  bool BoundsChangeMeetsThreshold(const gfx::Rect& rect) const {
    return (std::abs(rect.x() - initial_bounds_.x()) >= move_by_ ||
            std::abs(rect.y() - initial_bounds_.y()) >= move_by_) &&
           (std::abs(rect.width() - initial_bounds_.width()) >= resize_by_ ||
            std::abs(rect.height() - initial_bounds_.height()) >= resize_by_);
  }

  const raw_ptr<views::Widget> widget_;
  const int move_by_, resize_by_;
  const gfx::Rect initial_bounds_;
  base::RunLoop run_loop_;
};

// A helper class to wait for the bounds of two widgets to become equal.
class WidgetBoundsEqualWaiter final : public views::WidgetObserver {
 public:
  WidgetBoundsEqualWaiter(views::Widget* widget, views::Widget* widget_cmp)
      : widget_(widget), widget_cmp_(widget_cmp) {
    widget_->AddObserver(this);
    widget_cmp_->AddObserver(this);
  }

  WidgetBoundsEqualWaiter(const WidgetBoundsEqualWaiter&) = delete;
  WidgetBoundsEqualWaiter& operator=(const WidgetBoundsEqualWaiter&) = delete;
  ~WidgetBoundsEqualWaiter() final {
    widget_->RemoveObserver(this);
    widget_cmp_->RemoveObserver(this);
  }

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& rect) final {
    if (WidgetsBoundsEqual()) {
      widget_->RemoveObserver(this);
      widget_cmp_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  // Wait for changes to occur, or return immediately if they already have.
  void Wait() {
    if (!WidgetsBoundsEqual()) {
      run_loop_.Run();
    }
  }

 private:
  bool WidgetsBoundsEqual() {
    return widget_->GetWindowBoundsInScreen() ==
           widget_cmp_->GetWindowBoundsInScreen();
  }
  const raw_ptr<views::Widget> widget_ = nullptr;
  const raw_ptr<views::Widget> widget_cmp_ = nullptr;
  base::RunLoop run_loop_;
};

// Ensure `left=0,top=0` popup window feature coordinates are respected.
IN_PROC_BROWSER_TEST_F(PopupBrowserTest, OpenLeftAndTopZeroCoordinates) {
  // Attempt to open a popup at (0,0). Its bounds should match the request, but
  // be adjusted to meet minimum size and available display area constraints.
  Browser* popup =
      OpenPopup(browser(), "open('.', '', 'left=0,top=0,width=50,height=50')");
  const gfx::Rect work_area = GetDisplayNearestBrowser(popup).work_area();
  gfx::Rect expected(popup->window()->GetBounds().size());
  expected.AdjustToFit(work_area);
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/1286870) Desktop Linux window bounds are inaccurate.
  expected.Outset(50);
  EXPECT_TRUE(expected.Contains(popup->window()->GetBounds()))
      << " expected: " << expected.ToString()
      << " popup: " << popup->window()->GetBounds().ToString()
      << " work_area: " << work_area.ToString();
#else
  EXPECT_EQ(expected.ToString(), popup->window()->GetBounds().ToString())
      << " work_area: " << work_area.ToString();
#endif
}

// Ensure popups are opened in the available space of the opener's display.
IN_PROC_BROWSER_TEST_F(PopupBrowserTest, OpenClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  ASSERT_TRUE(display.work_area().Contains(browser()->window()->GetBounds()))
      << "The browser window should be contained by its display's work area";

  // Attempt to open popups outside the bounds of the opener's display.
  const char* const open_features[] = {
      ("left=${screen.availLeft-50},top=${screen.availTop-50}"
       ",width=200,height=200"),
      ("left=${screen.availLeft+screen.availWidth+50}"
       ",top=${screen.availTop+screen.availHeight+50},width=200,height=200"),
      ("left=${screen.availLeft+screen.availWidth-50}"
       ",top=${screen.availTop+screen.availHeight-50},width=500,height=500,"),
      "width=${screen.availWidth+300},height=${screen.availHeight+300}",
  };
  for (auto* const features : open_features) {
    const std::string script = "open('.', '', `" + std::string(features) + "`)";
    Browser* popup = OpenPopup(browser(), script);
    // The popup should be constrained to the opener's available display space.
    EXPECT_EQ(display, GetDisplayNearestBrowser(popup));
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup->window()->GetBounds().ToString();
  }
}

// Ensure popups cannot be moved beyond the available display space by script.
IN_PROC_BROWSER_TEST_F(PopupBrowserTest, MoveClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  ASSERT_TRUE(display.work_area().Contains(browser()->window()->GetBounds()))
      << "The browser window should be contained by its display's work area";
  const char kOpenPopup[] =
      ("open('.', '', `left=${screen.availLeft+screen.availWidth/2}"
       ",top=${screen.availTop+screen.availHeight/2},width=200,height=200`)");

  const char* const kMoveScripts[] = {
      "moveBy(screen.availWidth*2, screen.availHeight* 2)",
      "moveBy(screen.availWidth*-2, screen.availHeight*-2)",
      ("moveTo(screen.availLeft+screen.availWidth+50,"
       "screen.availTop+screen.availHeight+50)"),
      "moveTo(screen.availLeft-50, screen.availTop-50)",
  };
  for (auto* const script : kMoveScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    auto popup_bounds = popup->window()->GetBounds();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    SCOPED_TRACE(testing::Message()
                 << " script: " << script
                 << " work_area: " << display.work_area().ToString()
                 << " popup-before: " << popup_bounds.ToString());
    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for a substantial move, bounds change during init.
    BoundsChangeWaiter(popup, /*move_by=*/40, /*resize_by=*/0).Wait();
    EXPECT_NE(popup_bounds.origin(), popup->window()->GetBounds().origin());
    EXPECT_EQ(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " popup-after: " << popup->window()->GetBounds().ToString();
  }
}

// Ensure popups cannot be resized beyond the available display space by script.
IN_PROC_BROWSER_TEST_F(PopupBrowserTest, ResizeClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  const char kOpenPopup[] =
      ("open('.', '', `left=${screen.availLeft},top=${screen.availTop}"
       ",width=200,height=200`)");

  const char* const kResizeScripts[] = {
      "resizeBy(screen.availWidth*2, screen.availHeight*2)",
      "resizeTo(screen.availWidth+200, screen.availHeight+200)",
  };
  for (auto* const script : kResizeScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    auto popup_bounds = popup->window()->GetBounds();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    SCOPED_TRACE(testing::Message()
                 << " script: " << script
                 << " work_area: " << display.work_area().ToString()
                 << " popup-before: " << popup_bounds.ToString());
    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for a substantial resize, bounds change during init.
    BoundsChangeWaiter(popup, /*move_by=*/0, /*resize_by=*/100).Wait();
    EXPECT_NE(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " popup-after: " << popup->window()->GetBounds().ToString();
  }
}

// Opens two popups with custom position and size, but one has noopener. They
// should both have the same position and size. http://crbug.com/1011688
IN_PROC_BROWSER_TEST_F(PopupBrowserTest, NoopenerPositioning) {
  const char kFeatures[] =
      "left=${screen.availLeft},top=${screen.availTop},width=200,height=200";
  Browser* noopener_popup = OpenPopup(
      browser(), "open('.', '', `noopener=1," + std::string(kFeatures) + "`)");
  Browser* opener_popup =
      OpenPopup(browser(), "open('.', '', `" + std::string(kFeatures) + "`)");

  WidgetBoundsEqualWaiter(views::Widget::GetWidgetForNativeWindow(
                              noopener_popup->window()->GetNativeWindow()),
                          views::Widget::GetWidgetForNativeWindow(
                              opener_popup->window()->GetNativeWindow()))
      .Wait();

  EXPECT_EQ(noopener_popup->window()->GetBounds(),
            opener_popup->window()->GetBounds());
}

// Tests popups with extended features from the Window Management API.
// Test fixtures are run with and without multi-screen Window Management
// permission. Manages virtual displays on supported platforms.
class WindowManagementPopupBrowserTest
    : public PopupBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  WindowManagementPopupBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFullscreenPopupWindows}, {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PopupBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/empty.html")));
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_MAC)
    virtual_display_util_.reset();
#endif
    PopupBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool ShouldTestWindowManagement() { return GetParam(); }

  // Requests screen details and grants window management permission.
  void SetUpWindowManagement() {
    if (!ShouldTestWindowManagement()) {
      return;
    }
    auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
    // Request and auto-accept the permission request.
    permissions::PermissionRequestManager* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(contents);
    permission_request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);
    ASSERT_GT(EvalJs(contents,
                     R"JS(getScreenDetails().then(s => {
                            window.screenDetails = s;
                            return s.screens.length; }))JS"),
              0);
    // Do not auto-accept any other permission requests.
    permission_request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::NONE);
  }

  // Waits until an element is fullscreen in the specified web contents.
  // Returns immediately if an element is already fullscreen.
  void WaitForHTMLFullscreen(content::WebContents* contents) {
    content::WaitForLoadStop(contents);
    ASSERT_TRUE(EvalJs(contents, R"JS(
          (new Promise(r => {
            if (!!document.fullscreenElement) {
              r();
            } else {
              document.addEventListener(`fullscreenchange`,
                () => { if (!!document.fullscreenElement) r(); },
                {once: true}
              );
            }
          })))JS")
                    .error.empty());
  }

  // Attempts to create virtual displays such that 2 displays become available
  // for testing multi-screen functionality. Not all platforms and OS versions
  // are supported. Returns false if virtual displays could not be created.
  // If the host already has 2 or more displays available, no virtual displays
  // are created.
  bool SetUpVirtualDisplays() {
    if (display::Screen::GetScreen()->GetNumDisplays() > 1) {
      return true;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay("100+100-801x802,901+100-802x802");
    AssertMinimumDisplayCount(2);
    return true;
#elif BUILDFLAG(IS_MAC)
    if (display::test::VirtualDisplayMacUtil::IsAPIAvailable()) {
      virtual_display_util_ =
          std::make_unique<display::test::VirtualDisplayMacUtil>();
      virtual_display_util_->AddDisplay(
          1, display::test::VirtualDisplayMacUtil::k1920x1080);
      AssertMinimumDisplayCount(2);
      return true;
    }
    return false;
#else
    return false;
#endif
  }

  // Asserts that the test environment has at least `count` screens available.
  void AssertMinimumDisplayCount(int count) {
    ASSERT_GE(count, display::Screen::GetScreen()->GetNumDisplays());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<display::test::VirtualDisplayMacUtil> virtual_display_util_;
#endif
};

INSTANTIATE_TEST_SUITE_P(All,
                         WindowManagementPopupBrowserTest,
                         ::testing::Bool());

// Tests opening a popup on another screen.
IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest, OpenOnAnotherScreen) {
  if (!SetUpVirtualDisplays()) {
    GTEST_SKIP() << "Virtual displays not supported on this platform.";
  }
  AssertMinimumDisplayCount(2);
  SetUpWindowManagement();
  auto* screen = display::Screen::GetScreen();
  for (const auto& opener_screen : screen->GetAllDisplays()) {
    browser()->window()->SetBounds(opener_screen.work_area());
    ASSERT_EQ(opener_screen, GetDisplayNearestBrowser(browser()));
    for (const auto& target_screen : screen->GetAllDisplays()) {
      for (const char* url : {".", "about:blank"}) {
        const std::string open_script = content::JsReplace(
            "open('$1', '', 'left=$2,top=$3,width=200,height=200')", url,
            target_screen.work_area().x(), target_screen.work_area().y());
        Browser* popup = OpenPopup(browser(), open_script);
        auto popup_display = GetDisplayNearestBrowser(popup);
        // The popup only opens on another screen with permission.
        EXPECT_EQ(ShouldTestWindowManagement() ? target_screen : opener_screen,
                  GetDisplayNearestBrowser(popup));
        // The popup is constrained to the available bounds of its screen.
        auto popup_bounds = popup->window()->GetBounds();
        EXPECT_TRUE(popup_display.work_area().Contains(popup_bounds))
            << " work_area: " << popup_display.work_area().ToString()
            << " popup: " << popup_bounds.ToString();
      }
    }
  }
}

// Tests opening a popup on the same screen, then moving it to another screen.
IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest, MoveToAnotherScreen) {
  if (!SetUpVirtualDisplays()) {
    GTEST_SKIP() << "Virtual displays not supported on this platform.";
  }
  AssertMinimumDisplayCount(2);
  SetUpWindowManagement();
  auto* opener = browser()->tab_strip_model()->GetActiveWebContents();
  auto* screen = display::Screen::GetScreen();
  for (const auto& opener_screen : screen->GetAllDisplays()) {
    browser()->window()->SetBounds(opener_screen.work_area());
    ASSERT_EQ(opener_screen, GetDisplayNearestBrowser(browser()));
    const auto opener_screen_center = opener_screen.work_area().CenterPoint();
    for (const auto& target_screen : screen->GetAllDisplays()) {
      for (const char* url : {".", "about:blank"}) {
        const std::string open_script = content::JsReplace(
            "w = open('$1', '', 'left=$2,top=$3,width=200,height=200')", url,
            opener_screen_center.x() - 100, opener_screen_center.y() - 100);
        Browser* popup = OpenPopup(browser(), open_script);
        EXPECT_EQ(opener_screen, GetDisplayNearestBrowser(popup));

        // Have the opener try to move the popup to the target screen.
        const std::string move_script = content::JsReplace(
            "w.moveTo($1, $2);", target_screen.work_area().x(),
            target_screen.work_area().y());
        content::ExecuteScriptAsync(opener, move_script);
        BoundsChangeWaiter(popup, /*move_by=*/40, /*resize_by=*/0).Wait();
        auto popup_display = GetDisplayNearestBrowser(popup);

        // The popup only moves to another screen with permission.
        EXPECT_EQ(ShouldTestWindowManagement() ? target_screen : opener_screen,
                  popup_display);
        // The popup is constrained to the available bounds of its screen.
        auto popup_bounds = popup->window()->GetBounds();
        EXPECT_TRUE(popup_display.work_area().Contains(popup_bounds))
            << " work_area: " << popup_display.work_area().ToString()
            << " popup: " << popup_bounds.ToString();
      }
    }
  }
}

// Tests opening a popup on another screen from a cross-origin iframe.
IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest, CrossOriginIFrame) {
  if (!SetUpVirtualDisplays()) {
    GTEST_SKIP() << "Virtual displays not supported on this platform.";
  }
  AssertMinimumDisplayCount(2);
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("a.com", "/empty.html")));
  SetUpWindowManagement();

  // Append cross-origin iframes with and without the permission policy.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL src = https_server.GetURL("b.com", "/empty.html");
  const std::string script = R"JS(
    new Promise(resolve => {
      let f = document.createElement('iframe');
      f.src = $1;
      f.allow = $2 ? 'window-management' : '';
      f.addEventListener('load', () => resolve(true));
      document.body.appendChild(f);
    });
  )JS";
  EXPECT_EQ(true, EvalJs(web_contents, content::JsReplace(script, src, false)));
  EXPECT_EQ(true, EvalJs(web_contents, content::JsReplace(script, src, true)));

  auto* screen = display::Screen::GetScreen();
  for (const auto& opener_screen : screen->GetAllDisplays()) {
    browser()->window()->SetBounds(opener_screen.work_area());
    ASSERT_EQ(opener_screen, GetDisplayNearestBrowser(browser()));
    for (const bool iframe_policy_granted : {true, false}) {
      content::RenderFrameHost* cross_origin_iframe =
          ChildFrameAt(web_contents, iframe_policy_granted ? 1 : 0);
      ASSERT_TRUE(cross_origin_iframe);
      ASSERT_NE(cross_origin_iframe->GetLastCommittedOrigin(),
                web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
      for (const auto& target_screen : screen->GetAllDisplays()) {
        for (const char* url : {".", "about:blank"}) {
          const std::string open_script = content::JsReplace(
              "w = open('$1', '', 'left=$2,top=$3,width=200,height=200')", url,
              target_screen.work_area().x(), target_screen.work_area().y());
          Browser* popup = OpenPopup(cross_origin_iframe, open_script);
          // The popup only opens on another screen with permission.
          EXPECT_EQ(ShouldTestWindowManagement() && iframe_policy_granted
                        ? target_screen
                        : opener_screen,
                    GetDisplayNearestBrowser(popup));
        }
      }
    }
  }
}

IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest, BasicFullscreen) {
  SetUpWindowManagement();
  Browser* new_popup =
      OpenPopup(browser(), "open('/empty.html', '_blank', 'popup,fullscreen')");
  content::WebContents* new_contents =
      new_popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(new_contents);
  }
  EXPECT_EQ(EvalJs(new_contents,
                   "!!document.fullscreenElement && document.fullscreenElement "
                   "== document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  FullscreenController* fullscreen_controller =
      new_popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
  EXPECT_EQ(EvalJs(new_contents, "document.exitFullscreen()").error.empty(),
            ShouldTestWindowManagement());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());

  // Test that a navigation doesn't re-trigger fullscreen.
  EXPECT_TRUE(EvalJs(new_contents,
                     "window.location.href = '" +
                         embedded_test_server()->GetURL("/title1.html").spec() +
                         "'")
                  .error.empty());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest, AboutBlankFullscreen) {
  SetUpWindowManagement();
  Browser* new_popup =
      OpenPopup(browser(), "open('about:blank', '_blank', 'popup,fullscreen')");
  content::WebContents* new_contents =
      new_popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(new_contents);
  }
  EXPECT_EQ(EvalJs(new_contents,
                   "!!document.fullscreenElement && document.fullscreenElement "
                   "== document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  FullscreenController* fullscreen_controller =
      new_popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
  EXPECT_EQ(EvalJs(new_contents, "document.exitFullscreen()").error.empty(),
            ShouldTestWindowManagement());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());

  // Test that a navigation doesn't re-trigger fullscreen.
  EXPECT_TRUE(EvalJs(new_contents,
                     "window.location.href = '" +
                         embedded_test_server()->GetURL("/title1.html").spec() +
                         "'")
                  .error.empty());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest, FullscreenWithBounds) {
  SetUpWindowManagement();
  Browser* new_popup =
      OpenPopup(browser(),
                "open('/empty.html', '_blank', "
                "'height=200,width=200,top=100,left=100,fullscreen')");
  content::WebContents* new_contents =
      new_popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(new_contents);
  }
  EXPECT_EQ(EvalJs(new_contents,
                   "!!document.fullscreenElement && document.fullscreenElement "
                   "== document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  FullscreenController* fullscreen_controller =
      new_popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
}

// Fullscreen should not work if the new window is not specified as a popup.
IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest,
                       FullscreenRequiresPopupWindowFeature) {
  SetUpWindowManagement();

  // OpenPopup() cannot be used here since it waits for a new browser which
  // would not open in this case.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      EvalJs(web_contents, "open('/empty.html', '_blank', 'fullscreen')")
          .error.empty());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_FALSE(
      EvalJs(web_contents, "!!document.fullscreenElement").ExtractBool());
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

// Tests that the fullscreen flag is ignored if the window.open() does not
// result in a new window.
IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest,
                       FullscreenRequiresNewWindow) {
  SetUpWindowManagement();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe.html")));
  // OpenPopup() cannot be used here since it waits for a new browser which
  // would not open in this case. open() targeting a frame named "test" in
  // "iframe.html" will not create a new window.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      EvalJs(web_contents, "open('/empty.html', 'test', 'popup,fullscreen')")
          .error.empty());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FALSE(
      EvalJs(web_contents, "!!document.fullscreenElement").ExtractBool());
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

IN_PROC_BROWSER_TEST_P(WindowManagementPopupBrowserTest,
                       FullscreenDifferentScreen) {
  if (!SetUpVirtualDisplays()) {
    GTEST_SKIP() << "Virtual displays not supported on this platform.";
  }
  SetUpWindowManagement();

  // Falls back to opening a popup on the current screen in testing scenarios
  // where window management is not granted in SetUpWindowManagement().
  Browser* new_popup = OpenPopup(browser(), R"JS(
    (() =>
          {
            otherScreen = (!!window.screenDetails && screenDetails.screens
              .find(s => s != screenDetails.currentScreen)) || window.screen;
            return open('/empty.html', '_blank',
                    `top=${otherScreen.availTop},
                    left=${otherScreen.availLeft},
                    height=200,
                    width=200,
                    popup,
                    fullscreen`);
          })()
  )JS");

  content::WebContents* new_contents =
      new_popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(new_contents);
  }
  EXPECT_EQ(EvalJs(new_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  EXPECT_TRUE(EvalJs(new_contents,
                     "screen.availLeft == opener.otherScreen.availLeft && "
                     "screen.availTop == opener.otherScreen.availTop")
                  .ExtractBool());
  FullscreenController* fullscreen_controller =
      new_popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
}

}  // namespace
