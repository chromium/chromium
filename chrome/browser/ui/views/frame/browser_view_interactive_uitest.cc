// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/buildflags.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/test/base/interactive_test_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/wm/window_pin_util.h"
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
    chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), true);
    // Ensure that the browser window is activated. BrowserView::Show calls
    // into BridgedNativeWidgetImpl::SetVisibilityState and makeKeyAndOrderFront
    // there somehow does not change the window's key status on bot.
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#endif
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_MAC)
    chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), true);
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
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
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

  // Enter into fullscreen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  bool top_view_in_browser_fullscreen = false;
#if BUILDFLAG(IS_MAC)
  // The top view should show up by default.
  EXPECT_TRUE(browser_view->GetTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Return back to normal mode and toggle to not show the top view in full
  // screen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(browser_view->IsFullscreen());
  // Disable 'Always Show Toolbar in Full Screen'.
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);

  // While back to fullscreen mode, the top view no longer shows up.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(browser_view->GetTabStripVisible());
  // In non-immersive mode, the bookmark visibility cannot be changed because
  // the toolbar is invisible. In immersive mode, the bookmark visibility should
  // be able to change because the toolbar cannot be permanently hidden.
  EXPECT_EQ(chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR),
            base::FeatureList::IsEnabled(features::kImmersiveFullscreen));

  if (browser_view->immersive_mode_controller()->IsEnabled()) {
    // Move mouse to the upper border of the browser window and the toolbar
    // should become visible.
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        browser_view->GetBoundsInScreen().top_center(),
        browser_view->GetWidget()->GetNativeWindow()));
    views::test::PropertyWaiter(
        base::BindRepeating(&BrowserView::GetTabStripVisible,
                            base::Unretained(browser_view)),
        true)
        .Wait();
    EXPECT_TRUE(browser_view->GetTabStripVisible());
  }

  // Test toggling toolbar while being in fullscreen mode.
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), true);
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
      browser_view->immersive_mode_controller()->IsEnabled();
  EXPECT_EQ(top_view_in_tab_fullscreen, browser_view->GetTabStripVisible());
  // The 'Always Show Bookmarks Bar' should be disabled in tab fullscreen mode.
  EXPECT_EQ(top_view_in_tab_fullscreen,
            chrome::IsCommandEnabled(browser(), IDC_SHOW_BOOKMARK_BAR));

  // Return back to browser fullscreen mode.
  input::NativeWebKeyboardEvent event(
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

  // Return to regular mode.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
#else
  // Adding `FullscreenWaiter` will make the TESTs on Lacros fail
  // determinately, which should have been a no-op.
  // TODO(crbug.com/40857465): Repair this defect.
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
  EXPECT_TRUE(views::test::PropertyWaiter(
                  base::BindRepeating(&BrowserView::GetTabStripVisible,
                                      base::Unretained(browser_view)),
                  false)
                  .Wait());

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
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);
#endif
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // Now the bookmark bar should show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());

  // Enter into fullscreen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Move to the center of the window so that the toolbar becomes hidden in
  // immersive mode.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      browser_view->GetBoundsInScreen().CenterPoint(),
      browser_view->GetWidget()->GetNativeWindow()));
  views::test::PropertyWaiter(
      base::BindRepeating(&BrowserView::IsBookmarkBarVisible,
                          base::Unretained(browser_view)),
      false)
      .Wait();
  EXPECT_FALSE(browser_view->GetTabStripVisible());
  EXPECT_FALSE(browser_view->IsBookmarkBarVisible());

#if BUILDFLAG(IS_MAC)
  // Test toggling toolbar state in fullscreen mode would also affect bookmark
  // bar state.
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), true);
  EXPECT_TRUE(browser_view->GetTabStripVisible());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());

  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);
  EXPECT_FALSE(browser_view->GetTabStripVisible());
  EXPECT_FALSE(browser_view->IsBookmarkBarVisible());
#endif

  // Exit from fullscreen mode.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
#else
  // Adding `FullscreenWaiter` will make the TESTs on Lacros fail
  // determinately, which should have been a no-op.
  // TODO(crbug.com/40857465): Repair this defect.
  chrome::ToggleFullscreenMode(browser());
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->GetTabStripVisible());
  EXPECT_TRUE(browser_view->IsBookmarkBarVisible());
}

