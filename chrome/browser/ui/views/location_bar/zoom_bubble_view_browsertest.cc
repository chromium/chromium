// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/extension_builder.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_chromeos.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_test_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_commands_mac.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/views/test/button_test_api.h"
#endif

using ZoomBubbleBrowserTest = InProcessBrowserTest;

namespace {

void ShowInActiveTab(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::USER_GESTURE);
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());
}

}  // namespace

// Test whether the zoom bubble is anchored and whether it is visible when in
// content fullscreen.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, ContentFullscreen) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();

  // The zoom bubble should be anchored when not in fullscreen.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ASSERT_TRUE(ZoomBubbleView::GetZoomBubble());
  const ZoomBubbleView* zoom_bubble = ZoomBubbleView::GetZoomBubble();
  EXPECT_TRUE(zoom_bubble->GetAnchorView());

  // Entering fullscreen should close the bubble. (We enter into tab fullscreen
  // here because tab fullscreen is non-immersive even on Chrome OS.)
  {
    // The fullscreen change notification is sent asynchronously. Wait for the
    // notification before testing the zoom bubble visibility.
    ui_test_utils::FullscreenWaiter waiter(browser(), {.tab_fullscreen = true});
    static_cast<content::WebContentsDelegate*>(browser())
        ->EnterFullscreenModeForTab(web_contents->GetPrimaryMainFrame(), {});
    waiter.Wait();
  }
#if !BUILDFLAG(IS_MAC)
  // The immersive mode controller is enabled in content fullscreen on Mac.
  ASSERT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
#endif
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // The bubble should not be anchored when it is shown in non-immersive
  // fullscreen.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ASSERT_TRUE(ZoomBubbleView::GetZoomBubble());
  zoom_bubble = ZoomBubbleView::GetZoomBubble();
  EXPECT_FALSE(zoom_bubble->GetAnchorView());

  // Exit fullscreen before ending the test for the sake of sanity.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
}

// Immersive fullscreen is either/or on Mac. Base class for tests that only
// apply to non-immersive.
#if BUILDFLAG(IS_MAC)
class ZoomBubbleImmersiveDisabledBrowserTest : public ZoomBubbleBrowserTest {
 public:
  ZoomBubbleImmersiveDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kImmersiveFullscreen);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
#else
using ZoomBubbleImmersiveDisabledBrowserTest = ZoomBubbleBrowserTest;
#endif

// Test whether the zoom bubble is anchored to the same location if the toolbar
// shows in fullscreen. And when the toolbar hides in fullscreen, the zoom
// bubble should close and re-show in a new un-anchored position.
//
// TODO(crbug.com/40727884): Fails on Lacros bots.
// TODO(lgrey): Disable this test for Mac or delete it when immersive is the
// only code path. This was originally added for a Mac bug that is impossible
// to trigger in immersive mode, and is very implementation-coupled.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_AnchorPositionsInFullscreen DISABLED_AnchorPositionsInFullscreen
#else
#define MAYBE_AnchorPositionsInFullscreen AnchorPositionsInFullscreen
#endif
IN_PROC_BROWSER_TEST_F(ZoomBubbleImmersiveDisabledBrowserTest,
                       MAYBE_AnchorPositionsInFullscreen) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();

  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ASSERT_TRUE(ZoomBubbleView::GetZoomBubble());
  ZoomBubbleView* zoom_bubble = ZoomBubbleView::GetZoomBubble();
  ASSERT_TRUE(zoom_bubble);
  // Record the anchor view when not in fullscreen.
  const views::View* org_anchor_view = zoom_bubble->GetAnchorView();

  // Enter into a browser fullscreen mode. This would close the zoom bubble.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  const bool should_show_toolbar = true;
#else
  const bool should_show_toolbar = false;
#endif
  EXPECT_EQ(should_show_toolbar, browser()->window()->IsToolbarVisible());

  // The zoom bubble should be anchored to the same anchor view if the toolbar
  // shows.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  zoom_bubble = ZoomBubbleView::GetZoomBubble();
  ASSERT_TRUE(zoom_bubble);
  if (should_show_toolbar) {
    EXPECT_EQ(org_anchor_view, zoom_bubble->GetAnchorView());
#if BUILDFLAG(IS_MAC)
    const ZoomBubbleView* org_zoom_bubble = zoom_bubble;
    // Hide toolbar.
    chrome::ToggleAlwaysShowToolbarInFullscreen(browser());

    zoom_bubble = ZoomBubbleView::GetZoomBubble();
    EXPECT_EQ(org_zoom_bubble, zoom_bubble);
    EXPECT_EQ(org_anchor_view, zoom_bubble->GetAnchorView());

    views::test::WidgetDestroyedWaiter waiter(zoom_bubble->GetWidget());
    // Press the zoom-in button. This will open a new bubble in an un-anchored
    // position.
    const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(zoom_bubble->zoom_in_button_).NotifyClick(event);
    zoom_bubble = ZoomBubbleView::GetZoomBubble();
    EXPECT_NE(org_zoom_bubble, zoom_bubble);
    EXPECT_FALSE(zoom_bubble->GetAnchorView());

    // Closing the original zoom bubble is asynchronous.
    waiter.Wait();
    EXPECT_TRUE(zoom_bubble->GetWidget());
#endif
  } else {
    EXPECT_FALSE(zoom_bubble->GetAnchorView());
  }

  // Don't leave the browser in fullscreen for subsequent tests.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test whether the zoom bubble is anchored and whether it is visible when in
// immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, ImmersiveFullscreen) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();

  ImmersiveModeController* immersive_controller =
      browser_view->immersive_mode_controller();
  chromeos::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerChromeos*>(immersive_controller)
          ->controller())
      .SetupForTest();

  // Enter immersive fullscreen.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  ASSERT_TRUE(immersive_controller->IsEnabled());
  ASSERT_FALSE(immersive_controller->IsRevealed());

  // The zoom bubble should not be anchored when it is shown in immersive
  // fullscreen and the top-of-window views are not revealed.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ASSERT_TRUE(ZoomBubbleView::GetZoomBubble());
  const ZoomBubbleView* zoom_bubble = ZoomBubbleView::GetZoomBubble();
  EXPECT_FALSE(zoom_bubble->GetAnchorView());

  // An immersive reveal should hide the zoom bubble.
  std::unique_ptr<ImmersiveRevealedLock> immersive_reveal_lock =
      immersive_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO);
  ASSERT_TRUE(immersive_controller->IsRevealed());
  EXPECT_EQ(nullptr, ZoomBubbleView::zoom_bubble_);

  // The zoom bubble should be anchored when it is shown in immersive fullscreen
  // and the top-of-window views are revealed.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  zoom_bubble = ZoomBubbleView::GetZoomBubble();
  ASSERT_TRUE(zoom_bubble);
  EXPECT_TRUE(zoom_bubble->GetAnchorView());

  // The top-of-window views should not hide till the zoom bubble hides. (It
  // would be weird if the view to which the zoom bubble is anchored hid while
  // the zoom bubble was still visible.)
  immersive_reveal_lock.reset();
  EXPECT_TRUE(immersive_controller->IsRevealed());
  ZoomBubbleView::CloseCurrentBubble();
  // The zoom bubble is deleted on a task.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(immersive_controller->IsRevealed());

  // Exit fullscreen before ending the test for the sake of sanity.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that trying to open zoom bubble with stale WebContents is safe.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, NoWebContentsIsSafe) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  // Close the current tab and try opening the zoom bubble with stale
  // |web_contents|.
  chrome::CloseTab(browser());
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
}

// Ensure a tab switch closes the bubble.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, TabSwitchCloses) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK));
  ShowInActiveTab(browser());
  views::test::WidgetDestroyedWaiter close_waiter(
      ZoomBubbleView::GetZoomBubble()->GetWidget());
  chrome::SelectNextTab(browser());
  close_waiter.Wait();
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// Ensure the bubble is dismissed on tab closure and doesn't reference a
// destroyed WebContents.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, DestroyedWebContents) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK));
  ShowInActiveTab(browser());

  ZoomBubbleView* bubble = ZoomBubbleView::GetZoomBubble();
  EXPECT_TRUE(bubble);

  views::test::TestWidgetObserver observer(bubble->GetWidget());
  EXPECT_FALSE(bubble->GetWidget()->IsClosed());

  chrome::CloseTab(browser());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Widget::Close() completes asynchronously, so it's still safe to access
  // |bubble| here, even though GetZoomBubble() returned null.
  EXPECT_FALSE(observer.widget_closed());
  EXPECT_TRUE(bubble->GetWidget()->IsClosed());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.widget_closed());
}

namespace {

class TestZoomRequestClient : public extensions::ExtensionZoomRequestClient {
 public:
  TestZoomRequestClient(scoped_refptr<const extensions::Extension> extension,
                        bool should_suppress_bubble)
      : extensions::ExtensionZoomRequestClient(extension),
        should_suppress_bubble_(should_suppress_bubble) {}

  bool ShouldSuppressBubble() const override { return should_suppress_bubble_; }

 protected:
  ~TestZoomRequestClient() override = default;

 private:
  const bool should_suppress_bubble_;
};

}  // namespace

