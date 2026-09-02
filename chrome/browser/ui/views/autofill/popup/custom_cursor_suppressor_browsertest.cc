// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"

#include <vector>

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

using ::content::GlobalRenderFrameHostId;
using ::testing::Contains;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class CustomCursorSuppressorBrowserTest : public InProcessBrowserTest {
 public:
  CustomCursorSuppressorBrowserTest() = default;
  ~CustomCursorSuppressorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetUrl1() {
    return embedded_test_server()->GetURL("a.com", "/title1.html");
  }
  GURL GetUrl2() {
    return embedded_test_server()->GetURL("b.com", "/title2.html");
  }
  GURL GetUrl3() {
    return embedded_test_server()->GetURL("c.com", "/title3.html");
  }

  [[nodiscard]] bool AddTab(BrowserWindowInterface* browser, const GURL& url) {
    return ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  [[nodiscard]] bool AddBackgroundTab(BrowserWindowInterface* browser,
                                      const GURL& url) {
    return ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  GlobalRenderFrameHostId GetRfhIdOfActiveWebContents(
      BrowserWindowInterface& browser) {
    return browser.GetTabStripModel()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame()
        ->GetGlobalId();
  }
};

// Tests that custom cursor suppression works for a single browser window with a
// single tab.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest,
                       SingleBrowserSingleTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));

  CustomCursorSuppressor suppressor;
  EXPECT_FALSE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));

  suppressor.Start();
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  suppressor.Stop();
  EXPECT_FALSE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
}

// Tests that a navigation that results in a different `RenderFrameHost` for the
// tab still maintains a suppressed custom cursor.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest,
                       SingleBrowserSingleTabWithNavigationToDifferentOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));

  CustomCursorSuppressor suppressor;
  suppressor.Start();

  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};

  // Simulate a navigation to a different origin.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl2()));
  EXPECT_NE(GetRfhIdOfActiveWebContents(*browser()),
            expected_suppressed_ids.front());
  expected_suppressed_ids.push_back(GetRfhIdOfActiveWebContents(*browser()));
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));
}

// Tests that custom cursor suppression reacts to active tab changes in a single
// browser window.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest,
                       SingleBrowserWithTabChange) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  ASSERT_TRUE(AddTab(browser(), GetUrl2()));
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl2());

  CustomCursorSuppressor suppressor;
  suppressor.Start();

  std::vector<GlobalRenderFrameHostId> expected_suppressed_ids = {
      GetRfhIdOfActiveWebContents(*browser())};
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));

  // Activating the tab with `GetUrl1()` (at index 0) adds a new suppression
  // scope.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl1());
  expected_suppressed_ids.push_back(GetRfhIdOfActiveWebContents(*browser()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));

  // Switching back to the previously focused tab does not add another
  // suppression scope, since one already exists.
  browser()->GetTabStripModel()->ActivateTabAt(1);
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl2());
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAreArray(expected_suppressed_ids));
}

// Tests that custom cursor suppression reacts to new foreground tabs that are
// added to a single browser window.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest,
                       SingleBrowserWithForegroundTabAddition) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl1());

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  // Adding a new tab adds a new suppression scope.
  ASSERT_TRUE(AddTab(browser(), GetUrl2()));
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl2());
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              Contains(GetRfhIdOfActiveWebContents(*browser())));
}

// Tests that custom cursor suppression does not react to a tab that is added to
// the background.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest,
                       SingleBrowserWithBackgroundTabAddition) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl1());

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  // Adding a new background tab does not lead to a new suppression scope.
  ASSERT_TRUE(AddBackgroundTab(browser(), GetUrl2()));
  ASSERT_THAT(
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL(),
      GetUrl1());
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));
}

// Tests that custom cursor suppression scopes are created for all active tabs
// in all active browser windows.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest, MultipleBrowsers) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  ASSERT_TRUE(AddTab(browser(), GetUrl2()));

  // Set up a second browser window with a loaded tab.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 2u);
  ASSERT_TRUE(AddTab(browser2, GetUrl3()));

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser2->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser()),
                                   GetRfhIdOfActiveWebContents(*browser2)));
}

// Tests that a new custom cursor suppression scope is created on browser window
// creation.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorBrowserTest, BrowserAddition) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));

  CustomCursorSuppressor suppressor;
  suppressor.Start();
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              UnorderedElementsAre(GetRfhIdOfActiveWebContents(*browser())));

  // Open a second browser window while the suppression is already on.
  BrowserWindowInterface* browser2 = CreateBrowser(browser()->GetProfile());
  ASSERT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(), 2u);
  ASSERT_TRUE(AddTab(browser2, GetUrl2()));
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser()->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_TRUE(suppressor.IsSuppressing(
      *browser2->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              Contains(GetRfhIdOfActiveWebContents(*browser())));
  EXPECT_THAT(suppressor.SuppressedRenderFrameHostIdsForTesting(),
              Contains(GetRfhIdOfActiveWebContents(*browser2)));

  suppressor.Stop();
}

class CustomCursorSuppressorExtensionBrowserTest
    : public extensions::ExtensionBrowserTest {
 protected:
  // Installs an extension and shows it in its side panel.
  scoped_refptr<const extensions::Extension> LoadExtensionInSidePanel() {
    scoped_refptr<const extensions::Extension> extension = LoadExtension(
        test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
    CHECK(extension);
    SidePanelEntry::Key extension_key =
        SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id());
    SidePanelEntry* const entry =
        SidePanelRegistry::From(browser())->GetEntryForKey(extension_key);
    CHECK(entry);

    ExtensionTestMessageListener default_path_listener("default_path");
    SidePanelUI* const side_panel_ui = SidePanelUI::From(browser());
    side_panel_ui->Show(extension_key);
    CHECK(default_path_listener.WaitUntilSatisfied());
    CHECK(side_panel_ui->IsSidePanelShowing());
    return extension;
  }
};

// Tests that starting custom cursor suppression disables custom cursors in
// extension `WebContents` objects that were created before the suppressor is
// started.
IN_PROC_BROWSER_TEST_F(CustomCursorSuppressorExtensionBrowserTest,
                       SuppressionWorksForAlreadyLoadedExtensions) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionInSidePanel();
  auto* extension_coordinator =
      browser()
          ->GetFeatures()
          .extension_side_panel_manager()
          ->GetExtensionCoordinatorForTesting(extension->id());
  content::WebContents* host_contents =
      extension_coordinator->GetHostWebContentsForTesting();

  CustomCursorSuppressor suppressor;
  EXPECT_FALSE(suppressor.IsSuppressing(*host_contents));
  suppressor.Start();
  EXPECT_TRUE(suppressor.IsSuppressing(*host_contents));
}

// Tests that starting custom cursor suppression disables custom cursors in
// extensions `WebContents` objects that are created after the suppressor
// is started.
IN_PROC_BROWSER_TEST_F(
    CustomCursorSuppressorExtensionBrowserTest,
    SuppressionWorksForExtensionsLoadedAfterSuppressorStart) {
  CustomCursorSuppressor suppressor;
  suppressor.Start();

  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionInSidePanel();
  auto* extension_coordinator =
      browser()
          ->GetFeatures()
          .extension_side_panel_manager()
          ->GetExtensionCoordinatorForTesting(extension->id());
  content::WebContents* host_contents =
      extension_coordinator->GetHostWebContentsForTesting();
  EXPECT_TRUE(suppressor.IsSuppressing(*host_contents));
}

}  // namespace
