// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/mojo_tree_builder.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
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

  auto result =
      MojoTreeBuilder(GetTabStripModel())
          .Build(GetTabStripModel()->GetRootForTesting()->GetHandle());

  // First layer is just pinned/unpinned.
  // Pinned is idx 0, unpinned is idx1.
  ASSERT_EQ(2ul, result->children.size());
  const auto& unpinned = result->children.at(1);
  ASSERT_TRUE(unpinned->data->is_unpinned_tabs());
  ASSERT_EQ(3ul, unpinned->children.size());

  // First branch.
  ASSERT_TRUE(unpinned->children.at(0)->data->is_tab());

  // Second branch.
  const auto& group_one = unpinned->children.at(1);
  ASSERT_TRUE(group_one->data->is_tab_group());
  ASSERT_EQ(1ul, group_one->children.size());
  ASSERT_TRUE(group_one->children.at(0)->data->is_tab());

  // Third branch.
  const auto& group_two = unpinned->children.at(2);
  ASSERT_TRUE(group_two->data->is_tab_group());
  ASSERT_EQ(2ul, group_two->children.size());
  ASSERT_TRUE(group_two->children.at(0)->data->is_tab());
  ASSERT_TRUE(group_two->children.at(1)->data->is_tab());
}

}  // namespace
}  // namespace tabs_api
