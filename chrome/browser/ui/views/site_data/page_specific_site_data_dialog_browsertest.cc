// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/page_info/core/features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

const char kCookiesInUseDialogOpenedActionName[] = "CookiesInUseDialog.Opened";

const char kDeleteBrowsingDataActionName[] =
    "Privacy.DeleteBrowsingData.Action";

void ClickButton(views::Button* button) {
  views::test::ButtonTestApi test_api(button);
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  test_api.NotifyClick(e);
}

}  // namespace

class PageSpecificSiteDataDialogBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PageSpecificSiteDataDialogBrowserTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  PageSpecificSiteDataDialogBrowserTest(
      const PageSpecificSiteDataDialogBrowserTest&) = delete;
  PageSpecificSiteDataDialogBrowserTest& operator=(
      const PageSpecificSiteDataDialogBrowserTest&) = delete;

  ~PageSpecificSiteDataDialogBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->Start());

    content::CookieChangeObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(), 2);

    // Load a page with cookies.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server()->GetURL("a.test", "/cookie1.html")));

    observer.Wait();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  views::Widget* OpenDialog() {
    std::string widget_name = "PageSpecificSiteDataDialog";
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
    DCHECK(tracked_element);
    return tracked_element->AsA<views::TrackedElementViews>()->view();
  }

  views::View* GetViewByIdentifierAtIndex(ui::ElementContext context,
                                          ui::ElementIdentifier id,
                                          size_t index) {
    auto* element_tracker = ui::ElementTracker::GetElementTracker();
    auto tracked_elements =
        element_tracker->GetAllMatchingElements(id, context);
    DCHECK(tracked_elements.size() > index);
    return tracked_elements[index]->AsA<views::TrackedElementViews>()->view();
  }

  void ClickBlockMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/40231917): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnBlockMenuItemClicked(/*event_flags*/ 0);
  }

  void ClickAllowMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/40231917): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnAllowMenuItemClicked(/*event_flags*/ 0);
  }

  void ClickClearOnExitMenuItem(SiteDataRowView* row_view) {
    // TODO(crbug.com/40231917): Get the menu item from the the menu runner and
    // click on it.
    row_view->OnClearOnExitMenuItemClicked(/*event_flags*/ 0);
  }

  size_t infobar_count() const {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents
               ? infobars::ContentInfoBarManager::FromWebContents(web_contents)
                     ->infobars()
                     .size()
               : 0;
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

///////////////////////////////////////////////////////////////////////////////
// Testing the dialog lifecycle, if the dialog is properly destroyed in
// different scenarious.

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest, CloseDialog) {
  // Test opening and closing dialog.
  base::UserActionTester user_actions;

  // No actions recorded at start.
  EXPECT_EQ(0,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));

  auto* dialog = OpenDialog();
  EXPECT_FALSE(dialog->IsClosed());

  dialog->Close();
  EXPECT_TRUE(dialog->IsClosed());

  EXPECT_EQ(0u, infobar_count());
  // Expect recorded open dialog action.
  EXPECT_EQ(1,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       NavigateSameOrigin) {
  // Test navigating while the dialog is open.
  // Navigating to the another page with the same origin won't close dialog.
  auto* dialog = OpenDialog();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/cookie2.html")));
  EXPECT_FALSE(dialog->IsClosed());
}

// TODO(crbug.com/40231917): Figure out why the dialog isn't closed when
// nnavigating away on Linux and overall flaky on other platforms.
IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       DISABLED_NavigateAway) {
  // Test navigating while the dialog is open.
  // Navigation in the owning tab will close dialog.
  auto* dialog = OpenDialog();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("b.test", "/cookie2.html")));

  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  EXPECT_TRUE(dialog->IsClosed());
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       ChangeAndCloseTab) {
  // Test closing tab while the dialog is open.
  // Closing the owning tab will close dialog.
  auto* dialog = OpenDialog();

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view = GetViewByIdentifier(context, kPageSpecificSiteDataDialogRow);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  EXPECT_TRUE(row_view->GetVisible());

  EXPECT_TRUE(row_view->delete_button_for_testing()->GetVisible());
  ClickButton(row_view->delete_button_for_testing());

  EXPECT_FALSE(row_view->GetVisible());

  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  EXPECT_TRUE(dialog->IsClosed());
  EXPECT_EQ(0u, infobar_count());
}

