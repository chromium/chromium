// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/page_info/core/features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

void SetCookieFromJS(content::RenderFrameHost* frame, std::string cookie) {
  EXPECT_TRUE(content::ExecJs(frame, "document.cookie = '" + cookie + "'"));
  // Run loop to wait for cookie to be set. Without this, tests are flaky and
  // dialog is sometimes open without cookies.
  base::RunLoop().RunUntilIdle();
}

void ClickButton(views::Button* button) {
  views::test::ButtonTestApi test_api(button);
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  test_api.NotifyClick(e);
}

}  // namespace

class PageSpecificSiteDataDialogBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  PageSpecificSiteDataDialogBrowserTest() {
    feature_list_.InitWithFeatureState(page_info::kPageSpecificSiteDataDialog,
                                       GetParam());
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

    // Load a page with cookies.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server()->GetURL(
                       "a.test", "/iframe_cross_site_with_script.html")));

    // Set one first party cookie.
    content::RenderFrameHost* main_frame = GetMainFrame();
    SetCookieFromJS(main_frame, "name=Good;");
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  content::RenderFrameHost* GetMainFrame() {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
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
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
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
      browser(), https_server()->GetURL("a.test", "/cookie2.html")));
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
      browser(), https_server()->GetURL("b.test", "/cookie2.html")));

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
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
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
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
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
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
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
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickClearOnExitMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Clear on close");
  // TODO(crbug.com/1344787): Check the histograms value.

  dialog->Close();
  EXPECT_EQ(1u, infobar_count());
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       OnlyPartitionedAccess) {
  if (!GetParam()) {
    return;
  }

  content::RenderFrameHost* main_frame = GetMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  // Set one partitioned third party cookie. No
  // regular third party cookies.
  SetCookieFromJS(
      iframe,
      "__Host-name=partitioned; Secure; Path=/; SameSite=None; Partitioned;");

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view = GetViewByIdentifierAtIndex(
      context, kPageSpecificSiteDataDialogRowForTesting, /*index=*/1);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // Only partitioned cookie was set in this third-party context.
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(),
            u"Only using partitioned storage");
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Blocked");
  // TODO(crbug.com/1344787): Check the histograms value.
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       MixedAccessPartitionedAndThirdParty) {
  if (!GetParam()) {
    return;
  }

  content::RenderFrameHost* main_frame = GetMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  // Set one partitioned third party cookie and one
  // regular third party cookie.
  SetCookieFromJS(
      iframe,
      "__Host-name=partitioned; Secure; Path=/; SameSite=None; Partitioned;");
  SetCookieFromJS(iframe, "name=3PC; Secure; SameSite=None;");

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view = GetViewByIdentifierAtIndex(
      context, kPageSpecificSiteDataDialogRowForTesting, /*index=*/1);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // Both third-party and partitioned cookies are allowed access.
  // TODO(crbug.com/1344787): The label shouldn't be visible here but GetVisible
  // returns true. It's not actually visible because it has size 0.
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Allowed");
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Blocked");
  // TODO(crbug.com/1344787): Check the histograms value.
}

IN_PROC_BROWSER_TEST_P(PageSpecificSiteDataDialogBrowserTest,
                       MixedAccessPartitionedAndBlockedThirdParty) {
  if (!GetParam()) {
    return;
  }

  // Block third-party cookies.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  base::RunLoop().RunUntilIdle();

  content::RenderFrameHost* main_frame = GetMainFrame();
  content::RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);
  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  // Set one partitioned third party cookie and one
  // regular third party cookie.
  SetCookieFromJS(iframe,
                  "__Host-name=PartitionedCookie; Secure; Path=/; "
                  "SameSite=None; Partitioned;");
  SetCookieFromJS(iframe, "name=3PCookie; Secure; SameSite=None;");

  auto* dialog = OpenDialog();
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(dialog);

  auto* view = GetViewByIdentifierAtIndex(
      context, kPageSpecificSiteDataDialogRowForTesting, /*index=*/1);
  auto* row_view = static_cast<SiteDataRowView*>(view);
  // Third party cookie ("3PCookie") is blocked by third-party cookie blocking
  // perf, partitioned cookie ("PartitionedCookie") isn't because it is treated
  // as a first party. The state is "Only using partitioned storage" as other
  // types of storage are not used or blocked from access.
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(),
            u"Only using partitioned storage");
  ClickButton(row_view->menu_button_for_testing());
  // TODO(crbug.com/1344787): Use the actual menu to perform action. Check if
  // correct menu item are displayed.
  ClickBlockMenuItem(row_view);
  EXPECT_TRUE(row_view->state_label_for_testing()->GetVisible());
  EXPECT_EQ(row_view->state_label_for_testing()->GetText(), u"Blocked");
  // TODO(crbug.com/1344787): Check the histograms value.
}

// Run tests with kPageSpecificSiteDataDialog flag enabled and disabled.
INSTANTIATE_TEST_SUITE_P(All,
                         PageSpecificSiteDataDialogBrowserTest,
                         ::testing::Values(false, true));
