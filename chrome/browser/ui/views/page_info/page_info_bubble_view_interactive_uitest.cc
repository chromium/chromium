// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/test_event.h"

namespace {

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ui::test::TestEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Tracks focus of an arbitrary UI element.
class FocusTracker {
 public:
  FocusTracker(const FocusTracker&) = delete;
  FocusTracker& operator=(const FocusTracker&) = delete;

  bool focused() const { return focused_; }

  // Wait for focused() to be in state |target_state_is_focused|. If focused()
  // is already in the desired state, returns immediately, otherwise waits until
  // it is.
  void WaitForFocus(bool target_state_is_focused) {
    if (focused_ == target_state_is_focused)
      return;
    target_state_is_focused_ = target_state_is_focused;
    run_loop_.Run();
  }

 protected:
  explicit FocusTracker(bool initially_focused) : focused_(initially_focused) {}
  virtual ~FocusTracker() = default;

  void OnFocused() {
    focused_ = true;
    if (run_loop_.running() && target_state_is_focused_ == focused_)
      run_loop_.Quit();
  }

  void OnBlurred() {
    focused_ = false;
    if (run_loop_.running() && target_state_is_focused_ == focused_)
      run_loop_.Quit();
  }

 private:
  // Whether the tracked visual element is currently focused.
  bool focused_ = false;

  // Desired state when waiting for focus to change.
  bool target_state_is_focused_;

  base::RunLoop run_loop_;
};

// Watches a WebContents for focus changes.
class WebContentsFocusTracker : public FocusTracker,
                                public content::WebContentsObserver {
 public:
  explicit WebContentsFocusTracker(content::WebContents* web_contents)
      : FocusTracker(IsWebContentsFocused(web_contents)),
        WebContentsObserver(web_contents) {}

  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override {
    OnFocused();
  }

  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override {
    OnBlurred();
  }

 private:
  static bool IsWebContentsFocused(content::WebContents* web_contents) {
    Browser* const browser = chrome::FindBrowserWithTab(web_contents);
    if (!browser)
      return false;
    if (browser->tab_strip_model()->GetActiveWebContents() != web_contents)
      return false;
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->contents_web_view()
        ->HasFocus();
  }
};

// Watches a View for focus changes.
class ViewFocusTracker : public FocusTracker, public views::ViewObserver {
 public:
  explicit ViewFocusTracker(views::View* view)
      : FocusTracker(view->HasFocus()) {
    scoped_observation_.Observe(view);
  }

  void OnViewFocused(views::View* observed_view) override { OnFocused(); }

  void OnViewBlurred(views::View* observed_view) override { OnBlurred(); }

 private:
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
};

}  // namespace

class PageInfoBubbleViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  PageInfoBubbleViewInteractiveUiTest() = default;
  PageInfoBubbleViewInteractiveUiTest(
      const PageInfoBubbleViewInteractiveUiTest& test) = delete;
  PageInfoBubbleViewInteractiveUiTest& operator=(
      const PageInfoBubbleViewInteractiveUiTest& test) = delete;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void TriggerReloadPromptOnClose() const {
    // Set some dummy non-default permissions. This will trigger a reload prompt
    // when the bubble is closed.
    PageInfo::PermissionInfo permission;
    permission.type = ContentSettingsType::NOTIFICATIONS;
    permission.setting = ContentSetting::CONTENT_SETTING_BLOCK;
    permission.default_setting = ContentSetting::CONTENT_SETTING_ASK;
    permission.source = content_settings::SettingSource::SETTING_SOURCE_USER;

    PageInfo* presenter = static_cast<PageInfoBubbleView*>(
                              PageInfoBubbleView::GetPageInfoBubbleForTesting())
                              ->presenter_for_testing();
    presenter->OnSitePermissionChanged(permission.type, permission.setting,
                                       permission.requesting_origin,
                                       permission.is_one_time);
  }
};

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1029882
#define MAYBE_FocusReturnsToContentOnClose DISABLED_FocusReturnsToContentOnClose
#else
#define MAYBE_FocusReturnsToContentOnClose FocusReturnsToContentOnClose
#endif

// Test that when the PageInfo bubble is closed, focus is returned to the web
// contents pane.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewInteractiveUiTest,
                       MAYBE_FocusReturnsToContentOnClose) {
  WebContentsFocusTracker web_contents_focus_tracker(web_contents());
  web_contents()->Focus();
  web_contents_focus_tracker.WaitForFocus(true);

  OpenPageInfoBubble(browser());
  base::RunLoop().RunUntilIdle();

  auto* page_info_bubble_view =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_FALSE(web_contents_focus_tracker.focused());

  page_info_bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  web_contents_focus_tracker.WaitForFocus(true);
  EXPECT_TRUE(web_contents_focus_tracker.focused());
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1029882
#define MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt \
  DISABLED_FocusDoesNotReturnToContentsOnReloadPrompt
#else
#define MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt \
  FocusDoesNotReturnToContentsOnReloadPrompt
#endif

// Test that when the PageInfo bubble is closed and a reload prompt is
// displayed, focus is NOT returned to the web contents pane, but rather returns
// to the location bar so accessibility users must tab through the reload prompt
// before getting back to web contents (see https://crbug.com/910067).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewInteractiveUiTest,
                       MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt) {
  WebContentsFocusTracker web_contents_focus_tracker(web_contents());
  ViewFocusTracker location_bar_focus_tracker(
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView());
  web_contents()->Focus();
  web_contents_focus_tracker.WaitForFocus(true);

  OpenPageInfoBubble(browser());
  base::RunLoop().RunUntilIdle();

  auto* page_info_bubble_view =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_FALSE(web_contents_focus_tracker.focused());

  TriggerReloadPromptOnClose();
  page_info_bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  location_bar_focus_tracker.WaitForFocus(true);
  web_contents_focus_tracker.WaitForFocus(false);
  EXPECT_TRUE(location_bar_focus_tracker.focused());
  EXPECT_FALSE(web_contents_focus_tracker.focused());
}
