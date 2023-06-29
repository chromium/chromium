// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/views/extensions/extension_install_friction_dialog_view.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/views/test/widget_test.h"

namespace {

void CloseAndWait(views::Widget* widget) {
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();
}

}  // namespace

// Helper class to display the ExtensionInstallFrictionDialogView dialog for
// testing.
class ExtensionInstallFrictionDialogTest : public DialogBrowserTest {
 public:
  ExtensionInstallFrictionDialogTest() = default;

  void ShowUi(const std::string& name) override {
    extensions::ShowExtensionInstallFrictionDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallFrictionDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

class ExtensionInstallFrictionDialogViewTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionInstallFrictionDialogViewTest() = default;
  ExtensionInstallFrictionDialogViewTest(
      const ExtensionInstallFrictionDialogViewTest&) = delete;
  ExtensionInstallFrictionDialogViewTest& operator=(
      const ExtensionInstallFrictionDialogViewTest&) = delete;

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    // Note: Any extension will do.
    extension_ = LoadExtension(test_data_dir_.AppendASCII("install/install"));
    web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  ExtensionInstallFrictionDialogView* CreateAndShowFrictionDialogView() {
    auto dialog = std::make_unique<ExtensionInstallFrictionDialogView>(
        web_contents(), base::BindOnce([](bool) {}));
    ExtensionInstallFrictionDialogView* delegate_view = dialog.get();

    views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
        dialog.release(), nullptr,
        platform_util::GetViewForWindow(
            browser()->window()->GetNativeWindow()));
    modal_dialog->Show();

    return delegate_view;
  }

 protected:
  content::WebContents* web_contents() { return web_contents_; }

 private:
  raw_ptr<const extensions::Extension, DanglingUntriaged> extension_ = nullptr;
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;
};

// Regression test for https://crbug.com/1201031: Ensures that while an
// ExtensionInstallFrictionDialogView is visible, it does not (and cannot) refer
// to its originator tab/WebContents after the tab's closure.
IN_PROC_BROWSER_TEST_F(ExtensionInstallFrictionDialogViewTest,
                       TabClosureClearsWebContentsFromDialogView) {
  ExtensionInstallFrictionDialogView* delegate_view =
      CreateAndShowFrictionDialogView();
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* originator_contents =
      tab_strip_model->GetActiveWebContents();
  EXPECT_EQ(originator_contents, delegate_view->parent_web_contents());

  // Add a second tab.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  {
    // Close the first tab that results in install dialog moving to the second
    // tab.
    int tab1_idx = tab_strip_model->GetIndexOfWebContents(originator_contents);
    content::WebContentsDestroyedWatcher tab_destroyed_watcher(
        tab_strip_model->GetWebContentsAt(tab1_idx));
    int previous_tab_count = tab_strip_model->count();
    tab_strip_model->CloseWebContentsAt(tab1_idx, TabCloseTypes::CLOSE_NONE);
    EXPECT_EQ(previous_tab_count - 1, tab_strip_model->count());
    tab_destroyed_watcher.Wait();
  }

  // The dialog remains visible even though |originator_contents| is gone. Note
  // that this doesn't seem quite intuitive, but this is how things are at the
  // moment. See crbug.com/1201031 for details.
  EXPECT_TRUE(delegate_view->GetVisible());

  // After WebContents is destroyed, ensure |delegate_view| sees it as
  // nullptr.
  EXPECT_EQ(nullptr, delegate_view->parent_web_contents());

  // TODO(lazyboy): This is similar to TabAddedObserver in
  // extension_install_view_dialog_browsertest.cc, consider putting it in a
  // common place.
  class TabAddedObserver : public TabStripModelObserver {
   public:
    explicit TabAddedObserver(TabStripModel* tab_strip_model) {
      tab_strip_model->AddObserver(this);
    }

    void WaitForWebstoreTabAdded() { run_loop_.Run(); }

    // TabStripModelObserver:
    void OnTabStripModelChanged(
        TabStripModel* tab_strip_model,
        const TabStripModelChange& change,
        const TabStripSelectionChange& selection) override {
      if (change.type() != TabStripModelChange::kInserted)
        return;

      GURL learn_more_url(chrome::kCwsEnhancedSafeBrowsingLearnMoreURL);
      for (const auto& contents : change.GetInsert()->contents) {
        // Note: GetVisibleURL() is used instead of GetLastCommittedURL() for
        // simplicity's sake as this test doesn't serve webstore url and
        // the url doesn't commit.
        const GURL& url = contents.contents->GetVisibleURL();
        if (url == learn_more_url) {
          run_loop_.Quit();
          return;
        }
      }
    }

   private:
    base::RunLoop run_loop_;
  };

  // Click "learn more" link.
  {
    TabAddedObserver observer(tab_strip_model);
    delegate_view->ClickLearnMoreLinkForTesting();
    observer.WaitForWebstoreTabAdded();
  }

  CloseAndWait(delegate_view->GetWidget());
}
