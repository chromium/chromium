// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/extension_builder.h"
#include "ui/views/test/test_widget_observer.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
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
// non-immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, NonImmersiveFullscreen) {
#if defined(OS_MACOSX)
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
    FullscreenNotificationObserver waiter(browser());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    waiter.Wait();
  }
  ASSERT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // The bubble should not be anchored when it is shown in non-immersive
  // fullscreen.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ASSERT_TRUE(ZoomBubbleView::GetZoomBubble());
  zoom_bubble = ZoomBubbleView::GetZoomBubble();
  EXPECT_FALSE(zoom_bubble->GetAnchorView());

  // Exit fullscreen before ending the test for the sake of sanity.
  {
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
  }
}

#if defined(OS_CHROMEOS)
// Test whether the zoom bubble is anchored and whether it is visible when in
// immersive fullscreen.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, ImmersiveFullscreen) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();

  ImmersiveModeController* immersive_controller =
      browser_view->immersive_mode_controller();
  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(immersive_controller)
          ->controller())
      .SetupForTest();

  // Enter immersive fullscreen.
  {
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
  }
  ASSERT_TRUE(immersive_controller->IsEnabled());
  ASSERT_FALSE(immersive_controller->IsRevealed());

  // The zoom bubble should not be anchored when it is shown in immersive
  // fullscreen and the top-of-window views are not revealed.
  ZoomBubbleView::ShowBubble(web_contents, ZoomBubbleView::AUTOMATIC);
  ASSERT_TRUE(ZoomBubbleView::GetZoomBubble());
  const ZoomBubbleView* zoom_bubble = ZoomBubbleView::GetZoomBubble();
  EXPECT_FALSE(zoom_bubble->GetAnchorView());

  // An immersive reveal should hide the zoom bubble.
  std::unique_ptr<ImmersiveRevealedLock> immersive_reveal_lock(
      immersive_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  ASSERT_TRUE(immersive_controller->IsRevealed());
  EXPECT_EQ(NULL, ZoomBubbleView::zoom_bubble_);

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
  {
    FullscreenNotificationObserver waiter(browser());
    chrome::ToggleFullscreenMode(browser());
    waiter.Wait();
  }
}
#endif  // OS_CHROMEOS

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
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK);
  ShowInActiveTab(browser());
  chrome::SelectNextTab(browser());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// Ensure the bubble is dismissed on tab closure and doesn't reference a
// destroyed WebContents.
IN_PROC_BROWSER_TEST_F(ZoomBubbleBrowserTest, DestroyedWebContents) {
  AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_LINK);
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

// Extensions may be whitelisted to not show a bubble when they perform a zoom
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
  const base::string16 old_label = bubble->label_->GetText();

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test").Build();
  scoped_refptr<const TestZoomRequestClient> client =
      base::MakeRefCounted<const TestZoomRequestClient>(extension, true);
  const double new_zoom_level = old_zoom_level + 0.5;
  zoom_controller->SetZoomLevelByClient(new_zoom_level, client);

  ASSERT_EQ(ZoomBubbleView::GetZoomBubble(), bubble);
  const base::string16 new_label = bubble->label_->GetText();

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

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override { ShowInActiveTab(browser()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleDialogTest);
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
