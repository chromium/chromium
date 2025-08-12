// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/mojo_tree_builder.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tabs_api {
namespace {

class TabStripServiceMojoTreeBuilderBrowserTest : public InProcessBrowserTest {
 protected:
  TabStripModel* GetTabStripModel() { return browser()->tab_strip_model(); }
};

IN_PROC_BROWSER_TEST_F(TabStripServiceMojoTreeBuilderBrowserTest,
                       BuildMojoTree) {
  /**
   * Create the following scenario to test tree walking.
   * (c) => collection
   * (t) => tab
   *
   *          ------------(c) tab strip
   *         |             |
   *         |           unpinned
   *         |          /  |  \
   *         |         (t0) (c) (c)
   *       pinned            |   |  \
   *                      (t1) (t2) (t3)
   */
  // 0th is not needed, because tab strip model creates it by default.
  chrome::AddTabAt(browser(), GURL("1"), 1, false);
  chrome::AddTabAt(browser(), GURL("2"), 2, false);
  chrome::AddTabAt(browser(), GURL("3"), 3, false);

  GetTabStripModel()->AddToNewGroup({1});
  GetTabStripModel()->AddToNewGroup({2, 3});

  auto result = MojoTreeBuilder(GetTabStripModel()).Build();

  // First layer is just pinned/unpinned.
  // Pinned is idx 0, unpinned is idx1.
  ASSERT_EQ(2ul, result->elements.size());
  ASSERT_TRUE(result->elements.at(1)->is_tab_collection_container());

  auto unpinned =
      std::move(result->elements.at(1)->get_tab_collection_container());
  ASSERT_EQ(3ul, unpinned->elements.size());

  // First branch.
  ASSERT_TRUE(unpinned->elements.at(0)->is_tab_container());

  // Second branch.
  ASSERT_TRUE(unpinned->elements.at(1)->is_tab_collection_container());
  ASSERT_EQ(1ul, unpinned->elements.at(1)
                     ->get_tab_collection_container()
                     ->elements.size());
  ASSERT_TRUE(unpinned->elements.at(1)
                  ->get_tab_collection_container()
                  ->elements.at(0)
                  ->is_tab_container());

  // Third branch.
  ASSERT_TRUE(unpinned->elements.at(2)->is_tab_collection_container());
  ASSERT_EQ(2ul, unpinned->elements.at(2)
                     ->get_tab_collection_container()
                     ->elements.size());
  ASSERT_TRUE(unpinned->elements.at(2)
                  ->get_tab_collection_container()
                  ->elements.at(0)
                  ->is_tab_container());
  ASSERT_TRUE(unpinned->elements.at(2)
                  ->get_tab_collection_container()
                  ->elements.at(1)
                  ->is_tab_container());
}

}  // namespace
}  // namespace tabs_api
