// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/incognito_clear_browsing_data_dialog_coordinator.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_observer.h"

namespace {

class IncognitoClearBrowsingDataDialogTest : public InProcessBrowserTest {
 public:
  void OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type type) {
    incognito_browser_ = CreateIncognitoBrowser(browser()->profile());
    auto* coordinator = GetCoordinator();
    coordinator->Show(type);
    EXPECT_TRUE(coordinator->IsShowing());
  }

  Browser* GetIncognitoBrowser() { return incognito_browser_; }

  IncognitoClearBrowsingDataDialog* GetDialogView() {
    return GetCoordinator()->GetIncognitoClearBrowsingDataDialogForTesting();
  }

  views::Widget* GetDialogWidget() {
    auto* dialog_view = GetDialogView();
    return dialog_view ? dialog_view->GetWidget() : nullptr;
  }

  IncognitoClearBrowsingDataDialogCoordinator* GetCoordinator() {
    return IncognitoClearBrowsingDataDialogCoordinator::GetOrCreateForBrowser(
        incognito_browser_);
  }

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

// Used to test that the bubble widget is destroyed before the browser.
class BubbleWidgetDestroyedObserver : public views::WidgetObserver {
 public:
  explicit BubbleWidgetDestroyedObserver(views::Widget* bubble_widget) {
    bubble_widget->AddObserver(this);
  }
  ~BubbleWidgetDestroyedObserver() override = default;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    ASSERT_GT(BrowserList::GetIncognitoBrowserCount(), 0u);
  }
};

}  // namespace

class IncognitoClearBrowsingDataDialogBrowserTest
    : public SupportsTestDialog<IncognitoClearBrowsingDataDialogTest> {
 public:
  void ShowUi(const std::string& name) override {
    OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::kDefaultBubble);
  }
};

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestDialogIsShown_DefaultBubble) {
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::kDefaultBubble);
  auto* incognito_cbd_dialog_view = GetDialogView();

  ASSERT_TRUE(GetCoordinator()->IsShowing());
  ASSERT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            incognito_cbd_dialog_view->buttons());
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kCancel));
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestCloseWindowsButton) {
  base::HistogramTester histogram_tester;
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::kDefaultBubble);
  auto destroyed_observer =
      BubbleWidgetDestroyedObserver(GetDialogView()->GetWidget());

  GetDialogView()->AcceptDialog();
  histogram_tester.ExpectBucketCount(
      "Incognito.ClearBrowsingDataDialog.ActionType",
      IncognitoClearBrowsingDataDialog::DialogActionType::kCloseIncognito, 1);
  ui_test_utils::WaitForBrowserToClose(GetIncognitoBrowser());
  ASSERT_EQ(0u, BrowserList::GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest, TestCancelButton) {
  base::HistogramTester histogram_tester;
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::kDefaultBubble);

  views::test::WidgetDestroyedWaiter destroyed_waiter(GetDialogWidget());

  GetDialogView()->Cancel();
  histogram_tester.ExpectBucketCount(
      "Incognito.ClearBrowsingDataDialog.ActionType",
      IncognitoClearBrowsingDataDialog::DialogActionType::kCancel, 1);
  destroyed_waiter.Wait();

  ASSERT_FALSE(GetCoordinator()->IsShowing());
  ASSERT_FALSE(GetDialogView());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestBrowserCloseEventClosesDialogFirst) {
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::kDefaultBubble);
  auto destroyed_observer = BubbleWidgetDestroyedObserver(GetDialogWidget());

  CloseBrowserSynchronously(GetIncognitoBrowser());

  ASSERT_EQ(0u, BrowserList::GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       ClearBrowsingDataNavigationInIncognito) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  ui_test_utils::SendToOmniboxAndSubmit(incognito_browser,
                                        "chrome://settings/clearBrowserData");
  std::u16string current_tab_title;
  ui_test_utils::GetCurrentTabTitle(incognito_browser, &current_tab_title);
  EXPECT_EQ(u"about:blank", current_tab_title);
  auto* coordinator = IncognitoClearBrowsingDataDialogCoordinator::FromBrowser(
      incognito_browser);
  ASSERT_TRUE(coordinator->IsShowing());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestDialogIsShown_HistoryDisclaimerBubble) {
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::
                 kHistoryDisclaimerBubble);
  auto* incognito_cbd_dialog_view = GetDialogView();

  ASSERT_TRUE(GetCoordinator()->IsShowing());
  ASSERT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            incognito_cbd_dialog_view->buttons());
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));
  ASSERT_TRUE(incognito_cbd_dialog_view->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kCancel));
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest,
                       TestCloseIncognitoButton) {
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::
                 kHistoryDisclaimerBubble);

  GetDialogView()->CancelDialog();
  ui_test_utils::WaitForBrowserToClose(GetIncognitoBrowser());
  ASSERT_EQ(0u, BrowserList::GetIncognitoBrowserCount());
}

IN_PROC_BROWSER_TEST_F(IncognitoClearBrowsingDataDialogTest, TestGotItButton) {
  OpenDialog(IncognitoClearBrowsingDataDialogInterface::Type::
                 kHistoryDisclaimerBubble);

  views::test::WidgetDestroyedWaiter destroyed_waiter(GetDialogWidget());

  GetDialogView()->AcceptDialog();
  destroyed_waiter.Wait();

  ASSERT_FALSE(GetCoordinator()->IsShowing());
  ASSERT_FALSE(GetDialogView());
}
