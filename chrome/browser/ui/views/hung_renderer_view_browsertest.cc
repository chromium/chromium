// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/accessibility/view_accessibility.h"

// Interactive UI tests for the hung renderer (aka page unresponsive) dialog.
class HungRendererDialogViewBrowserTest : public DialogBrowserTest {
 public:
  HungRendererDialogViewBrowserTest() {
    HungRendererDialogView::BypassActiveBrowserRequirementForTests();
  }

  HungRendererDialogViewBrowserTest(const HungRendererDialogViewBrowserTest&) =
      delete;
  HungRendererDialogViewBrowserTest& operator=(
      const HungRendererDialogViewBrowserTest&) = delete;

  // Normally the dialog only shows multiple WebContents when they're all part
  // of the same process, but that's hard to achieve in a test.
  void AddWebContents(HungRendererDialogView* dialog,
                      content::WebContents* web_contents) {
    HungPagesTableModel* model = dialog->hung_pages_table_model_.get();
    model->tab_observers_.push_back(
        std::make_unique<HungPagesTableModel::WebContentsObserverImpl>(
            model, web_contents));
    if (model->observer_)
      model->observer_->OnModelChanged();
    dialog->UpdateLabels();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    HungRendererDialogView::Show(
        web_contents,
        web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget(),
        base::DoNothing());

    if (name == "MultiplePages") {
      HungRendererDialogView* view =
          HungRendererDialogView::GetInstanceForWebContentsForTests(
              web_contents);
      auto* web_contents2 = chrome::DuplicateTabAt(browser(), 0);
      AddWebContents(view, web_contents2);
    }
  }

  HungRendererDialogView* CreateDialogView() {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    return HungRendererDialogView::CreateInstance(
        web_contents, browser()->window()->GetNativeWindow());
  }

  void EndForWebContents(HungRendererDialogView* dialog,
                         content::WebContents* contents) {
    dialog->EndDialog(
        contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());
  }
};

// TODO(tapted): The framework sometimes doesn't pick up the spawned dialog and
// the ASSERT_EQ in TestBrowserUi::ShowAndVerifyUi() fails. This seems to only
// happen on the bots. So these tests are disabled for now.
IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest,
                       DISABLED_InvokeUi_Default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest,
                       DISABLED_InvokeUi_MultiplePages) {
  ShowAndVerifyUi();
}

// This is a regression test for https://crbug.com/855369.
IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest, InactiveWindow) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Simulate creation of the dialog, without initializing or showing it yet.
  // This is what happens when HungRendererDialogView::ShowForWebContents
  // returns early if the frame or the dialog are not active.
  CreateDialogView();
  ASSERT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents));

  // Simulate the renderer becoming responsive again.
  content::RenderWidgetHost* render_widget_host =
      web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  content::WebContentsDelegate* web_contents_delegate = browser();
  web_contents_delegate->RendererResponsive(web_contents, render_widget_host);
}

IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest, ProcessClosed) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  HungRendererDialogView* dialog = CreateDialogView();
  ASSERT_TRUE(dialog);
  ASSERT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents));

  // The Hung Dialog requires window activation, window activations does not
  // work reliably in browser tests, especially in Mac because of the activation
  // policy for obtaining window key status is set to prohibited. Instead of
  // showing the window, populate the table model instead.
  dialog->table_model_for_testing()->InitForWebContents(
      web_contents,
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget(),
      base::DoNothing());

  // Makes sure the virtual accessibility views are in sync with the model when
  // the dialog is created. Should consist of a single item.
  views::ViewAccessibility& view_accessibility =
      dialog->table_for_testing()->GetViewAccessibility();
  EXPECT_EQ(size_t{1}, view_accessibility.virtual_children().size());

  // Simulate an abrupt ending to the WebContents. The accessibility tree for
  // the TableView depends on the Hung Render View table model to send the right
  // events. By checking the virtual tree, we can guarantee the sync is
  // correct.
  EndForWebContents(dialog, web_contents);
  EXPECT_EQ(size_t{0}, view_accessibility.virtual_children().size());
}

// This dialog used to be a singleton, so this is a basic test that it can now
// be shown on two different browser windows without crashing or obviously
// misbehaving.
IN_PROC_BROWSER_TEST_F(HungRendererDialogViewBrowserTest, TwoHungBrowsers) {
  if (logging::DialogsAreSuppressed()) {
    // If dialogs are suppressed then the hung renderer dialog can't be shown,
    // and this test cannot pass, so bail.
    LOG(ERROR) << "HungRendererDialogViewBrowserTest.TwoHungBrowsers cannot "
                  "run on bots, as hang dialogs are suppressed on them. You "
                  "will need to run this test locally to verify your fixes.";
    return;
  }

  Browser* browser1 = browser();
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();
  content::RenderWidgetHost* widget_host1 =
      web_contents1->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget();

  Browser* browser2 =
      Browser::Create(Browser::CreateParams(browser1->profile(), true));
  chrome::NewTab(browser2);
  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();
  content::RenderWidgetHost* widget_host2 =
      web_contents2->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget();

  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Show(web_contents1, widget_host1, base::DoNothing());
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Show(web_contents2, widget_host2, base::DoNothing());
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Hide(web_contents1, widget_host1);
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Hide(web_contents2, widget_host2);
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents2));

  HungRendererDialogView::Show(web_contents1, widget_host1, base::DoNothing());
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Show(web_contents2, widget_host2, base::DoNothing());
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Hide(web_contents2, widget_host2);
  EXPECT_TRUE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
  HungRendererDialogView::Hide(web_contents1, widget_host1);
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents1));
  EXPECT_FALSE(HungRendererDialogView::IsShowingForWebContents(web_contents2));
}
