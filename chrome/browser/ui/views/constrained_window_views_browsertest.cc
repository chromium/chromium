// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tab_modal_confirm_dialog_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace {

class TestDialog : public views::DialogDelegateView {
 public:
  TestDialog() {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetModalType(ui::MODAL_TYPE_CHILD);
    // Dialogs that take focus must have a name and role to pass accessibility
    // checks.
    GetViewAccessibility().OverrideRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().OverrideName("Test dialog");
  }

  TestDialog(const TestDialog&) = delete;
  TestDialog& operator=(const TestDialog&) = delete;

  ~TestDialog() override {}

  views::View* GetInitiallyFocusedView() override { return this; }
};

// A helper function to create and show a web contents modal dialog.
TestDialog* ShowModalDialog(content::WebContents* web_contents) {
  auto dialog = std::make_unique<TestDialog>();
  TestDialog* dialog_ptr = dialog.get();
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
  return dialog_ptr;
}

class ConstrainedWindowViewTest : public InProcessBrowserTest {
 public:
  ConstrainedWindowViewTest() = default;

  ConstrainedWindowViewTest(const ConstrainedWindowViewTest&) = delete;
  ConstrainedWindowViewTest& operator=(const ConstrainedWindowViewTest&) =
      delete;

  ~ConstrainedWindowViewTest() override = default;
};

}  // namespace

#if BUILDFLAG(IS_MAC)
// Unexpected multiple focus managers on MacViews: http://crbug.com/824551
#define MAYBE_FocusTest DISABLED_FocusTest
#else
#define MAYBE_FocusTest FocusTest
#endif
// Tests the intial focus of tab-modal dialogs, the restoration of focus to the
// browser when they close, and that queued dialogs don't register themselves as
// accelerator targets until they are displayed.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, MAYBE_FocusTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  TestDialog* const dialog1 = ShowModalDialog(web_contents);
  views::ViewTracker tracker1(dialog1);
  EXPECT_EQ(dialog1, tracker1.view());

  // |dialog1| should be active and focused.
  EXPECT_TRUE(dialog1->GetWidget()->IsVisible());
  views::FocusManager* focus_manager = dialog1->GetWidget()->GetFocusManager();
  EXPECT_EQ(dialog1->GetContentsView(), focus_manager->GetFocusedView());

  // Create a second dialog. This will also be modal to |web_contents|, but will
  // remain hidden since the |dialog1| is still showing.
  TestDialog* const dialog2 = ShowModalDialog(web_contents);
  views::ViewTracker tracker2(dialog2);
  EXPECT_EQ(dialog2, tracker2.view());
  EXPECT_FALSE(dialog2->GetWidget()->IsVisible());
  EXPECT_TRUE(dialog1->GetWidget()->IsVisible());
  EXPECT_EQ(focus_manager, dialog2->GetWidget()->GetFocusManager());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_EQ(dialog1->GetContentsView(), focus_manager->GetFocusedView());

  // Pressing return should close |dialog1|.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, tracker1.view());

  // |dialog2| should be visible and focused.
  EXPECT_TRUE(dialog2->GetWidget()->IsVisible());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_EQ(dialog2->GetContentsView(), focus_manager->GetFocusedView());

  // Creating a new tab should take focus away from the other tab's dialog.
  const int tab_with_dialog = browser()->tab_strip_model()->active_index();
  chrome::NewTab(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_NE(dialog2->GetContentsView(), focus_manager->GetFocusedView());

  // Activating the previous tab should bring focus to the dialog.
  browser()->tab_strip_model()->ActivateTabAt(tab_with_dialog);
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_EQ(dialog2->GetContentsView(), focus_manager->GetFocusedView());

  // Pressing enter again should close |dialog2|.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)));
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, tracker2.view());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Tests that the tab-modal window is closed properly when its tab is closed.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, TabCloseTest) {
  TestDialog* const dialog =
      ShowModalDialog(browser()->tab_strip_model()->GetActiveWebContents());
  views::ViewTracker tracker(dialog);
  EXPECT_EQ(dialog, tracker.view());
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());
  chrome::CloseTab(browser());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, tracker.view());
}

// Tests that the tab-modal window is hidden when an other tab is selected and
// shown when its tab is selected again.
// Flaky on ASAN builds (https://crbug.com/997634)
// Flaky on Mac (https://crbug.com/1385896)
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_TabSwitchTest DISABLED_TabSwitchTest
#else
#define MAYBE_TabSwitchTest TabSwitchTest
#endif
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, MAYBE_TabSwitchTest) {
  TestDialog* const dialog =
      ShowModalDialog(browser()->tab_strip_model()->GetActiveWebContents());
  views::ViewTracker tracker(dialog);
  EXPECT_EQ(dialog, tracker.view());
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());

  // Open a new tab. The tab-modal window should hide itself.
  chrome::NewTab(browser());
  EXPECT_FALSE(dialog->GetWidget()->IsVisible());

  // Close the new tab. The tab-modal window should show itself again.
  chrome::CloseTab(browser());
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());

  // Close the original tab.
  chrome::CloseTab(browser());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, tracker.view());
}

// Tests that tab-modal dialogs follow tabs dragged between browser windows.
// TODO(crbug.com/1336418): On Mac, animations cause this test to be flaky.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TabMoveTest DISABLED_TabMoveTest
#else
#define MAYBE_TabMoveTest TabMoveTest
#endif
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, MAYBE_TabMoveTest) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TestDialog* const dialog = ShowModalDialog(web_contents);
  views::ViewTracker tracker(dialog);
  EXPECT_EQ(dialog, tracker.view());
  dialog->GetWidget()->SetVisibilityChangedAnimationsEnabled(false);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());

  // Move the tab to a second browser window; but first create another tab.
  // That prevents the first browser window from closing when its tab is moved.
  chrome::NewTab(browser());
  std::unique_ptr<content::WebContents> owned_web_contents =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(
          browser()->tab_strip_model()->GetIndexOfWebContents(web_contents));
  Browser* browser2 = CreateBrowser(browser()->profile());
  browser2->tab_strip_model()->AppendWebContents(std::move(owned_web_contents),
                                                 true);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());

  // Close the first browser.
  chrome::CloseWindow(browser());
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());

  // Close the dialog's browser window.
  chrome::CloseTab(browser2);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, tracker.view());
}

// Tests that the dialog closes when the escape key is pressed.
IN_PROC_BROWSER_TEST_F(ConstrainedWindowViewTest, ClosesOnEscape) {
  TestDialog* const dialog =
      ShowModalDialog(browser()->tab_strip_model()->GetActiveWebContents());
  views::ViewTracker tracker(dialog);
  EXPECT_EQ(dialog, tracker.view());
  // On Mac, animations cause this test to be flaky.
  dialog->GetWidget()->SetVisibilityChangedAnimationsEnabled(false);
  EXPECT_TRUE(dialog->GetWidget()->IsVisible());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE,
                                              false, false, false, false));
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(nullptr, tracker.view());
}
