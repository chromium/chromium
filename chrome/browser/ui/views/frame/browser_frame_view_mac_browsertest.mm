// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_mac.h"

#include <AppKit/AppKit.h>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_view.h"
#include "url/gurl.h"

namespace {

class TextChangeWaiter {
 public:
  explicit TextChangeWaiter(views::Label* label)
      : subscription_(label->AddTextChangedCallback(
            base::BindRepeating(&TextChangeWaiter::OnTextChanged,
                                base::Unretained(this)))) {}

  TextChangeWaiter(const TextChangeWaiter&) = delete;
  TextChangeWaiter& operator=(const TextChangeWaiter&) = delete;

  // Runs a loop until a text change is observed (unless one has
  // already been observed, in which case it returns immediately).
  void Wait() {
    if (observed_change_) {
      return;
    }

    run_loop_.Run();
  }

 private:
  void OnTextChanged() {
    observed_change_ = true;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  bool observed_change_ = false;
  base::RunLoop run_loop_;
  base::CallbackListSubscription subscription_;
};

}  // anonymous namespace

enum class PrefixTitles { kEnabled, kDisabled };

using BrowserFrameViewMacBrowserTestTitlePrefixed =
    web_app::WebAppBrowserTestBase;

// This will always be flaky on mac due to RemoteCocoa, the way it mocks out
// fullscreen doesn't play with remote cocoa. So it gets true fullscreen (which
// would probably be flaky in browser tests), but then also hits the DCHECK
// because it tests real full screen. https://crbug.com/333417404
#if BUILDFLAG(IS_MAC)
#define MAYBE_TitleUpdates DISABLED_TitleUpdates
#else
#define MAYBE_TitleUpdates TitleUpdates
#endif
IN_PROC_BROWSER_TEST_F(BrowserFrameViewMacBrowserTestTitlePrefixed,
                       MAYBE_TitleUpdates) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(start_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Ensure the main page has loaded and is ready for ExecJs DOM manipulation.
  ASSERT_TRUE(content::NavigateToURL(web_contents, start_url));

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::FrameView* const frame_view =
      browser_view->GetWidget()->non_client_view()->frame_view();
  auto* const title =
      static_cast<views::Label*>(frame_view->GetViewByID(VIEW_ID_WINDOW_TITLE));

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_TRUE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(u"A Web App - Full Screen", title->GetText());
  }

  {
    chrome::ToggleFullscreenMode(browser);
    EXPECT_FALSE(browser_view->GetWidget()->IsFullscreen());
    TextChangeWaiter waiter(title);
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        "document.querySelector('title').textContent = 'Not Full Screen'"));
    waiter.Wait();
    EXPECT_EQ(u"A Web App - Not Full Screen", title->GetText());
  }
}

using BrowserFrameViewMacBrowserTest = web_app::WebAppBrowserTestBase;

// Test to make sure the WebAppToolbarFrame triggers an InvalidateLayout() when
// toggled in fullscreen mode.
// TODO(crbug.com/40735737): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ToolbarLayoutFullscreenTransition \
  DISABLED_ToolbarLayoutFullscreenTransition
#else
#define MAYBE_ToolbarLayoutFullscreenTransition \
  ToolbarLayoutFullscreenTransition
#endif
IN_PROC_BROWSER_TEST_F(BrowserFrameViewMacBrowserTest,
                       MAYBE_ToolbarLayoutFullscreenTransition) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;

  const GURL start_url = GetInstallableAppURL();
  const webapps::AppId app_id = InstallPWA(start_url);
  Browser* const browser = LaunchWebAppBrowser(app_id);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  BrowserFrameView* const frame_view = static_cast<BrowserFrameView*>(
      browser_view->GetWidget()->non_client_view()->frame_view());

  // Trigger a layout on the view tree to address any invalid layouts waiting
  // for a re-layout.
  views::ViewTestApi frame_view_test_api(frame_view);
  browser_view->GetWidget()->LayoutRootViewIfNecessary();

  // Assert that the layout of the frame view is in a valid state.
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, false);

  chrome::ToggleFullscreenMode(browser);
  EXPECT_FALSE(frame_view_test_api.needs_layout());

  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);

  // Showing the toolbar in fullscreen mode should trigger a layout
  // invalidation.
  EXPECT_TRUE(frame_view_test_api.needs_layout());
}

class VerticalTabStripDoubleClickMacTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {};

// Test that double-clicking on the empty area of the vertical tab strip
// zooms the window according to the macOS system preference.
IN_PROC_BROWSER_TEST_F(VerticalTabStripDoubleClickMacTest,
                       DoubleClickOnEmptyAreaZoomsWindow) {
  VerticalTabStripRegionView* view =
      browser()->GetBrowserView().vertical_tab_strip_region_view_for_testing();
  ASSERT_TRUE(view);
  ASSERT_TRUE(view->GetVisible());

  // Pick a point in the empty space below all tabs. The vertical center of
  // the bottom half of the region view should be well below the last tab.
  const gfx::Rect bounds = view->GetLocalBounds();
  gfx::Point caption_point = bounds.bottom_center();
  caption_point.Offset(0, -bounds.height() / 4);
  ASSERT_TRUE(view->IsPositionInWindowCaption(caption_point));

  // Convert to widget (window) coordinates.
  views::View::ConvertPointToWidget(view, &caption_point);

  // Override the system preference for the test and ensure it's restored.
  NSString* const kPrefKey = @"AppleActionOnDoubleClick";
  NSString* original_action =
      [[NSUserDefaults standardUserDefaults] stringForKey:kPrefKey];
  [[NSUserDefaults standardUserDefaults] setObject:@"Maximize" forKey:kPrefKey];
  // Should be restored at the end of the test, even if it fails.
  base::ScopedClosureRunner restore_pref(base::BindOnce(^{
    if (original_action) {
      [[NSUserDefaults standardUserDefaults] setObject:original_action
                                                forKey:kPrefKey];
    } else {
      [[NSUserDefaults standardUserDefaults] removeObjectForKey:kPrefKey];
    }
  }));

  NSWindow* ns_window =
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  ASSERT_TRUE(ns_window);

  // Widget coordinates use top-left origin; NSWindow coordinates use
  // bottom-left origin. Since Chromium uses NSFullSizeContentViewWindowMask,
  // the content view fills the entire window frame.
  NSPoint ns_point =
      NSMakePoint(caption_point.x(),
                  NSHeight([ns_window.contentView frame]) - caption_point.y());

  EXPECT_FALSE([ns_window isZoomed]);

  // Send a double-click (mouseDown + mouseUp with clickCount=2) via
  // cocoa_test_event_utils.
  [ns_window sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                           ns_point, NSEventTypeLeftMouseDown, ns_window, 2)];
  [ns_window sendEvent:cocoa_test_event_utils::MouseEventAtPointInWindow(
                           ns_point, NSEventTypeLeftMouseUp, ns_window, 2)];

  EXPECT_TRUE([ns_window isZoomed]);
}
