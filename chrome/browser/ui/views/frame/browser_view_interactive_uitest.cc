// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/ax_event_counter.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/profiles/profile.h"
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

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_MAC)
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

  FullscreenNotificationObserver fullscreen_observer(browser());
  // Enter into fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  fullscreen_observer.Wait();
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
  EXPECT_TRUE(browser_view->GetTabStripVisible());

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    // Enter into fullscreen mode.
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
  }
  EXPECT_TRUE(browser_view->IsFullscreen());

  bool top_view_in_browser_fullscreen = false;
#if BUILDFLAG(IS_MAC)
  // The top view should show up by default.
  EXPECT_TRUE(browser_view->GetTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    // Return back to normal mode and toggle to not show the top view in full
    // screen mode.
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
  }
  EXPECT_FALSE(browser_view->IsFullscreen());
  chrome::ToggleFullscreenToolbar(browser());

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    // While back to fullscreen mode, the top view no longer shows up.
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
  }
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(browser_view->GetTabStripVisible());
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
  EXPECT_EQ(top_view_in_browser_fullscreen, browser_view->GetTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be enabled if top view is shown.
  EXPECT_EQ(top_view_in_browser_fullscreen,
            chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Enter into tab fullscreen mode from browser fullscreen mode.
  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  controller->EnterFullscreenModeForTab(web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(browser_view->IsFullscreen());
  bool top_view_in_tab_fullscreen =
      browser_view->immersive_mode_controller()->IsEnabled() ? true : false;
  EXPECT_EQ(top_view_in_tab_fullscreen, browser_view->GetTabStripVisible());
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
  EXPECT_EQ(top_view_in_browser_fullscreen, browser_view->GetTabStripVisible());
  // This makes sure that the layout was updated accordingly.
  EXPECT_EQ(top_view_in_browser_fullscreen,
            browser_view->tabstrip()->GetVisible());
  EXPECT_EQ(top_view_in_browser_fullscreen,
            chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

// Adding `FullscreenNotificationObserver` will make the TESTs on Lacros fail
// determinately, which should have been a no-op.
// TODO(crbug.com/1351971): Repair this defect.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    // Return to regular mode.
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
  }
#else
  // Return to regular mode.
  chrome::ToggleFullscreenMode(browser());
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->GetTabStripVisible());
}

// Test whether the top view including toolbar and tab strip appears or hides
// correctly in tab fullscreen mode.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, TabFullscreenShowTopView) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // The top view should always show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->GetTabStripVisible());

  // Enter into tab fullscreen mode.
  FullscreenController* controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  controller->EnterFullscreenModeForTab(web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // The top view should not show up.
  EXPECT_FALSE(browser_view->GetTabStripVisible());

  // After exiting the fullscreen mode, the top view should show up again.
  controller->ExitFullscreenModeForTab(web_contents);
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->GetTabStripVisible());
}

// Test whether bookmark bar shows up or hides correctly for fullscreen modes.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_FullscreenShowBookmarkBar DISABLED_FullscreenShowBookmarkBar
#else
#define MAYBE_FullscreenShowBookmarkBar FullscreenShowBookmarkBar
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_FullscreenShowBookmarkBar) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // If the bookmark bar is not showing, enable showing it so that we can check
  // its state.
  if (!browser_view->IsBookmarkBarVisible())
    chrome::ToggleBookmarkBar(browser());
#if BUILDFLAG(IS_MAC)
  // Disable showing toolbar in fullscreen mode to make its behavior similar to
  // other platforms.
  chrome::ToggleFullscreenToolbar(browser());
#endif
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // Now the bookmark bar should show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());

  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    // Enter into fullscreen mode.
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
  }
  EXPECT_TRUE(browser_view->IsFullscreen());
  if (browser_view->immersive_mode_controller()->IsEnabled())
    EXPECT_TRUE(browser_view->IsBookmarkBarVisible());
  else
    EXPECT_FALSE(browser_view->IsBookmarkBarVisible());

#if BUILDFLAG(IS_MAC)
  // Test toggling toolbar state in fullscreen mode would also affect bookmark
  // bar state.
  chrome::ToggleFullscreenToolbar(browser());
  EXPECT_TRUE(browser_view->GetTabStripVisible());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());

  chrome::ToggleFullscreenToolbar(browser());
  EXPECT_FALSE(browser_view->GetTabStripVisible());
  EXPECT_FALSE(browser_view->IsBookmarkBarVisible());
#endif

