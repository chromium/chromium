// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/sad_tab.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace test {

// A friend of SadTabView that's able to call RecordFirstPaint.
class SadTabViewTestApi {
 public:
  SadTabViewTestApi(const SadTabViewTestApi&) = delete;
  SadTabViewTestApi& operator=(const SadTabViewTestApi&) = delete;

  static void RecordFirstPaintForTesting(SadTabView* sad_tab_view) {
    if (!sad_tab_view->painted_) {
      sad_tab_view->RecordFirstPaint();
      sad_tab_view->painted_ = true;
    }
  }
};

}  // namespace test

class SadTabViewInteractiveUITest : public InProcessBrowserTest {
 public:
  SadTabViewInteractiveUITest() {}

  SadTabViewInteractiveUITest(const SadTabViewInteractiveUITest&) = delete;
  SadTabViewInteractiveUITest& operator=(const SadTabViewInteractiveUITest&) =
      delete;

 protected:
  void KillRendererForActiveWebContentsSync() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderProcessHost* process =
        web_contents->GetPrimaryMainFrame()->GetProcess();
    content::RenderProcessHostWatcher crash_observer(
        process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(content::RESULT_CODE_KILLED);
    crash_observer.Wait();
  }

  void PressTab() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                                false, false, false));
  }

  void PressShiftTab() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                                true, false, false));
  }

  void PressSpacebar() {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_SPACE,
                                                false, false, false, false));
  }

  views::FocusManager* GetFocusManager() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetWidget()->GetFocusManager();
  }

  views::View* GetFocusedView() { return GetFocusManager()->GetFocusedView(); }

  template <typename T>
  bool IsFocusedViewInsideViewClass() {
    views::View* view = GetFocusedView();
    while (view) {
      if (views::IsViewClass<T>(view)) {
        return true;
      }
      view = view->parent();
    }
    return false;
  }

  bool IsFocusedViewInsideSadTab() {
    return IsFocusedViewInsideViewClass<SadTabView>();
  }

  bool IsFocusedViewInsideBrowserToolbar() {
    return IsFocusedViewInsideViewClass<ToolbarView>();
  }

  bool IsFocusedViewOnActionButtonInSadTab() {
    return IsFocusedViewInsideViewClass<SadTabView>() &&
           IsFocusedViewInsideViewClass<views::MdTextButton>();
  }

  void ClickOnActionButtonInSadTab() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContents* web_contents =
        tab_strip_model->GetActiveWebContents();
    while (!IsFocusedViewOnActionButtonInSadTab())
      PressTab();

    // SadTab has a DCHECK that it's been painted at least once
    // before the action button can be pressed, bypass that.
    SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(web_contents);
    SadTabView* sad_tab_view =
        static_cast<SadTabView*>(sad_tab_helper->sad_tab());
    test::SadTabViewTestApi::RecordFirstPaintForTesting(sad_tab_view);
    PressSpacebar();
  }
};

#if BUILDFLAG(IS_MAC)
// Focusing or input is not completely working on Mac: http://crbug.com/824418
#define MAYBE_SadTabKeyboardAccessibility DISABLED_SadTabKeyboardAccessibility
#else
#define MAYBE_SadTabKeyboardAccessibility SadTabKeyboardAccessibility
#endif
IN_PROC_BROWSER_TEST_F(SadTabViewInteractiveUITest,
                       MAYBE_SadTabKeyboardAccessibility) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/links.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Start with focus in the location bar.
  chrome::FocusLocationBar(browser());
  ASSERT_FALSE(IsFocusedViewInsideSadTab());
  ASSERT_TRUE(IsFocusedViewInsideBrowserToolbar());

  // Kill the renderer process, resulting in a sad tab.
  KillRendererForActiveWebContentsSync();

  // Focus should now be on a MdText button inside the sad tab.
  ASSERT_TRUE(views::IsViewClass<views::MdTextButton>(GetFocusedView()));
  ASSERT_TRUE(IsFocusedViewInsideSadTab());
  ASSERT_FALSE(IsFocusedViewInsideBrowserToolbar());

  // Pressing the Tab key should cycle focus back to the toolbar or the browser
  // frame if the tab search caption button is enabled.
  PressTab();
  ASSERT_FALSE(IsFocusedViewInsideSadTab());
  ASSERT_TRUE(IsFocusedViewInsideBrowserToolbar());

  // Keep pressing the Tab key and make sure we make it back to the sad tab.
  while (!IsFocusedViewInsideSadTab())
    PressTab();
  ASSERT_FALSE(IsFocusedViewInsideBrowserToolbar());

  // Press Shift-Tab and ensure we end up back in the toolbar.
  PressShiftTab();
  ASSERT_FALSE(IsFocusedViewInsideSadTab());
  ASSERT_TRUE(IsFocusedViewInsideBrowserToolbar());
}

// TODO(crbug.com/40752417): flaky test.
IN_PROC_BROWSER_TEST_F(SadTabViewInteractiveUITest,
                       DISABLED_ReloadMultipleSadTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/links.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Kill the renderer process, resulting in a sad tab.
  KillRendererForActiveWebContentsSync();

  // Create a second tab, navigate to a second url.
  chrome::NewTab(browser());
  GURL url2(embedded_test_server()->GetURL("/simple.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // Kill that one too.
  KillRendererForActiveWebContentsSync();

  // Switch back to the first tab.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->active_index());
  tab_strip_model->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(0, tab_strip_model->active_index());
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  EXPECT_TRUE(web_contents->IsCrashed());

  ClickOnActionButtonInSadTab();

  // Ensure the first WebContents reloads.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_FALSE(web_contents->IsCrashed());

  // Switch to the second tab, reload it too.
  tab_strip_model->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  web_contents = tab_strip_model->GetActiveWebContents();
  EXPECT_TRUE(web_contents->IsCrashed());
  ClickOnActionButtonInSadTab();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_FALSE(web_contents->IsCrashed());
}
