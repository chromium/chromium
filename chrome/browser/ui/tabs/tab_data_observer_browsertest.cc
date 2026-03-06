// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_data_observer.h"

#include <memory>
#include <string>

#include "base/byte_size.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_sharing/public/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/url_constants.h"

namespace tabs {

class TabDataObserverBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void UpdateTitleForEntry(content::WebContents* contents,
                           const std::u16string& title) {
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry);
    contents->UpdateTitleForEntry(entry, title);
  }

 private:
};

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, DefaultTabData) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();

  EXPECT_FALSE(data.pinned);
  EXPECT_TRUE(data.should_display_favicon);
  EXPECT_EQ(data.network_state, TabNetworkState::kNone);
  EXPECT_FALSE(data.alert_state.has_value());
  EXPECT_EQ(data.visible_url, GURL(url::kAboutBlankURL));
  EXPECT_EQ(data.title, TabUIHelper::From(tab_interface)->GetTitle());
  EXPECT_FALSE(data.blocked);
  EXPECT_FALSE(data.should_hide_throbber);
  EXPECT_FALSE(data.should_show_discard_status);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, PinnedStateChange) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_FALSE(data.pinned);

  tab_strip_model->SetTabPinned(0, true);
  EXPECT_TRUE(data.pinned);

  tab_strip_model->SetTabPinned(0, false);
  EXPECT_FALSE(data.pinned);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, TitleChange) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* const web_contents =
      tab_strip_model->GetWebContentsAt(0);

  UpdateTitleForEntry(web_contents, u"First Tab");
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_EQ(data.title, u"First Tab");

  UpdateTitleForEntry(web_contents, u"First Tab Updated");
  EXPECT_EQ(data.title, u"First Tab Updated");
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, BlockedState) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  // Initially not blocked
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_FALSE(data.blocked);

  // Block the tab and verify
  tab_strip_model->SetTabBlocked(0, true);
  EXPECT_TRUE(data.blocked);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, FaviconAndIconFlags) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  {  // Initial favicon data matches default
    TabInterface* const tab_interface = tab_strip_model->GetTabAtIndex(0);
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
    const TabData& data = tab_data_observer->tab_data();
    EXPECT_EQ(data.favicon, TabUIHelper::From(tab_interface)->GetFavicon());
    EXPECT_FALSE(data.should_themify_favicon);
    EXPECT_FALSE(data.is_monochrome_favicon);
    EXPECT_TRUE(data.should_display_favicon);
  }

  {  // Themeable by virtual URL only.
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    TabInterface* const tab_interface = tab_strip_model->GetTabAtIndex(1);
    content::WebContents* const wc_virtual = tab_interface->GetContents();
    auto* entry_virtual = wc_virtual->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry_virtual);
    const GURL themeable_virtual_url("chrome://feedback/");
    entry_virtual->SetVirtualURL(themeable_virtual_url);
    auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
    const TabData& virtual_data = tab_data_observer->tab_data();
    EXPECT_TRUE(virtual_data.should_themify_favicon);
  }

  {  // Themeable by actual URL only.
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    TabInterface* const tab_interface = tab_strip_model->GetTabAtIndex(2);
    content::WebContents* const wc_actual = tab_interface->GetContents();
    auto* entry_actual = wc_actual->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry_actual);
    const GURL themeable_url("chrome://new-tab-page/");
    entry_actual->SetURL(themeable_url);
    auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
    const TabData& actual_data = tab_data_observer->tab_data();
    EXPECT_TRUE(actual_data.should_themify_favicon);
  }
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, Urls) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  const GURL kUrl("http://example.com/");
  entry->SetURL(kUrl);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_EQ(data.visible_url, kUrl);
  EXPECT_TRUE(data.should_display_url);
  EXPECT_FALSE(data.should_render_loading_title);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest,
                       DomainUrlHiddenForNtpAndTabSearch) {
  struct TestCase {
    GURL url;
    bool expected_domain_visible;
  };

  TestCase test_cases[] = {
      {GURL("https://example.com"), true},
      {GURL(chrome::kChromeUIVersionURL), true},
      {GURL(chrome::kChromeUINewTabURL), false},
      {GURL(chrome::kChromeUINewTabPageURL), false},
      {GURL(chrome::kChromeUITabSearchURL), false},
  };

  for (const auto& test_case : test_cases) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), test_case.url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    TabInterface* const tab_interface =
        browser()->GetTabStripModel()->GetTabAtIndex(0);
    auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
    const TabData& data = tab_data_observer->tab_data();
    EXPECT_EQ(data.should_display_url, test_case.expected_domain_visible)
        << test_case.url;
  }
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, UncomittedNavigationUrl) {
  // The /nocontent navigation won't commit so it reverts back to the initial
  // navigation.
  GURL uncommitted_url = embedded_test_server()->GetURL("c.test", "/nocontent");
  ASSERT_FALSE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), uncommitted_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* wc = tab_strip_model->GetWebContentsAt(1);
  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry->IsInitialEntry());
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_EQ(data.visible_url, GURL(url::kAboutBlankURL));
  EXPECT_TRUE(data.should_display_url);
  EXPECT_FALSE(data.should_render_loading_title);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, ShouldRenderLoadingTitle) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  UpdateTitleForEntry(wc, u"");

  auto* entry = wc->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);
  const GURL kUntrustedUrl("chrome-untrusted://test/");
  entry->SetURL(kUntrustedUrl);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_FALSE(data.should_render_loading_title);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, CrashedStatus) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_FALSE(data.is_crashed);
  content::CrashTab(wc);
  EXPECT_TRUE(data.is_crashed);

  // The tab should not be crashed after navigating to another site.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(data.is_crashed);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, AlertStateAudioPlaying) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(0);
  content::WebContents* wc = tab_interface->GetContents();
  base::ScopedClosureRunner scoped_closure_runner = wc->MarkAudible();
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_TRUE(data.alert_state.has_value());
  EXPECT_EQ(data.alert_state.value(), TabAlert::kAudioPlaying);
}

IN_PROC_BROWSER_TEST_F(TabDataObserverBrowserTest, ShouldHideThrobber) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  TabInterface* const tab_interface =
      browser()->GetTabStripModel()->GetTabAtIndex(1);
  TabUIHelper* const helper = TabUIHelper::From(tab_interface);
  ASSERT_NE(nullptr, helper);
  helper->SetCreatedBySessionRestore(true);
  auto tab_data_observer = std::make_unique<TabDataObserver>(tab_interface);
  const TabData& data = tab_data_observer->tab_data();
  EXPECT_TRUE(helper->ShouldHideThrobber());
  EXPECT_TRUE(data.should_hide_throbber);
}
}  // namespace tabs