// TODO(crbug.com/40598906): Only Aura platforms use the WindowActivated
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

class BrowserViewFullscreenTest : public BrowserViewTest {
 public:
  BrowserViewFullscreenTest() {
    feature_list_.InitAndEnableFeature(features::kAsyncFullscreenWindowState);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Disabled on platforms where async fullscreen state transition is not
// yet supported.
// TODO(b/40276379): Apply this to all remaining desktop platforms.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_Fullscreen Fullscreen
#else
#define MAYBE_Fullscreen DISABLED_Fullscreen
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewFullscreenTest, MAYBE_Fullscreen) {
#if BUILDFLAG(IS_LINUX)
  // Skip non wayland cases, now, such as X11, where the fullscreen
  // call async support is not yet completed.
  if (ui::OzonePlatform::GetPlatformNameForTest() != "wayland") {
    GTEST_SKIP();
  }
#endif

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // The top view should always show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());

  // Enter into fullscreen mode.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.browser_fullscreen = true});
    chrome::ToggleFullscreenMode(browser());
    // The state won't change immediately.
    EXPECT_FALSE(browser_view->IsFullscreen());
    waiter.Wait();
    EXPECT_TRUE(browser_view->IsFullscreen());
  }

  // Exit from fullscreen mode.
  {
    ui_test_utils::FullscreenWaiter waiter(browser(),
                                           {.browser_fullscreen = false});
    chrome::ToggleFullscreenMode(browser());
    // The state won't change immediately.
    EXPECT_TRUE(browser_view->IsFullscreen());
    waiter.Wait();
    EXPECT_FALSE(browser_view->IsFullscreen());
  }
}

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

// TODO(b/326134178): Disable the flaky test on branded Lacros builder
// (ci/linux-lacros-chrome).
// TODO(crbug.com/41484767): Disable flaky test on Lacros.
// TODO(b/342017720): Re-enable on Mac
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
#define MAYBE_LoadingAnimationChangeOnMinimizeAndRestore \
  DISABLED_LoadingAnimationChangeOnMinimizeAndRestore
#else
#define MAYBE_LoadingAnimationChangeOnMinimizeAndRestore \
  LoadingAnimationChangeOnMinimizeAndRestore
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(BrowserViewTestWithStopLoadingAnimationForHiddenWindow,
                       MAYBE_LoadingAnimationChangeOnMinimizeAndRestore) {
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
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
using BrowserViewLockedFullscreenTestChromeOS = BrowserViewTest;

IN_PROC_BROWSER_TEST_F(BrowserViewLockedFullscreenTestChromeOS,
                       ShowExclusiveAccessBubbleWhenNotLocked) {
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/false);
  browser()->exclusive_access_manager()->context()->UpdateExclusiveAccessBubble(
      {
          .url = GURL(
              "http://www.example.com"),  // Should be non-empty to show bubble
          .type = ExclusiveAccessBubbleType::
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
          .force_update = true,
      },
      base::NullCallback());
  EXPECT_TRUE(browser_view()->IsExclusiveAccessBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(BrowserViewLockedFullscreenTestChromeOS,
                       HideExclusiveAccessBubbleWhenLocked) {
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  browser()->exclusive_access_manager()->context()->UpdateExclusiveAccessBubble(
      {.url = GURL(
           "http://www.example.com"),  // Should be non-empty to show bubble
       .type = ExclusiveAccessBubbleType::
           EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
       .force_update = true},
      base::NullCallback());
  EXPECT_FALSE(browser_view()->IsExclusiveAccessBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(BrowserViewLockedFullscreenTestChromeOS,
                       EnableImmersiveModeWhenNotTrustedPinned) {
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/false);
  EXPECT_TRUE(browser_view()->immersive_mode_controller()->IsEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserViewLockedFullscreenTestChromeOS,
                       DisableImmersiveModeWhenNotLockedForOnTask) {
  browser()->SetLockedForOnTask(false);
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  EXPECT_FALSE(browser_view()->immersive_mode_controller()->IsEnabled());
}

IN_PROC_BROWSER_TEST_F(BrowserViewLockedFullscreenTestChromeOS,
                       EnableImmersiveModeWhenLockedForOnTask) {
  browser()->SetLockedForOnTask(true);
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  EXPECT_TRUE(browser_view()->immersive_mode_controller()->IsEnabled());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
