// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;

  // Use unpinned collection as it can have tabs and collection as children.
  std::unique_ptr<tabs::UnpinnedTabCollection> collection_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  tabs::PreventTabFeatureInitialization prevent_;
};

TEST_F(TabCollectionIteratorTest, TabIteratorWithoutChildren) {
  EXPECT_EQ(*collection()->begin(), nullptr);
  EXPECT_EQ(*collection()->end(), nullptr);
  EXPECT_EQ(std::distance(collection()->begin(), collection()->end()), 0);
}

TEST_F(TabCollectionIteratorTest, TabIteratorWithOnlyCollection) {
  collection()->AddCollection(std::make_unique<tabs::TabGroupTabCollection>(
                                  tab_groups::TabGroupId::GenerateNew(),
                                  tab_groups::TabGroupVisualData()),
                              0);
  collection()->AddCollection(std::make_unique<tabs::TabGroupTabCollection>(
                                  tab_groups::TabGroupId::GenerateNew(),
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
  std::unique_ptr<tabs::TabGroupTabCollection> group_one =
      std::make_unique<tabs::TabGroupTabCollection>(
          tab_groups::TabGroupId::GenerateNew(),
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
          tab_groups::TabGroupId::GenerateNew(),
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

TEST_F(TabCollectionIteratorTest, IteratorWithoutChildren) {
  auto service_passkey = TabStripServiceImpl::get_passkey_for_testing();
  // collection_begin is a pre-order traversal which sets itself to the root tab
  // collection.
  auto it = collection()->collection_begin(service_passkey);
  ASSERT_NE(it, collection()->collection_end(service_passkey));
  const auto& root = *it;
  ASSERT_TRUE(std::holds_alternative<const tabs::TabCollection*>(root));
  const tabs::TabCollection* root_ptr =
      std::get<const tabs::TabCollection*>(root);
  ASSERT_EQ(root_ptr, collection());
}

TEST_F(TabCollectionIteratorTest, IteratorWithOnlyCollection) {
  collection()->AddCollection(std::make_unique<tabs::TabGroupTabCollection>(
                                  tab_groups::TabGroupId::GenerateNew(),
                                  tab_groups::TabGroupVisualData()),
                              0);
  collection()->AddCollection(std::make_unique<tabs::TabGroupTabCollection>(
                                  tab_groups::TabGroupId::GenerateNew(),
                                  tab_groups::TabGroupVisualData()),
                              0);
  auto service_passkey = TabStripServiceImpl::get_passkey_for_testing();
  auto it = collection()->collection_begin(service_passkey);
  ASSERT_NE(it, collection()->collection_end(service_passkey));
  it++;
  while (it != collection()->collection_end(service_passkey)) {
    const auto& current_element = *it;
    ASSERT_TRUE(
        std::holds_alternative<const tabs::TabCollection*>(current_element));
    const tabs::TabCollection* tab_collection =
        std::get<const tabs::TabCollection*>(current_element);
    ASSERT_TRUE(tab_collection->type() == tabs::TabCollection::Type::GROUP);
    it++;
  }
}

TEST_F(TabCollectionIteratorTest, IteratorWithOnlyTabs) {
  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        0);
  }

  auto service_passkey = TabStripServiceImpl::get_passkey_for_testing();
  auto it = collection()->collection_begin(service_passkey);
  auto end = collection()->collection_end(service_passkey);
  ASSERT_NE(it, end);
  it++;
  while (it != end) {
    const auto& current_element = *it;
    ASSERT_TRUE(
        std::holds_alternative<const tabs::TabInterface*>(current_element));
    it++;
  }
}

TEST_F(TabCollectionIteratorTest, IteratorWithMixedTabsAndCollections) {
  // Add a group with two tabs.
  std::unique_ptr<tabs::TabGroupTabCollection> group_one =
      std::make_unique<tabs::TabGroupTabCollection>(
          tab_groups::TabGroupId::GenerateNew(),
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

  // Add a split collection with two tabs.
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
  collection()->AddCollection(std::move(split_collection),
                              collection()->ChildCount());

  // Start at the root node.
  auto service_passkey = TabStripServiceImpl::get_passkey_for_testing();
  auto it = collection()->collection_begin(service_passkey);
  // First verify the first node is the TabGroup.
  it++;
  const auto& tab_group_element = *it;
  ASSERT_TRUE(
      std::holds_alternative<const tabs::TabCollection*>(tab_group_element));
  const tabs::TabCollection* tab_collection =
      std::get<const tabs::TabCollection*>(tab_group_element);
  ASSERT_TRUE(tab_collection->type() == tabs::TabCollection::Type::GROUP);
  // The rest will be tabs. 2 within the TabGroup and 5 within the
  // UnpinnedCollection.
  it++;
  for (int i = 0; i < 7; i++) {
    const auto& tab_element = *it;
    ASSERT_TRUE(std::holds_alternative<const tabs::TabInterface*>(tab_element));
    it++;
  }

  // Then the next node should be the SplitTab
  const auto& split_tab_element = *it;
  ASSERT_TRUE(
      std::holds_alternative<const tabs::TabCollection*>(split_tab_element));
  tab_collection = std::get<const tabs::TabCollection*>(split_tab_element);
  ASSERT_TRUE(tab_collection->type() == tabs::TabCollection::Type::SPLIT);
  // Follow by 2 more tabs within SplitTab
  it++;
  for (int i = 0; i < 2; i++) {
    const auto& tab_element = *it;
    ASSERT_TRUE(std::holds_alternative<const tabs::TabInterface*>(tab_element));
    it++;
  }
}