// Closing the widget asynchronously destroys the CollectedCookiesViews object,
// but synchronously removes it from the WebContentsModalDialogManager. Make
// sure there's no crash when trying to re-open the dialog right
// after closing it. Regression test for https://crbug.com/989888
IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       CloseDialogAndReopen) {
  auto* dialog = OpenDialog();

  dialog->Close();
  EXPECT_TRUE(dialog->IsClosed());

  auto* new_dialog = OpenDialog();
  EXPECT_FALSE(new_dialog->IsClosed());
  // If the test didn't crash, it has passed.
}

// TODO(crbug.com/40231917): Add testing dialog functionality such as showing
// infobar after changes, changing content settings, deleting data.

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest, DeleteMenuItem) {
  base::HistogramTester histograms;
  base::UserActionTester user_actions;
  const std::string remove_action = "CookiesInUseDialog.RemoveButtonClicked";

  // The histogram should start empty and no actions recorded.
  histograms.ExpectTotalCount(kDeleteBrowsingDataActionName, 0);
  // No actions recorded at start.
  EXPECT_EQ(0,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));
  EXPECT_EQ(0, user_actions.GetActionCount(remove_action));

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);
  // Expect recorded open dialog action.
  EXPECT_EQ(1,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));

  auto* view = GetViewByIdentifier(context, kPageSpecificSiteDataDialogRow);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  EXPECT_TRUE(row_view->GetVisible());

  EXPECT_TRUE(row_view->delete_button_for_testing()->GetVisible());
  ClickButton(row_view->delete_button_for_testing());

  EXPECT_FALSE(row_view->GetVisible());

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());

  histograms.ExpectBucketCount(
      kDeleteBrowsingDataActionName,
      browsing_data::DeleteBrowsingDataAction::kCookiesInUseDialog, 1);

  EXPECT_EQ(1, user_actions.GetActionCount(remove_action));
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest, BlockMenuItem) {
  base::UserActionTester user_actions;
  // No actions recorded at start.
  EXPECT_EQ(0,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  // Expect recorded open dialog action.
  EXPECT_EQ(1,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));
  auto* view = GetViewByIdentifier(context, kPageSpecificSiteDataDialogRow);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // The delete button is available for not blocked sites.
  EXPECT_TRUE(row_view->delete_button_for_testing()->GetVisible());
  // TODO(crbug.com/40231917): The label shouldn't be visible here but
  // GetVisible returns true. It's not actually visible because it has size 0.
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
  // The delete button isn't available for blocked sites.
  EXPECT_FALSE(row_view->delete_button_for_testing()->GetVisible());

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest, AllowMenuItem) {
  base::UserActionTester user_actions;
  // No actions recorded at start.
  EXPECT_EQ(0,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  // Expect recorded open dialog action.
  EXPECT_EQ(1,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));
  auto* view = GetViewByIdentifier(context, kPageSpecificSiteDataDialogRow);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // TODO(crbug.com/40231917): The label shouldn't be visible here but
  // GetVisible returns true. It's not actually visible because it has size 0.
  // TODO(crbug.com/40231917): Setup a site with blocked cookies to start with
  // blocked state here.
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(row_view);

  EXPECT_EQ(row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
  ClickAllowMenuItem(row_view);

  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       ClearOnExitMenuItem) {
  base::UserActionTester user_actions;
  // No actions recorded at start.
  EXPECT_EQ(0,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));
  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  // Expect recorded open dialog action.
  EXPECT_EQ(1,
            user_actions.GetActionCount(kCookiesInUseDialogOpenedActionName));
  auto* view = GetViewByIdentifier(context, kPageSpecificSiteDataDialogRow);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // TODO(crbug.com/40231917): The label shouldn't be visible here but
  // GetVisible returns true. It's not actually visible because it has size 0.
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickClearOnExitMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(
      row_view->state_label_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_SESSION_ONLY_STATE_SUBTITLE));

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

