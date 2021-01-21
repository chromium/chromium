// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/ax_event_counter.h"

#if defined(OS_MAC)
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/test/base/interactive_test_utils.h"
#endif

using views::FocusManager;

namespace {

class BrowserViewTest : public InProcessBrowserTest {
 public:
  BrowserViewTest() : ax_observer_(views::AXEventManager::Get()) {}
  ~BrowserViewTest() override = default;
  BrowserViewTest(const BrowserViewTest&) = delete;
  BrowserViewTest& operator=(const BrowserViewTest&) = delete;

  void SetUpOnMainThread() override {
#if defined(OS_MAC)
    // Set the preference to true so we expect to see the top view in
    // fullscreen mode.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);

    // Ensure that the browser window is activated. BrowserView::Show calls
    // into BridgedNativeWidgetImpl::SetVisibilityState and makeKeyAndOrderFront
    // there somehow does not change the window's key status on bot.
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#endif
  }

 protected:
  views::test::AXEventCounter ax_observer_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserViewTest, FullscreenClearsFocus) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  FocusManager* focus_manager = browser_view->GetFocusManager();

  // Focus starts in the location bar or one of its children.
  EXPECT_TRUE(location_bar_view->Contains(focus_manager->GetFocusedView()));

  // Enter into fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Focus is released from the location bar.
  EXPECT_FALSE(location_bar_view->Contains(focus_manager->GetFocusedView()));
}

// Test whether the top view including toolbar and tab strip shows up or hides
// correctly in browser fullscreen mode.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, BrowserFullscreenShowTopView) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // The top view should always show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsTabStripVisible());

  // Enter into fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  bool top_view_in_browser_fullscreen = false;
#if defined(OS_MAC)
  // The top view should show up by default.
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Return back to normal mode and toggle to not show the top view in full
  // screen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_view->IsFullscreen());
  chrome::ToggleFullscreenToolbar(browser());

  // While back to fullscreen mode, the top view no longer shows up.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(browser_view->IsTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be disabled.
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Test toggling toolbar while being in fullscreen mode.
  chrome::ToggleFullscreenToolbar(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());
  top_view_in_browser_fullscreen = true;
#else
  // In immersive fullscreen mode, the top view should show up; otherwise, it
  // always hides.
  if (browser_view->immersive_mode_controller()->IsEnabled())
    top_view_in_browser_fullscreen = true;
#endif
  EXPECT_EQ(top_view_in_browser_fullscreen, browser_view->IsTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be enabled if top view is shown.
  EXPECT_EQ(top_view_in_browser_fullscreen,
            chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Enter into tab fullscreen mode from browser fullscreen mode.
  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  controller->EnterFullscreenModeForTab(web_contents->GetMainFrame());
  EXPECT_TRUE(browser_view->IsFullscreen());
  bool top_view_in_tab_fullscreen =
      browser_view->immersive_mode_controller()->IsEnabled() ? true : false;
  EXPECT_EQ(top_view_in_tab_fullscreen, browser_view->IsTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be disabled in tab fullscreen mode.
  EXPECT_EQ(top_view_in_tab_fullscreen,
            chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Return back to browser fullscreen mode.
  content::NativeWebKeyboardEvent event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  event.windows_key_code = ui::VKEY_ESCAPE;
  browser()->exclusive_access_manager()->HandleUserKeyEvent(event);
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_EQ(top_view_in_browser_fullscreen, browser_view->IsTabStripVisible());
  // This makes sure that the layout was updated accordingly.
  EXPECT_EQ(top_view_in_browser_fullscreen,
            browser_view->tabstrip()->GetVisible());
  EXPECT_EQ(top_view_in_browser_fullscreen,
            chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Return to regular mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
}

// Test whether the top view including toolbar and tab strip appears or hides
// correctly in tab fullscreen mode.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, TabFullscreenShowTopView) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // The top view should always show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsTabStripVisible());

  // Enter into tab fullscreen mode.
  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  controller->EnterFullscreenModeForTab(web_contents->GetMainFrame());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // The top view should not show up.
  EXPECT_FALSE(browser_view->IsTabStripVisible());

  // After exiting the fullscreen mode, the top view should show up again.
  controller->ExitFullscreenModeForTab(web_contents);
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
}

// Test whether bookmark bar shows up or hides correctly for fullscreen modes.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, FullscreenShowBookmarkBar) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // If the bookmark bar is not showing, enable showing it so that we can check
  // its state.
  if (!browser_view->IsBookmarkBarVisible())
    chrome::ToggleBookmarkBar(browser());
#if defined(OS_MAC)
  // Disable showing toolbar in fullscreen mode to make its bahavior similar to
  // other platforms.
  chrome::ToggleFullscreenToolbar(browser());
#endif
  AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);

  // Now the bookmark bar should show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());

  // Enter into fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());
  if (browser_view->immersive_mode_controller()->IsEnabled())
    EXPECT_TRUE(browser_view->IsBookmarkBarVisible());
  else
    EXPECT_FALSE(browser_view->IsBookmarkBarVisible());

#if defined(OS_MAC)
  // Test toggling toolbar state in fullscreen mode would also affect bookmark
  // bar state.
  chrome::ToggleFullscreenToolbar(browser());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());

  chrome::ToggleFullscreenToolbar(browser());
  EXPECT_FALSE(browser_view->IsTabStripVisible());
  EXPECT_FALSE(browser_view->IsBookmarkBarVisible());
#endif

  // Exit from fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsTabStripVisible());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());
}

// TODO(crbug.com/897177): Only Aura platforms use the WindowActivated
// accessibility event. We need to harmonize the firing of accessibility events
// between platforms.
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
IN_PROC_BROWSER_TEST_F(BrowserViewTest, WindowActivatedAccessibleEvent) {
  // Wait for window activated event from the first browser window.
  // This event is asynchronous, it is emitted as a response to a system window
  // event. It is possible that we haven't received it yet when we run this test
  // and we need to explicitly wait for it.
  if (ax_observer_.GetCount(ax::mojom::Event::kWindowActivated) == 0)
    ax_observer_.WaitForEvent(ax::mojom::Event::kWindowActivated);
  ASSERT_EQ(1, ax_observer_.GetCount(ax::mojom::Event::kWindowActivated));

  // Create a new browser window and wait for event again.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  if (ax_observer_.GetCount(ax::mojom::Event::kWindowActivated) == 1)
    ax_observer_.WaitForEvent(ax::mojom::Event::kWindowActivated);
  ASSERT_EQ(2, ax_observer_.GetCount(ax::mojom::Event::kWindowActivated));
}
#endif
