// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include <algorithm>
#include <memory>

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

using BrowserListTest = BrowserWithTestWindowTest;

namespace {

// Helper function to iterate and count all the tabs.
size_t CountAllTabs() {
  size_t count = 0;
  tabs::ForEachTabInterface([&count](tabs::TabInterface* tab) {
    ++count;
    return true;
  });
  return count;
}

}  // namespace

TEST_F(BrowserListTest, TabContentsIteratorVerifyCount) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  EXPECT_EQ(0U, CountAllTabs());

  // Create more browsers/windows.
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  // Create browser 3 and 4 on the Ash desktop (the iterator shouldn't see the
  // difference).
  Browser::CreateParams ash_params(profile(), true);
  std::unique_ptr<Browser> browser3(
      CreateBrowserWithTestWindowForParams(ash_params));
  std::unique_ptr<Browser> browser4(
      CreateBrowserWithTestWindowForParams(ash_params));

  // Sanity checks.
  EXPECT_EQ(4U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser2->tab_strip_model()->count());
  EXPECT_EQ(0, browser3->tab_strip_model()->count());
  EXPECT_EQ(0, browser4->tab_strip_model()->count());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i) {
    chrome::NewTab(browser2.get());
  }
  chrome::NewTab(browser3.get());

  EXPECT_EQ(4U, CountAllTabs());

  // Close some tabs.
  browser2->tab_strip_model()->CloseAllTabs();

  EXPECT_EQ(1U, CountAllTabs());

  // Add lots of tabs.
  for (size_t i = 0; i < 41; ++i) {
    chrome::NewTab(browser());
  }

  EXPECT_EQ(42U, CountAllTabs());
  // Close all remaining tabs to keep all the destructors happy.
  browser3->tab_strip_model()->CloseAllTabs();
}

TEST_F(BrowserListTest, TabContentsIteratorVerifyBrowser) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());

  // Create more browsers/windows.
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForParams(native_params));
  // Create browser 3 on the Ash desktop (the iterator shouldn't see the
  // difference).
  Browser::CreateParams ash_params(profile(), true);
  std::unique_ptr<Browser> browser3(
      CreateBrowserWithTestWindowForParams(ash_params));

  // Sanity checks.
  EXPECT_EQ(3U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser2->tab_strip_model()->count());
  EXPECT_EQ(0, browser3->tab_strip_model()->count());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i) {
    chrome::NewTab(browser2.get());
  }
  for (size_t i = 0; i < 2; ++i) {
    chrome::NewTab(browser3.get());
  }

  absl::flat_hash_map<BrowserWindowInterface*, size_t> tab_counts;
  tabs::ForEachTabInterface([&tab_counts](tabs::TabInterface* const tab) {
    ++tab_counts[tab->GetBrowserWindowInterface()];
    return true;
  });
  EXPECT_EQ(3u, tab_counts[browser2.get()]);
  EXPECT_EQ(2u, tab_counts[browser3.get()]);

  // Close some tabs.
  browser2->tab_strip_model()->CloseAllTabs();
  // This is normally invoked when the tab strip is empty (specifically from
  // BrowserView::OnWindowCloseRequested).
  browser2->OnWindowClosing();
  EXPECT_TRUE(browser2->is_delete_scheduled());
  browser3->tab_strip_model()->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);

  tab_counts.clear();
  tabs::ForEachTabInterface([&tab_counts](tabs::TabInterface* const tab) {
    ++tab_counts[tab->GetBrowserWindowInterface()];
    return true;
  });
  EXPECT_EQ(1u, tab_counts.size());
  EXPECT_EQ(1u, tab_counts[browser3.get()]);

  // Add one tab back to browser.
  chrome::NewTab(browser());

  tab_counts.clear();
  tabs::ForEachTabInterface([&tab_counts](tabs::TabInterface* const tab) {
    ++tab_counts[tab->GetBrowserWindowInterface()];
    return true;
  });
  EXPECT_EQ(2u, tab_counts.size());
  EXPECT_EQ(1u, tab_counts[browser()]);
  EXPECT_EQ(1u, tab_counts[browser3.get()]);

  // Close all remaining tabs to keep all the destructors happy.
  browser2->tab_strip_model()->CloseAllTabs();
  browser3->tab_strip_model()->CloseAllTabs();
}
