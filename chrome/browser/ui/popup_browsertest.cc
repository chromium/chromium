// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
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

// Tests of window placement for popup browser windows. Test fixtures are run
// with and without multi-screen Window Management permission.
class PopupBrowserTest : public InProcessBrowserTest,
                         public ::testing::WithParamInterface<bool> {
 public:
  PopupBrowserTest(const PopupBrowserTest&) = delete;
  PopupBrowserTest& operator=(const PopupBrowserTest&) = delete;

 protected:
  PopupBrowserTest() = default;
  ~PopupBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        embedder_support::kDisablePopupBlocking);
  }

  display::Display GetDisplayNearestBrowser(const Browser* browser) const {
    return display::Screen::GetScreen()->GetDisplayNearestWindow(
        browser->window()->GetNativeWindow());
  }

  Browser* OpenPopup(Browser* browser, const std::string& script) const {
    auto* contents = browser->tab_strip_model()->GetActiveWebContents();
    content::ExecuteScriptAsync(contents, script);
    Browser* popup = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_NE(popup, browser);
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(WaitForRenderFrameReady(popup_contents->GetPrimaryMainFrame()));
    return popup;
  }
};

INSTANTIATE_TEST_SUITE_P(All, PopupBrowserTest, ::testing::Bool());

// A helper class to wait for widget bounds changes beyond given thresholds.
class WidgetBoundsChangeWaiter final : public views::WidgetObserver {
 public:
  WidgetBoundsChangeWaiter(views::Widget* widget, int move_by, int resize_by)
      : widget_(widget),
        move_by_(move_by),
        resize_by_(resize_by),
        initial_bounds_(widget->GetWindowBoundsInScreen()) {
    widget_->AddObserver(this);
  }

  WidgetBoundsChangeWaiter(const WidgetBoundsChangeWaiter&) = delete;
  WidgetBoundsChangeWaiter& operator=(const WidgetBoundsChangeWaiter&) = delete;
  ~WidgetBoundsChangeWaiter() final { widget_->RemoveObserver(this); }

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
    if (!BoundsChangeMeetsThreshold(widget_->GetWindowBoundsInScreen()))
      run_loop_.Run();
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

// Ensure `left=0,top=0` popup window feature coordinates are respected.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, OpenLeftAndTopZeroCoordinates) {
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
// TODO(crbug.com/1211516): Flaky.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, DISABLED_OpenClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  EXPECT_TRUE(display.work_area().Contains(browser()->window()->GetBounds()))
      << "The browser window should be contained by its display's work area";

  // Attempt to open a popup outside the bounds of the opener's display.
  const char* const open_scripts[] = {
      "open('.', '', 'left=' + (screen.availLeft - 50));",
      "open('.', '', 'left=' + (screen.availLeft + screen.availWidth + 50));",
      "open('.', '', 'top=' + (screen.availTop - 50));",
      "open('.', '', 'top=' + (screen.availTop + screen.availHeight + 50));",
      "open('.', '', 'left=' + (screen.availLeft - 50) + "
      "',top=' + (screen.availTop - 50));",
      "open('.', '', 'left=' + (screen.availLeft - 50) + "
      "',top=' + (screen.availTop - 50) + "
      "',width=300,height=300');",
      "open('.', '', 'left=' + (screen.availLeft + screen.availWidth + 50) + "
      "',top=' + (screen.availTop + screen.availHeight + 50) + "
      "',width=300,height=300');",
      "open('.', '', 'left=' + screen.availLeft + ',top=' + screen.availTop + "
      "',width=' + (screen.availWidth + 300) + ',height=300');",
      "open('.', '', 'left=' + screen.availLeft + ',top=' + screen.availTop + "
      "',width=300,height='+ (screen.availHeight + 300));",
      "open('.', '', 'left=' + screen.availLeft + ',top=' + screen.availTop + "
      "',width=' + (screen.availWidth + 300) + "
      "',height='+ (screen.availHeight + 300));",
  };
  for (auto* const script : open_scripts) {
    Browser* popup = OpenPopup(browser(), script);
    // The popup should be constrained to the opener's available display space.
    // TODO(crbug.com/897300): Wait for the final window placement to occur;
    // this is flakily checking initial or intermediate window placement bounds.
    EXPECT_EQ(display, GetDisplayNearestBrowser(popup));
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup->window()->GetBounds().ToString();
  }
}