// Adding `FullscreenNotificationObserver` will make the TESTs on Lacros fail
// determinately, which should have been a no-op.
// TODO(crbug.com/1351971): Repair this defect.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  {
    FullscreenNotificationObserver fullscreen_observer(browser());
    // Exit from fullscreen mode.
    chrome::ToggleFullscreenMode(browser());
    fullscreen_observer.Wait();
  }
#else
  // Exit from fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->GetTabStripVisible());
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

// Class for BrowserView unit tests for the loading animation feature.
// Creates a Browser with a |features_list| where
// kStopLoadingAnimationForHiddenWindow is enabled before setting GPU thread.
class BrowserViewTestWithStopLoadingAnimationForHiddenWindow
    : public BrowserViewTest {
 public:
  BrowserViewTestWithStopLoadingAnimationForHiddenWindow() {
    feature_list_.InitAndEnableFeature(
        features::kStopLoadingAnimationForHiddenWindow);
  }

  BrowserViewTestWithStopLoadingAnimationForHiddenWindow(
      const BrowserViewTestWithStopLoadingAnimationForHiddenWindow&) = delete;
  BrowserViewTestWithStopLoadingAnimationForHiddenWindow& operator=(
      const BrowserViewTestWithStopLoadingAnimationForHiddenWindow&) = delete;

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserViewTestWithStopLoadingAnimationForHiddenWindow,
                       LoadingAnimationChangeOnMinimizeAndRestore) {
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver navigation_watcher(
      contents, 1, content::MessageLoopRunner::QuitMode::DEFERRED);

  // Navigate without blocking.
  const GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  contents->GetController().LoadURL(test_url, content::Referrer(),
                                    ui::PAGE_TRANSITION_LINK, std::string());
  {
    base::RunLoop run_loop;
    browser_view()->SetLoadingAnimationStateChangeClosureForTesting(
        run_loop.QuitClosure());

    // Loading animation is not rendered when browser view is minimized.
    browser_view()->Minimize();
    run_loop.Run();
  }

  EXPECT_TRUE(browser()->tab_strip_model()->TabsAreLoading());
  EXPECT_FALSE(browser_view()->IsLoadingAnimationRunningForTesting());

  {
    base::RunLoop run_loop;
    browser_view()->SetLoadingAnimationStateChangeClosureForTesting(
        run_loop.QuitClosure());

    // Loading animation is rendered when browser view is restored.
    browser_view()->Restore();
    run_loop.Run();
  }

  EXPECT_TRUE(browser()->tab_strip_model()->TabsAreLoading());
  EXPECT_TRUE(browser_view()->IsLoadingAnimationRunningForTesting());

  // Now block for the navigation to complete.
  navigation_watcher.Wait();
  EXPECT_FALSE(browser()->tab_strip_model()->TabsAreLoading());
}

// On Mac, voiceover treats tab modal dialogs as native windows, so setting an
// accessible title for tab-modal dialogs is not necessary.
#if !BUILDFLAG(IS_MAC)

namespace {

class TestTabModalConfirmDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  explicit TestTabModalConfirmDialogDelegate(content::WebContents* contents)
      : TabModalConfirmDialogDelegate(contents) {}

  TestTabModalConfirmDialogDelegate(const TestTabModalConfirmDialogDelegate&) =
      delete;
  TestTabModalConfirmDialogDelegate& operator=(
      const TestTabModalConfirmDialogDelegate&) = delete;

  std::u16string GetTitle() override { return std::u16string(u"Dialog Title"); }
  std::u16string GetDialogMessage() override { return std::u16string(); }
};

}  // namespace

// Open a tab-modal dialog and check that the accessible window title is the
// title of the dialog. The accessible window title is based on the focused
// dialog and this dependency on focus is why this is an interactive ui test.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, GetAccessibleTabModalDialogTitle) {
  std::u16string window_title =
      u"about:blank - " + l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  EXPECT_TRUE(base::StartsWith(browser_view()->GetAccessibleWindowTitle(),
                               window_title, base::CompareCase::SENSITIVE));

  content::WebContents* contents = browser_view()->GetActiveWebContents();
  auto delegate = std::make_unique<TestTabModalConfirmDialogDelegate>(contents);
  TestTabModalConfirmDialogDelegate* delegate_observer = delegate.get();
  TabModalConfirmDialog::Create(std::move(delegate), contents);
  EXPECT_EQ(browser_view()->GetAccessibleWindowTitle(),
            delegate_observer->GetTitle());

  delegate_observer->Close();

  EXPECT_TRUE(base::StartsWith(browser_view()->GetAccessibleWindowTitle(),
                               window_title, base::CompareCase::SENSITIVE));
}

#endif
