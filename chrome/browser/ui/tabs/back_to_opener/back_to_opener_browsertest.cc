// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace back_to_opener {

class BackToOpenerBrowserTest : public InProcessBrowserTest {
 public:
  BackToOpenerBrowserTest() {
    feature_list_.InitAndEnableFeature(tabs::kBackToOpener);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  base::test::ScopedFeatureList feature_list_;
};

// User clicks link in opener to open in new tab, back button should be enabled
// and clicking it should close destination and focus opener.
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest, BasicBackToOpener) {
  // Navigate opener to a page with a link
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int opener_tab_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(opener_contents);

  // Wait for opener page to load
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Click the link to open in new tab
  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, nullptr);
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  ASSERT_NE(dest_tab, nullptr);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();
  ASSERT_NE(controller, nullptr);

  // Verify relationship is established
  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  // Verify back button is enabled in UI
  EXPECT_TRUE(browser()->command_controller()->IsCommandEnabled(IDC_BACK));

  // Verify the histogram for destination tab close duration
  base::HistogramTester histogram_tester;

  content::WebContentsDestroyedWatcher close_watcher(dest_contents);
  chrome::ExecuteCommand(browser(), IDC_BACK);
  close_watcher.Wait();

  // Verify the histograms were recorded
  histogram_tester.ExpectTotalCount("Navigation.BackToOpener.Clicked", 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.BackToOpener.DestinationTabCloseDuration", 1);

  // Verify opener is focused
  EXPECT_EQ(
      opener_tab_index,
      browser()->tab_strip_model()->GetIndexOfWebContents(opener_contents));
  EXPECT_EQ(opener_contents,
            browser()->tab_strip_model()->GetActiveWebContents());
}

// Opener closed should disable back button
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest,
                       OpenerClosedDisablesBackButton) {
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();

  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  // Close opener tab
  int opener_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(opener_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(opener_index,
                                                   TabCloseTypes::CLOSE_NONE);

  // Back button should be disabled
  EXPECT_FALSE(controller->HasValidOpener());
  EXPECT_FALSE(controller->CanGoBackToOpener());
  EXPECT_FALSE(browser()->command_controller()->IsCommandEnabled(IDC_BACK));
}

// Opener navigated away should disable back button
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest,
                       OpenerNavigatedAwayDisablesBackButton) {
  // Set up opener and destination
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();

  // Verify relationship exists before navigation
  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  // Switch back to opener tab
  int opener_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(opener_contents);
  browser()->tab_strip_model()->ActivateTabAt(opener_index);

  // Navigate opener to different site
  GURL new_opener_url =
      embedded_test_server()->GetURL("other.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_opener_url));

  // Switch back to destination tab
  int dest_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(dest_contents);
  browser()->tab_strip_model()->ActivateTabAt(dest_index);

  // Back button should be disabled
  EXPECT_FALSE(controller->HasValidOpener());
  EXPECT_FALSE(controller->CanGoBackToOpener());
}

// Opener gone but destination has navigation should keep
// back button enabled for navigation
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest,
                       OpenerGoneWithDestinationNavigationKeepsBackEnabled) {
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  // Navigate destination
  GURL dest_url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), dest_url2));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();

  // Close opener
  int opener_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(opener_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(opener_index,
                                                   TabCloseTypes::CLOSE_NONE);

  // Back button should still be enabled (has navigation history)
  EXPECT_FALSE(controller->HasValidOpener());
  EXPECT_TRUE(dest_contents->GetController().CanGoBack());

  EXPECT_TRUE(browser()->command_controller()->IsCommandEnabled(IDC_BACK));
}

// Pinned tab should disable back button but maintain relationship
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest, PinnedTabDisablesBackButton) {
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();

  // Verify relationship exists before pinning
  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  // Pin the destination tab.
  int dest_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(dest_contents);
  browser()->tab_strip_model()->SetTabPinned(dest_index, true);

  // Back button should be disabled for pinned tabs
  EXPECT_FALSE(controller->CanGoBackToOpener());
  // But relationship should be maintained
  EXPECT_TRUE(controller->HasValidOpener());

  int pinned_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(dest_contents);
  browser()->tab_strip_model()->SetTabPinned(pinned_index, false);

  // Back button should be enabled again
  EXPECT_TRUE(controller->CanGoBackToOpener());
  EXPECT_TRUE(controller->HasValidOpener());
}

// Test that back button works even if opener is moved to another window
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest, OpenerMovedToAnotherWindow) {
  // Set up opener and destination
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();

  EXPECT_TRUE(controller->HasValidOpener());

  // Move opener to new window
  int opener_index =
      browser()->tab_strip_model()->GetIndexOfWebContents(opener_contents);
  std::unique_ptr<content::WebContents> detached =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(
          opener_index);
  Browser* new_browser =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  new_browser->tab_strip_model()->InsertWebContentsAt(0, std::move(detached),
                                                      AddTabTypes::ADD_ACTIVE);

  // Back button should still work
  EXPECT_TRUE(controller->HasValidOpener());
  EXPECT_TRUE(controller->CanGoBackToOpener());

  content::WebContentsDestroyedWatcher close_watcher(dest_contents);
  chrome::ExecuteCommand(browser(), IDC_BACK);
  close_watcher.Wait();

  // Verify opener in new window is active
  EXPECT_EQ(new_browser->tab_strip_model()->GetActiveWebContents(),
            opener_contents);
}

