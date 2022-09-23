// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/page_info/core/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

class PageSpecificSiteDataDialogBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PageSpecificSiteDataDialogBrowserTest() {
    feature_list_.InitWithFeatureState(page_info::kPageSpecificSiteDataDialog,
                                       GetParam());
  }

  PageSpecificSiteDataDialogBrowserTest(
      const PageSpecificSiteDataDialogBrowserTest&) = delete;
  PageSpecificSiteDataDialogBrowserTest& operator=(
      const PageSpecificSiteDataDialogBrowserTest&) = delete;

  ~PageSpecificSiteDataDialogBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Load a page with cookies.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("a.test", "/cookie1.html")));
  }

  views::Widget* OpenDialog() {
    std::string widget_name =
        GetParam() ? "PageSpecificSiteDataDialog" : "CollectedCookiesViews";
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         widget_name);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
        web_contents);
    return waiter.WaitIfNeededAndGet();
  }

  views::View* GetViewByIdentifier(ui::ElementContext context,
                                   ui::ElementIdentifier id) {
    auto* element_tracker = ui::ElementTracker::GetElementTracker();
    auto* tracked_element =
        element_tracker->GetFirstMatchingElement(id, context);
    return tracked_element->AsA<views::TrackedElementViews>()->view();
  }

  void ClickDeleteMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/1344787): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnDeleteMenuItemClicked(/*event_flags*/ 0);
  }

  void ClickBlockMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/1344787): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnBlockMenuItemClicked(/*event_flags*/ 0);
  }

  void ClickAllowMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/1344787): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnAllowMenuItemClicked(/*event_flags*/ 0);
  }

  void ClickClearOnExitMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/1344787): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnClearOnExitMenuItemClicked(/*event_flags*/ 0);
  }

  size_t infobar_count() const {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents
               ? infobars::ContentInfoBarManager::FromWebContents(web_contents)
                     ->infobar_count()
               : 0;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

///////////////////////////////////////////////////////////////////////////////
// Testing the dialog lifecycle, if the dialog is properly destroyed in
// different scenarious.

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest, CloseDialog) {
  // Test closing dialog.
  auto* dialog = OpenDialog();
  EXPECT_FALSE(dialog->IsClosed());

  dialog->Close();
  EXPECT_TRUE(dialog->IsClosed());

  EXPECT_EQ(0u, infobar_count());
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       NavigateSameOrigin) {
  // Test navigating while the dialog is open.
  // Navigating to the another page with the same origin won't close dialog.
  auto* dialog = OpenDialog();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/cookie2.html")));
  EXPECT_FALSE(dialog->IsClosed());
}

// TODO(crbug.com/1344787): Figure out why the dialog isn't closed when
// nnavigating away on Linux and overall flaky on other platforms.
IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       DISABLED_NavigateAway) {
  // Test navigating while the dialog is open.
  // Navigation in the owning tab will close dialog.
  auto* dialog = OpenDialog();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.test", "/cookie2.html")));

  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  EXPECT_TRUE(dialog->IsClosed());
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       ChangeAndCloseTab) {
  if (!GetParam()) {
    return;
  }

  // Test closing tab while the dialog is open.
  // Closing the owning tab will close dialog.
  auto* dialog = OpenDialog();

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view =
      GetViewByIdentifier(context, kPageSpecificSiteDataDialogRowForTesting);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  EXPECT_TRUE(row_view->GetVisible());
  ClickDeleteMenuItem(row_view);
  EXPECT_FALSE(row_view->GetVisible());

  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  EXPECT_TRUE(dialog->IsClosed());
  EXPECT_EQ(0u, infobar_count());
}

// Closing the widget asynchronously destroys the CollectedCookiesViews object,
// but synchronously removes it from the WebContentsModalDialogManager. Make
// sure there's no crash when trying to re-open the dialog right
// after closing it. Regression test for https://crbug.com/989888
IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       CloseDialogAndReopen) {
  auto* dialog = OpenDialog();

  dialog->Close();
  EXPECT_TRUE(dialog->IsClosed());

  auto* new_dialog = OpenDialog();
  EXPECT_FALSE(new_dialog->IsClosed());
  // If the test didn't crash, it has passed.
}

// TODO(crbug.com/1344787): Add testing dialog functionality such as showing
// infobar after changes, changing content settings, deleting data.

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest, DeleteMenuItem) {
  if (!GetParam()) {
    return;
  }

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view =
      GetViewByIdentifier(context, kPageSpecificSiteDataDialogRowForTesting);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  EXPECT_TRUE(row_view->GetVisible());
  ClickDeleteMenuItem(row_view);
  EXPECT_FALSE(row_view->GetVisible());
  // TODO(crbug.com/1344787): Check the histograms value.
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest, BlockMenuItem) {
  if (!GetParam()) {
    return;
  }

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view =
      GetViewByIdentifier(context, kPageSpecificSiteDataDialogRowForTesting);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // TODO(crbug.com/1344787): The label shouldn't be visible here but GetVisible
  // returns true. It's not actually visible because it has size 0.
  ClickBlockMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Blocked");
  // TODO(crbug.com/1344787): Check the histograms value.

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest, AllowMenuItem) {
  if (!GetParam()) {
    return;
  }

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view =
      GetViewByIdentifier(context, kPageSpecificSiteDataDialogRowForTesting);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // TODO(crbug.com/1344787): The label shouldn't be visible here but GetVisible
  // returns true. It's not actually visible because it has size 0.
  // TODO(crbug.com/1344787): Setup a site with blocked cookies to start with
  // blocked state here.
  ClickBlockMenuItem(row_view);
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Blocked");
  ClickAllowMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Allowed");
  // TODO(crbug.com/1344787): Check the histograms value.

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       ClearOnExitMenuItem) {
  if (!GetParam()) {
    return;
  }

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view =
      GetViewByIdentifier(context, kPageSpecificSiteDataDialogRowForTesting);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // TODO(crbug.com/1344787): The label shouldn't be visible here but GetVisible
  // returns true. It's not actually visible because it has size 0.
  ClickClearOnExitMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Clear on close");
  // TODO(crbug.com/1344787): Check the histograms value.

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

// Run tests with kPageSpecificSiteDataDialog flag enabled and disabled.
INSTANTIATE_TEST_SUITE_P(All,
                         PageSpecificSiteDataDialogBrowserTest,
                         ::testing::Values(false, true));
