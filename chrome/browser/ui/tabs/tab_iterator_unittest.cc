// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/containers/adapters.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_group_desktop.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabCollectionIteratorTest : public ::testing::Test {
 public:
  TabCollectionIteratorTest() {
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());

    collection_ = std::make_unique<tabs::UnpinnedTabCollection>();
  }

  ~TabCollectionIteratorTest() override { collection_.reset(); }

  TabCollectionIteratorTest(const TabCollectionIteratorTest&) = delete;
  TabCollectionIteratorTest& operator=(const TabCollectionIteratorTest&) =
      delete;

  std::unique_ptr<content::WebContents> MakeWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(testing_profile_.get()));
  }

  tabs::UnpinnedTabCollection* collection() { return collection_.get(); }
  TabStripModel* GetTabStripModel() { return tab_strip_model_.get(); }
  Profile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;

  // Use unpinned collection as it can have tabs and collection as children.
  std::unique_ptr<tabs::UnpinnedTabCollection> collection_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(TabCollectionIteratorTest, TabIteratorWithoutChildren) {
  EXPECT_EQ(*collection()->begin(), nullptr);
  EXPECT_EQ(*collection()->end(), nullptr);
  EXPECT_EQ(std::distance(collection()->begin(), collection()->end()), 0);
}

TEST_F(TabCollectionIteratorTest, TabIteratorWithOnlyCollection) {
  TabGroupDesktop::Factory factory(profile());
  collection()->AddCollection(
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData()),
      0);
  collection()->AddCollection(
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData()),
      0);

  EXPECT_EQ(*collection()->begin(), nullptr);
  EXPECT_EQ(*collection()->end(), nullptr);
  EXPECT_EQ(std::distance(collection()->begin(), collection()->end()), 0);
}

TEST_F(TabCollectionIteratorTest, TabIteratorWithOnlyTabs) {
  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        0);
  }

  EXPECT_EQ(*collection()->begin(), collection()->GetTabAtIndexRecursive(0));
  EXPECT_EQ(*collection()->end(), nullptr);
  EXPECT_EQ(std::distance(collection()->begin(), collection()->end()), 5);
}

TEST_F(TabCollectionIteratorTest, TabIteratorWithMixedTabsAndCollections) {
  // Add a group with two tabs.
  TabGroupDesktop::Factory factory(profile());
  std::unique_ptr<tabs::TabGroupTabCollection> group_one =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData());

  group_one->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  group_one->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  collection()->AddCollection(std::move(group_one), 0);

  // Add five tabs.
  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        collection()->ChildCount());
  }

  // Add another group containing a tab and a split collection with two tabs.
  std::unique_ptr<tabs::TabGroupTabCollection> group_two =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData());

  group_two->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  std::unique_ptr<tabs::SplitTabCollection> split_collection =
      std::make_unique<tabs::SplitTabCollection>(
          split_tabs::SplitTabId::GenerateNew(),
          split_tabs::SplitTabVisualData());
  split_collection->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  split_collection->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  group_two->AddCollection(std::move(split_collection), 1);
  collection()->AddCollection(std::move(group_two), collection()->ChildCount());

  EXPECT_EQ(*collection()->end(), nullptr);
  EXPECT_EQ(std::distance(collection()->begin(), collection()->end()), 10);

  int index = 0;
  for (auto it = collection()->begin(); it != collection()->end();
       it++, index++) {
    EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(index));
  }
}

TEST_F(TabCollectionIteratorTest, TabIteratorBackwardIterationWithOnlyTabs) {
  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        collection()->ChildCount());
  }

  // Iterate backwards starting from end().
  auto it = collection()->end();
  for (int i = 4; i >= 0; i--) {
    --it;
    EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(i));
  }
  EXPECT_EQ(it, collection()->begin());

  // Test postfix decrement.
  it = collection()->end();
  it--;
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(4));
}

TEST_F(TabCollectionIteratorTest,
       TabIteratorBackwardIterationWithMixedTabsAndCollections) {
  TabGroupDesktop::Factory factory(profile());
  std::unique_ptr<tabs::TabGroupTabCollection> group_one =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData());

  group_one->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  group_one->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  collection()->AddCollection(std::move(group_one), 0);

  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        collection()->ChildCount());
  }

  std::unique_ptr<tabs::TabGroupTabCollection> group_two =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData());

  group_two->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  std::unique_ptr<tabs::SplitTabCollection> split_collection =
      std::make_unique<tabs::SplitTabCollection>(
          split_tabs::SplitTabId::GenerateNew(),
          split_tabs::SplitTabVisualData());
  split_collection->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  split_collection->AddTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      0);
  group_two->AddCollection(std::move(split_collection), 1);
  collection()->AddCollection(std::move(group_two), collection()->ChildCount());

  // Iterate backwards from end() to begin().
  auto it = collection()->end();
  for (int i = 9; i >= 0; i--) {
    --it;
    EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(i));
  }
  EXPECT_EQ(it, collection()->begin());

  // Test bidirectional stepping (forward then backward) across boundaries.
  it = collection()->begin();
  ++it;  // index 1 (group_one)
  ++it;  // index 2 (loose tab)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(2));
  --it;  // index 1 (group_one)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(1));
  --it;  // index 0 (group_one)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(0));
  EXPECT_EQ(it, collection()->begin());

  // Test bidirectional stepping across group_two and split_collection
  // boundaries.
  for (int i = 0; i < 8; ++i) {
    ++it;  // advance to index 8 (split_collection in group_two)
  }
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(8));
  ++it;  // index 9 (split_collection)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(9));
  --it;  // index 8 (split_collection)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(8));
  --it;  // index 7 (group_two direct child tab)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(7));
  --it;  // index 6 (loose tab)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(6));
  ++it;  // index 7 (group_two direct child tab)
  EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(7));

  // Test constructing from tab and stepping backward.
  tabs::TabInterface* middle_tab = collection()->GetTabAtIndexRecursive(5);
  tabs::TabCollection::TabIterator mid_it(middle_tab);
  EXPECT_EQ(*mid_it, middle_tab);
  --mid_it;
  EXPECT_EQ(*mid_it, collection()->GetTabAtIndexRecursive(4));
}

TEST_F(TabCollectionIteratorTest, ReverseIteratorAndBaseReversed) {
  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        collection()->ChildCount());
  }

  // Verify rbegin() and rend().
  int expected_index = 4;
  for (auto it = collection()->rbegin(); it != collection()->rend(); ++it) {
    EXPECT_EQ(*it, collection()->GetTabAtIndexRecursive(expected_index--));
  }
  EXPECT_EQ(expected_index, -1);

  // Verify base::Reversed support.
  expected_index = 4;
  for (tabs::TabInterface* tab : base::Reversed(*collection())) {
    EXPECT_EQ(tab, collection()->GetTabAtIndexRecursive(expected_index--));
  }
  EXPECT_EQ(expected_index, -1);
}
