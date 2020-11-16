// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#include <string>

#include "base/callback_helpers.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

// Interactive UI tests for the hung renderer (aka page unresponsive) dialog.
class HungRendererDialogViewBrowserTest : public DialogBrowserTest {
 public:
  HungRendererDialogViewBrowserTest() {}

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
        web_contents->GetMainFrame()->GetRenderViewHost()->GetWidget(),
        base::DoNothing::Repeatedly());

    if (name == "MultiplePages") {
      auto* web_contents2 = chrome::DuplicateTabAt(browser(), 0);
      AddWebContents(HungRendererDialogView::GetInstance(), web_contents2);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogViewBrowserTest);
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
  // Simulate creation of the dialog, without initializing or showing it yet.
  // This is what happens when HungRendererDialogView::ShowForWebContents
  // returns early if the frame or the dialog are not active.
  HungRendererDialogView::Create(browser()->window()->GetNativeWindow());
  EXPECT_TRUE(HungRendererDialogView::GetInstance());

  // Simulate the renderer becoming responsive again.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderWidgetHost* render_widget_host =
      web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  content::WebContentsDelegate* web_contents_delegate = browser();
  web_contents_delegate->RendererResponsive(web_contents, render_widget_host);
}
