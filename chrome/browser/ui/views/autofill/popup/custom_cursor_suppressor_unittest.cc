// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"

#include <memory>
#include <string_view>
#include <vector>

#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "url/gurl.h"

namespace {

using ::content::GlobalRenderFrameHostId;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

constexpr std::string_view kUrl1 = "https://www.foo.com";
constexpr std::string_view kUrl2 = "https://www.bar.com";
constexpr std::string_view kUrl3 = "https://www.doe.com";

class CustomCursorSuppressorTest : public BrowserWithTestWindowTest {
 public:
  CustomCursorSuppressorTest() = default;
  ~CustomCursorSuppressorTest() override = default;

  void AddBackgroundTab(Browser* browser, const GURL& url) {
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_TYPED);
    params.tabstrip_index = 0;
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    Navigate(&params);
    CommitPendingLoad(&params.navigated_or_inserted_contents->GetController());
  }

  GlobalRenderFrameHostId GetRfhIdOfActiveWebContents(Browser& browser) {
    return browser.tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame()
        ->GetGlobalId();
  }
};

// Tests that custom cursor suppression works for a single browser window with a
// single tab.
TEST_F(CustomCursorSuppressorTest, SingleBrowserSingleTab) {
  AddTab(browser(), GURL(kUrl1));

  CustomCursorSuppressor suppressor;
  EXPECT_FALSE(suppressor.IsSuppressing(
      *browser()->tab_strip_model()->GetActiveWebContents()));

  suppressor.Start();
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  suppressor.Stop();
  EXPECT_FALSE(suppressor.IsSuppressing(
      *browser()->tab_strip_model()->GetActiveWebContents()));
}

// Tests that a navigation that results in a different `RenderFrameHost` for the
// still maintains a suppressed custom cursor.
TEST_F(CustomCursorSuppressorTest,
       SingleBrowserSingleTabWithNavigationToDifferentOrigin) {
  AddTab(browser(), GURL(kUrl1));

  CustomCursorSuppressor suppressor;
  suppressor.Start();

  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->tab_strip_model()->GetActiveWebContents()));
  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};

  // Simulate a navigation to a different origin.
  NavigateAndCommitActiveTab(GURL(kUrl2));
  EXPECT_NE(GetRfhIdOfActiveWebContents(*browser()),
            expected_suppressed_ids.front());
  expected_suppressed_ids.push_back(GetRfhIdOfActiveWebContents(*browser()));
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));
}

// Tests that custom cursor suppression reacts to active tab changes in a single
// browser window.
TEST_F(CustomCursorSuppressorTest, SingleBrowserWithTabChange) {
  AddTab(browser(), GURL(kUrl1));
  AddTab(browser(), GURL(kUrl2));
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl2));

  CustomCursorSuppressor suppressor;
  suppressor.Start();

  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));

  // Activating the tab with `kUrl1` (now at index 1) adds a new suppression
  // scope.
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl1));
  expected_suppressed_ids.push_back(GetRfhIdOfActiveWebContents(*browser()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));

  // Switching back to the previously focused tab does not add another
  // suppression scope, since one already exists.
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl2));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));
}

// Tests that custom cursor suppression reacts to new foreground tabs that are
// added to a single browser window.
TEST_F(CustomCursorSuppressorTest, SingleBrowserWithForegroundTabAddition) {
  AddTab(browser(), GURL(kUrl1));
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl1));

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));

  // Adding a new tab adds a new suppression scope.
  AddTab(browser(), GURL(kUrl2));
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl2));
  expected_suppressed_ids.push_back(GetRfhIdOfActiveWebContents(*browser()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));
}

// Tests that custom cursor suppression does not react to a tab that is added to
// the background.
TEST_F(CustomCursorSuppressorTest, SingleBrowserWithBackgroundTabAddition) {
  AddTab(browser(), GURL(kUrl1));
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl1));

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  // Adding a new background tab does not lead to a new suppression scope.
  AddBackgroundTab(browser(), GURL(kUrl2));
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(kUrl1));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));
}

// Tests that custom cursor suppression scopes are created for all active tabs
// in all active browser windows.
TEST_F(CustomCursorSuppressorTest, MultipleBrowsers) {
  AddTab(browser(), GURL(kUrl1));
  AddTab(browser(), GURL(kUrl2));
  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};

  // Set up a second browser window.
  Browser::CreateParams native_params(profile(), true);
  native_params.initial_show_state = ui::mojom::WindowShowState::kNormal;
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  ASSERT_THAT(*BrowserList::GetInstance(), SizeIs(2));
  AddTab(browser2.get(), GURL(kUrl3));

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser()),
                                   GetRfhIdOfActiveWebContents(*browser2)));

  // All tabs must be closed prior to browser destruction.
  browser2->tab_strip_model()->CloseAllTabs();
}

// Tests that a new custom cursor suppression scope is created on browser window
// creation.
TEST_F(CustomCursorSuppressorTest, BrowserAddition) {
  AddTab(browser(), GURL(kUrl1));
  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  // Open a second browser window while the suppression is already on.
  Browser::CreateParams native_params(profile(), true);
  native_params.initial_show_state = ui::mojom::WindowShowState::kNormal;
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  ASSERT_THAT(*BrowserList::GetInstance(), SizeIs(2));
  AddTab(browser2.get(), GURL(kUrl2));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser()),
                                   GetRfhIdOfActiveWebContents(*browser2)));

  suppressor.Stop();

  // All tabs must be closed prior to browser destruction.
  browser2->tab_strip_model()->CloseAllTabs();
}

}  // namespace