// Test that tabs opened without an opener relationship don't have
// back-to-opener functionality
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest,
                       TabOpenedWithoutOpenerNoRelationship) {
  // Open a new tab directly (not from a link click)
  chrome::NewTab(browser());
  content::WebContents* new_tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate the new tab to a page
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* new_tab =
      tabs::TabInterface::GetFromContents(new_tab_contents);
  ASSERT_NE(new_tab, nullptr);
  BackToOpenerController* controller =
      new_tab->GetTabFeatures()->back_to_opener_controller();
  ASSERT_NE(controller, nullptr);

  // Verify no relationship exists
  EXPECT_FALSE(controller->HasValidOpener());
  EXPECT_FALSE(controller->CanGoBackToOpener());
}

// Test that back-to-opener menu item appears correctly in the back menu
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest, BackToOpenerMenuAppears) {
  // Navigate opener to a page with a link
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Click the link to open in new tab
  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));
  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  ASSERT_NE(dest_tab, nullptr);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();
  ASSERT_NE(controller, nullptr);

  // Verify relationship is established
  EXPECT_TRUE(controller->HasValidOpener());

  // Create BackForwardMenuModel to test menu structure
  BackForwardMenuModel back_model(browser(),
                                  BackForwardMenuModel::ModelType::kBackward);

  // Verify back-to-opener section exists
  EXPECT_TRUE(
      back_model.HasSection(BackForwardMenuModel::MenuSection::kBackToOpener));
  EXPECT_EQ(1u, back_model.GetSectionItemCount(
                    BackForwardMenuModel::MenuSection::kBackToOpener));

  // With no history, menu should have: Back To Opener -> Separator -> Show Full
  // History
  EXPECT_EQ(3u, back_model.GetItemCount());

  // Verify section positions
  std::optional<size_t> back_to_opener_start =
      back_model.GetStartingIndexOfSection(
          BackForwardMenuModel::MenuSection::kBackToOpener);
  EXPECT_TRUE(back_to_opener_start.has_value());
  EXPECT_EQ(0u, back_to_opener_start.value());

  std::optional<size_t> show_full_history_start =
      back_model.GetStartingIndexOfSection(
          BackForwardMenuModel::MenuSection::kShowFullHistory);
  EXPECT_TRUE(show_full_history_start.has_value());
  EXPECT_EQ(2u, show_full_history_start.value());
  // Show Full History should always be the last item in the menu
  EXPECT_EQ(back_model.GetItemCount() - 1, show_full_history_start.value());

  // Verify section types at each index
  EXPECT_EQ(BackForwardMenuModel::MenuSection::kBackToOpener,
            back_model.GetSectionForIndex(0).value());
  EXPECT_EQ(BackForwardMenuModel::MenuSection::kSeparator,
            back_model.GetSectionForIndex(1).value());
  EXPECT_EQ(BackForwardMenuModel::MenuSection::kShowFullHistory,
            back_model.GetSectionForIndex(2).value());

  // Verify back-to-opener label
  std::u16string opener_title =
      BackToOpenerController::GetFormattedOpenerTitle(dest_contents);
  EXPECT_EQ(opener_title, back_model.GetLabelAt(0));

  // Verify it's enabled and not a separator
  EXPECT_FALSE(back_model.IsSeparator(0));
  EXPECT_TRUE(back_model.IsEnabledAt(0));
}

