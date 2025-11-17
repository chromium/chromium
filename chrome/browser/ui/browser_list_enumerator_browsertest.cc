// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list_enumerator.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BrowserListIteratorBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserListIteratorBrowserTest, BasicIterator) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  Browser* browser3 = CreateBrowser(browser()->profile());
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

  std::set<Browser*> visited;
  BrowserListEnumerator enumerator;
  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(browser1, browser2, browser3));
}

IN_PROC_BROWSER_TEST_F(BrowserListIteratorBrowserTest, IteratorWithInsertions) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Start to scan the list.
  constexpr bool kEnumerateNewBrowser = true;
  BrowserListEnumerator enumerator(kEnumerateNewBrowser);
  std::set<Browser*> visited;

  if (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  // Insert a browser while the list is scanned.
  Browser* browser3 = CreateBrowser(browser()->profile());

  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited,
              testing::UnorderedElementsAre(browser1, browser2, browser3));
}

IN_PROC_BROWSER_TEST_F(BrowserListIteratorBrowserTest,
                       IteratorWithSkipInsertions) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Start to scan the list.
  constexpr bool kEnumerateNewBrowser = false;
  BrowserListEnumerator enumerator(kEnumerateNewBrowser);
  std::set<Browser*> visited;

  if (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  // Insert a browser while the list is scanned.
  CreateBrowser(browser()->profile());

  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited, testing::UnorderedElementsAre(browser1, browser2));
}

IN_PROC_BROWSER_TEST_F(BrowserListIteratorBrowserTest, IteratorWithRemovals) {
  Browser* browser1 = browser();
  Browser* browser2 = CreateBrowser(browser()->profile());
  Browser* browser3 = CreateBrowser(browser()->profile());
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

  // Start to scan the list.
  BrowserListEnumerator enumerator;
  std::set<Browser*> visited;

  if (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  // Remove a browser while the list is scanned.
  CloseBrowserSynchronously(browser2);

  while (!enumerator.empty()) {
    visited.insert(enumerator.Next());
  }

  EXPECT_THAT(visited, testing::UnorderedElementsAre(browser1, browser3));
}