// Extensions may be allowlisted to not show a bubble when they perform a zoom
// change. However, if a zoom bubble is already showing, zoom changes performed
// by the extension should update the bubble.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest,
                       BubbleSuppressingExtensionRefreshesExistingBubble) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  ASSERT_TRUE(zoom_controller);

  // Extension zoom bubble suppression only happens in manual mode.
  zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_MANUAL);

  ShowInActiveTab(browser());
  const ZoomBubbleView* bubble = ZoomBubbleView::GetZoomBubble();
  ASSERT_TRUE(bubble);

  const double old_zoom_level = zoom_controller->GetZoomLevel();
  const std::u16string old_label = bubble->label_->GetText();

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test").Build();
  scoped_refptr<const TestZoomRequestClient> client =
      base::MakeRefCounted<const TestZoomRequestClient>(extension, true);
  const double new_zoom_level = old_zoom_level + 0.5;
  zoom_controller->SetZoomLevelByClient(new_zoom_level, client);

  ASSERT_EQ(ZoomBubbleView::GetZoomBubble(), bubble);
  const std::u16string new_label = bubble->label_->GetText();

  EXPECT_NE(new_label, old_label);
}

class ZoomBubbleReuseTest : public ZoomBubbleBrowserTest {
 protected:
  // Performs two zoom changes by these respective clients (where nullptr
  // represents a user initiated zoom). Returns true if the zoom change by
  // |client2| reused the bubble from the zoom change by |client1|.
  bool IsBubbleReused(scoped_refptr<const TestZoomRequestClient> client1,
                      scoped_refptr<const TestZoomRequestClient> client2) {
    // This test would be inconclusive for clients that do not create bubbles.
    // See BubbleSuppressingExtensionRefreshesExistingBubble instead.
    DCHECK(!client1 || !client1->ShouldSuppressBubble());
    DCHECK(!client2 || !client2->ShouldSuppressBubble());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    zoom::ZoomController* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents);
    EXPECT_TRUE(zoom_controller);

    const double starting_zoom_level = zoom_controller->GetZoomLevel();
    const double zoom_level1 = starting_zoom_level + 0.5;
    const double zoom_level2 = zoom_level1 + 0.5;

    zoom_controller->SetZoomLevelByClient(zoom_level1, client1);
    const ZoomBubbleView* bubble1 = ZoomBubbleView::GetZoomBubble();
    EXPECT_TRUE(bubble1);
    zoom_controller->SetZoomLevelByClient(zoom_level2, client2);
    const ZoomBubbleView* bubble2 = ZoomBubbleView::GetZoomBubble();
    EXPECT_TRUE(bubble2);

    return bubble1 == bubble2;
  }

  void SetUpOnMainThread() override {
    extension1_ = extensions::ExtensionBuilder("Test1").Build();
    client1_ =
        base::MakeRefCounted<const TestZoomRequestClient>(extension1_, false);
    extension2_ = extensions::ExtensionBuilder("Test2").Build();
    client2_ =
        base::MakeRefCounted<const TestZoomRequestClient>(extension2_, false);
  }

  scoped_refptr<const extensions::Extension> extension1_;
  scoped_refptr<const TestZoomRequestClient> client1_;
  scoped_refptr<const extensions::Extension> extension2_;
  scoped_refptr<const TestZoomRequestClient> client2_;
};

IN_PROC_BROWSER_TEST_F(ZoomBubbleReuseTest, BothUserInitiated) {
  EXPECT_TRUE(IsBubbleReused(nullptr, nullptr));
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleReuseTest, SameExtension) {
  EXPECT_TRUE(IsBubbleReused(client1_, client1_));
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleReuseTest, DifferentExtension) {
  EXPECT_FALSE(IsBubbleReused(client1_, client2_));
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleReuseTest, ExtensionThenUser) {
  EXPECT_FALSE(IsBubbleReused(client1_, nullptr));
}

IN_PROC_BROWSER_TEST_F(ZoomBubbleReuseTest, UserThenExtension) {
  EXPECT_FALSE(IsBubbleReused(nullptr, client1_));
}

class ZoomBubbleDialogTest : public DialogBrowserTest {
 public:
  ZoomBubbleDialogTest() {}

  ZoomBubbleDialogTest(const ZoomBubbleDialogTest&) = delete;
  ZoomBubbleDialogTest& operator=(const ZoomBubbleDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override { ShowInActiveTab(browser()); }
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(ZoomBubbleDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// If a key event causes the zoom bubble to gain focus, it shouldn't close
// automatically. This allows keyboard-only users to interact with the bubble.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, FocusPreventsClose) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ZoomBubbleView* bubble = ZoomBubbleView::GetZoomBubble();
  ASSERT_TRUE(bubble);
  // |auto_close_timer_| is running so that the bubble is closed at the end.
  EXPECT_TRUE(bubble->auto_close_timer_.IsRunning());

  views::FocusManager* focus_manager = bubble->GetFocusManager();
  // The bubble must have an associated Widget from which to get a FocusManager.
  ASSERT_TRUE(focus_manager);

  // Focus is usually gained via a key combination like alt+shift+a. The test
  // simulates this by focusing the bubble and then sending an empty KeyEvent.
  focus_manager->SetFocusedView(bubble->reset_button_);
  bubble->OnKeyEvent(nullptr);
  // |auto_close_timer_| should not be running since focus should prevent the
  // bubble from closing.
  EXPECT_FALSE(bubble->auto_close_timer_.IsRunning());
}