// Test that back-to-opener menu item appears after history items
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest,
                       BackToOpenerMenuWithNavigationHistory) {
  // Navigate opener to a page with a link
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Click the link to open in new tab
  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));

  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, nullptr);
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  // Navigate destination to create navigation history
  GURL dest_url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), dest_url2));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  ASSERT_NE(dest_tab, nullptr);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();
  ASSERT_NE(controller, nullptr);

  // Verify relationship is established
  EXPECT_TRUE(controller->HasValidOpener());

  // Create BackForwardMenuModel to test menu structure
  BackForwardMenuModel back_model(browser(),
                                  BackForwardMenuModel::ModelType::kBackward);

  // Verify sections exist
  EXPECT_TRUE(
      back_model.HasSection(BackForwardMenuModel::MenuSection::kHistory));
  EXPECT_TRUE(
      back_model.HasSection(BackForwardMenuModel::MenuSection::kBackToOpener));
  EXPECT_TRUE(back_model.HasSection(
      BackForwardMenuModel::MenuSection::kShowFullHistory));

  // Verify section item counts
  size_t history_count = back_model.GetSectionItemCount(
      BackForwardMenuModel::MenuSection::kHistory);
  EXPECT_GT(history_count, 0u);
  EXPECT_EQ(1u, back_model.GetSectionItemCount(
                    BackForwardMenuModel::MenuSection::kBackToOpener));
  EXPECT_EQ(1u, back_model.GetSectionItemCount(
                    BackForwardMenuModel::MenuSection::kShowFullHistory));

  // Verify menu order: History -> Back To Opener -> Separator -> Show Full
  // History
  std::optional<size_t> history_start = back_model.GetStartingIndexOfSection(
      BackForwardMenuModel::MenuSection::kHistory);
  EXPECT_TRUE(history_start.has_value());
  EXPECT_EQ(0u, history_start.value());

  std::optional<size_t> back_to_opener_start =
      back_model.GetStartingIndexOfSection(
          BackForwardMenuModel::MenuSection::kBackToOpener);
  EXPECT_TRUE(back_to_opener_start.has_value());
  // Back-to-opener should come after history items
  EXPECT_EQ(history_count, back_to_opener_start.value());

  std::optional<size_t> show_full_history_start =
      back_model.GetStartingIndexOfSection(
          BackForwardMenuModel::MenuSection::kShowFullHistory);
  EXPECT_TRUE(show_full_history_start.has_value());
  // Show Full History should be after back-to-opener and separator
  EXPECT_EQ(history_count + 2, show_full_history_start.value());
  // Show Full History should always be the last item in the menu
  EXPECT_EQ(back_model.GetItemCount() - 1, show_full_history_start.value());

  // Verify section types at key positions
  EXPECT_EQ(BackForwardMenuModel::MenuSection::kHistory,
            back_model.GetSectionForIndex(0).value());
  EXPECT_EQ(
      BackForwardMenuModel::MenuSection::kBackToOpener,
      back_model.GetSectionForIndex(back_to_opener_start.value()).value());
  EXPECT_EQ(BackForwardMenuModel::MenuSection::kSeparator,
            back_model.GetSectionForIndex(show_full_history_start.value() - 1)
                .value());
  EXPECT_EQ(
      BackForwardMenuModel::MenuSection::kShowFullHistory,
      back_model.GetSectionForIndex(show_full_history_start.value()).value());

  // Verify back-to-opener label
  std::u16string opener_title =
      BackToOpenerController::GetFormattedOpenerTitle(dest_contents);
  EXPECT_EQ(opener_title, back_model.GetLabelAt(back_to_opener_start.value()));

  // Verify back-to-opener item is enabled and not a separator
  EXPECT_FALSE(back_model.IsSeparator(back_to_opener_start.value()));
  EXPECT_TRUE(back_model.IsEnabledAt(back_to_opener_start.value()));
}

// Test that back-to-opener entry appears correctly even without history items.
IN_PROC_BROWSER_TEST_F(BackToOpenerBrowserTest,
                       BackToOpenerMenuOrderNoHistory) {
  // Navigate opener to a page with a link
  GURL opener_url =
      embedded_test_server()->GetURL("/back_to_opener_opener.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), opener_url));
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(opener_contents));

  // Wait for new tab to open
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Click the link to open in new tab
  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "document.getElementById('link').click();"));
  content::WebContents* dest_contents = tab_waiter.Wait();
  ASSERT_NE(dest_contents, nullptr);
  ASSERT_NE(dest_contents, opener_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dest_contents));

  tabs::TabInterface* dest_tab =
      tabs::TabInterface::GetFromContents(dest_contents);
  ASSERT_NE(dest_tab, nullptr);
  BackToOpenerController* controller =
      dest_tab->GetTabFeatures()->back_to_opener_controller();
  ASSERT_NE(controller, nullptr);

  // Verify relationship is established
  EXPECT_TRUE(controller->HasValidOpener());

  // Create BackForwardMenuModel to test menu structure
  BackForwardMenuModel back_model(browser(),
                                  BackForwardMenuModel::ModelType::kBackward);

  // With back-to-opener but no history, we should have:
  // Back To Opener -> Separator -> Show Full History
  EXPECT_TRUE(
      back_model.HasSection(BackForwardMenuModel::MenuSection::kBackToOpener));
  EXPECT_EQ(3u, back_model.GetItemCount());

  // Verify positions
  std::optional<size_t> back_to_opener_start =
      back_model.GetStartingIndexOfSection(
          BackForwardMenuModel::MenuSection::kBackToOpener);
  EXPECT_TRUE(back_to_opener_start.has_value());
  EXPECT_EQ(0u, back_to_opener_start.value());

  EXPECT_TRUE(back_model.IsSeparator(1));
  EXPECT_EQ(BackForwardMenuModel::MenuSection::kShowFullHistory,
            back_model.GetSectionForIndex(2).value());
}

}  // namespace back_to_opener