// Ensure popups cannot be moved beyond the available display space by script.
// TODO(crbug.com/1228795): Flaking on Linux Ozone
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE)
#define MAYBE_MoveClampedToCurrentDisplay DISABLED_MoveClampedToCurrentDisplay
#else
#define MAYBE_MoveClampedToCurrentDisplay MoveClampedToCurrentDisplay
#endif
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, MAYBE_MoveClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  const char kOpenPopup[] =
      "open('.', '', 'left=' + (screen.availLeft + 50) + "
      "',top=' + (screen.availTop + 50) + "
      "',width=150,height=100');";
  const char* const kMoveScripts[] = {
      "moveBy(screen.availWidth * 2, 0);",
      "moveBy(screen.availWidth * -2, 0);",
      "moveBy(0, screen.availHeight * 2);",
      "moveBy(0, screen.availHeight * -2);",
      "moveBy(screen.availWidth * 2, screen.availHeight * 2);",
      "moveBy(screen.availWidth * -2, screen.availHeight * -2);",
      "moveTo(screen.availLeft + screen.availWidth + 50, screen.availTop);",
      "moveTo(screen.availLeft - 50, screen.availTop);",
      "moveTo(screen.availLeft, screen.availTop + screen.availHeight + 50);",
      "moveTo(screen.availLeft, screen.availTop - 50);",
      ("moveTo(screen.availLeft + screen.availWidth + 50, "
       "screen.availTop + screen.availHeight + 50);"),
      "moveTo(screen.availLeft - 50, screen.availTop - 50);",
  };
  for (auto* const script : kMoveScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    auto popup_bounds = popup->window()->GetBounds();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    auto* widget = views::Widget::GetWidgetForNativeWindow(
        popup->window()->GetNativeWindow());

    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for the substantial move, widgets may move during initialization.
    WidgetBoundsChangeWaiter(widget, /*move_by=*/40, /*resize_by=*/0).Wait();
    EXPECT_NE(popup_bounds.origin(), popup->window()->GetBounds().origin());
    EXPECT_EQ(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup_bounds.ToString();
  }
}

// Ensure popups cannot be resized beyond the available display space by script.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, ResizeClampedToCurrentDisplay) {
  const auto display = GetDisplayNearestBrowser(browser());
  const char kOpenPopup[] =
      "open('.', '', 'left=' + (screen.availLeft + 50) + "
      "',top=' + (screen.availTop + 50) + "
      "',width=150,height=100');";
  // The popup cannot be resized beyond the current screen by script.
  const char* const kResizeScripts[] = {
      "resizeBy(screen.availWidth * 2, 0);",
      "resizeBy(0, screen.availHeight * 2);",
      "resizeTo(screen.availWidth + 200, 200);",
      "resizeTo(200, screen.availHeight + 200);",
      "resizeTo(screen.availWidth + 200, screen.availHeight + 200);",
  };
  for (auto* const script : kResizeScripts) {
    Browser* popup = OpenPopup(browser(), kOpenPopup);
    auto popup_bounds = popup->window()->GetBounds();
    auto* popup_contents = popup->tab_strip_model()->GetActiveWebContents();
    auto* widget = views::Widget::GetWidgetForNativeWindow(
        popup->window()->GetNativeWindow());

    content::ExecuteScriptAsync(popup_contents, script);
    // Wait for the substantial resize, widgets may move during initialization.
    WidgetBoundsChangeWaiter(widget, /*move_by=*/0, /*resize_by=*/100).Wait();
    EXPECT_NE(popup_bounds.size(), popup->window()->GetBounds().size());
    EXPECT_TRUE(display.work_area().Contains(popup->window()->GetBounds()))
        << " script: " << script
        << " work_area: " << display.work_area().ToString()
        << " popup: " << popup_bounds.ToString();
  }
}