class PageSpecificSiteDataDialogPre3pcdBrowserTest
    : public PageSpecificSiteDataDialogBrowserTest {
 public:
  PageSpecificSiteDataDialogPre3pcdBrowserTest() {
    feature_list_.InitAndDisableFeature(
        content_settings::features::kTrackingProtection3pcd);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogPre3pcdBrowserTest,
                       PartitionedCookiesAndAllowedThirdParty) {
  // Allow third-party cookies.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));

  content::CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 8);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(
                     "a.test", "/third_party_partitioned_cookies.html")));

  observer.Wait();

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  // Get the first row: "b.test" with only partitioned cookie set.
  auto* partitioned_row_view =
      static_cast<SiteDataRowView*>(GetViewByIdentifierAtIndex(
          context, kPageSpecificSiteDataDialogRow, /*index=*/1));
  // Only partitioned cookie was set in this third-party context. Third-party
  // cookies are allowed, so the access is shown as allowed.
  EXPECT_TRUE(partitioned_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(partitioned_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));
  ClickButton(partitioned_row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(partitioned_row_view);
  EXPECT_TRUE(partitioned_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(partitioned_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
  // TODO(crbug.com/40231917): Check the histograms value.

  // Get the second row: "c.test" with both partitioned and regular cookies set.
  auto* mixed_row_view =
      static_cast<SiteDataRowView*>(GetViewByIdentifierAtIndex(
          context, kPageSpecificSiteDataDialogRow, /*index=*/2));
  // Both third-party and partitioned cookies are allowed access.
  // TODO(crbug.com/40231917): The label shouldn't be visible here but
  // GetVisible returns true. It's not actually visible because it has size 0.
  EXPECT_EQ(mixed_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));
  ClickButton(mixed_row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(mixed_row_view);
  EXPECT_TRUE(mixed_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(mixed_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
  // TODO(crbug.com/40231917): Check the histograms value.
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       PartitionedCookiesAndBlockedThirdParty) {
  // Block third-party cookies.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  content::CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 9);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(
                     "a.test", "/third_party_partitioned_cookies.html")));

  observer.Wait();

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  // Get the first row: "b.test" with only partitioned cookie set.
  auto* partitioned_row_view =
      static_cast<SiteDataRowView*>(GetViewByIdentifierAtIndex(
          context, kPageSpecificSiteDataDialogRow, /*index=*/1));
  // Only partitioned cookie was set in this third-party context. Partitioned
  // cookie isn't blocked as a third party cookie because it is treated as a
  // first party. The state is "Only using partitioned storage" as other types
  // of storage are not used or blocked from access.
  EXPECT_TRUE(partitioned_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(partitioned_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE));
  ClickButton(partitioned_row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(partitioned_row_view);
  EXPECT_TRUE(partitioned_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(partitioned_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
  // TODO(crbug.com/40231917): Check the histograms value.

  // Get the second row: "c.test" with both partitioned and regular cookies set.
  auto* mixed_row_view =
      static_cast<SiteDataRowView*>(GetViewByIdentifierAtIndex(
          context, kPageSpecificSiteDataDialogRow, /*index=*/2));
  // Regular third party cookies is blocked by third-party cookie blocking
  // perf, partitioned cookie isn't because it is treated
  // as a first party. The state is "Only using partitioned storage" as other
  // types of storage are not used or blocked from access.
  EXPECT_TRUE(mixed_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(mixed_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE));
  ClickButton(mixed_row_view->menu_button_for_testing());
  // TODO(crbug.com/40231917): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(mixed_row_view);
  EXPECT_TRUE(mixed_row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(mixed_row_view->state_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
  // TODO(crbug.com/40231917): Check the histograms value.
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogBrowserTest,
                       SameOriginNavigationDeletion) {
  // Regression test for crbug.com/1421521. As the dialog remains open during
  // same-origin navigations, it mustn't cache any pointers owned by the
  // PageSpecificContentSettings, which is _page_ specific, and so changes even
  // on same-origin navigations. Attempting a deletion is sufficient to access
  // the BrowsingDataModel, which is owned by the PageSpecificContentSettings,
  // and so must _not_ have pointers cached by the dialog.

  auto* dialog = OpenDialog();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("a.test", "/cookie2.html")));
  EXPECT_FALSE(dialog->IsClosed());

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);
  auto* view = GetViewByIdentifier(context, kPageSpecificSiteDataDialogRow);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  EXPECT_TRUE(row_view->GetVisible());

  EXPECT_TRUE(row_view->delete_button_for_testing()->GetVisible());
  ClickButton(row_view->delete_button_for_testing());
  EXPECT_FALSE(dialog->IsClosed());
}