// TODO(crbug.com/1183791): Disabled everywhere except ChromeOS and Mac because
// of races with SetScreenInstance and observers not being notified.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_AboutBlankCrossScreenPlacement AboutBlankCrossScreenPlacement
#else
#define MAYBE_AboutBlankCrossScreenPlacement \
  DISABLED_AboutBlankCrossScreenPlacement
#endif
// Tests that an about:blank popup can be moved across screens with permission.
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, MAYBE_AboutBlankCrossScreenPlacement) {
  display::Screen* screen = display::Screen::GetScreen();
  int actual_num_displays = screen->GetNumDisplays();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#elif BUILDFLAG(IS_MAC)
  if (!display::test::VirtualDisplayMacUtil::IsAPIAvailable()) {
    GTEST_SKIP() << "Skipping test for unsupported MacOS version.";
  }
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;
  virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
#else
  display::ScreenBase test_screen;
  test_screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      {2, gfx::Rect(901, 100, 802, 802)},
      display::DisplayList::Type::NOT_PRIMARY);
  display::Screen::SetScreenInstance(&test_screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(actual_num_displays + 1, screen->GetNumDisplays());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* opener = browser()->tab_strip_model()->GetActiveWebContents();

  // TODO(crbug.com/1119974): this test could be in content_browsertests
  // and not browser_tests if permission controls were supported.

  if (GetParam()) {  // Check whether to test multi-screen features.
    // Request and auto-accept the permission request.
    permissions::PermissionRequestManager* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(opener);
    permission_request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);
    constexpr char kGetScreensLength[] = R"(
      (async () => {
        try {
          return (await getScreenDetails()).screens.length;
        } catch {
          return 0;
        }
      })();
    )";
    EXPECT_EQ(actual_num_displays + 1, EvalJs(opener, kGetScreensLength));
    // Do not auto-accept any other permission requests.
    permission_request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::NONE);
  }

  // Open an about:blank popup. It should start on the same screen as browser().
  Browser* popup = OpenPopup(
      browser(), "w = open('about:blank', '', 'width=200,height=200');");
  const auto opener_display = GetDisplayNearestBrowser(browser());
  auto original_popup_display = GetDisplayNearestBrowser(popup);
  EXPECT_EQ(opener_display, original_popup_display);

  const auto second_display = screen->GetAllDisplays()[1];
  const std::string move_popup_to_the_second_screen_script = base::StringPrintf(
      "w.moveTo(%d, %d);", second_display.work_area().x() + 100,
      second_display.work_area().y() + 100);
  // Have the opener try to move the popup to the second screen.
  content::ExecuteScriptAsync(opener, move_popup_to_the_second_screen_script);

  // Wait for the substantial move, widgets may move during initialization.
  auto* widget = views::Widget::GetWidgetForNativeWindow(
      popup->window()->GetNativeWindow());
  WidgetBoundsChangeWaiter(widget, /*move_by=*/40, /*resize_by=*/0).Wait();
  auto new_popup_display = GetDisplayNearestBrowser(popup);
  // The popup only moves to the second screen with permission.
  EXPECT_EQ(GetParam(), original_popup_display != new_popup_display);
  EXPECT_EQ(GetParam(), second_display == new_popup_display);
  // The popup is always constrained to the bounds of the target display.
  auto popup_bounds = popup->window()->GetBounds();
  EXPECT_TRUE(new_popup_display.work_area().Contains(popup_bounds))
      << " work_area: " << new_popup_display.work_area().ToString()
      << " popup: " << popup_bounds.ToString();

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)
  display::Screen::SetScreenInstance(nullptr);
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_MAC)
}

// Opens two popups with custom position and size, but one has noopener. They
// should both have the same position and size. http://crbug.com/1011688
IN_PROC_BROWSER_TEST_P(PopupBrowserTest, NoopenerPositioning) {
  Browser* noopener_popup = OpenPopup(
      browser(),
      "open('.', '', 'noopener=1,height=200,width=200,top=100,left=100')");
  Browser* opener_popup = OpenPopup(
      browser(),
      "open('.', '', 'height=200,width=200,top=100,left=100')");
  EXPECT_EQ(noopener_popup->window()->GetBounds(),
            opener_popup->window()->GetBounds());
}

}  // namespace
