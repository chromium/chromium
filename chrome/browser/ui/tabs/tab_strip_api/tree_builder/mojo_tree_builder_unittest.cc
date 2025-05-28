// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tree_builder/mojo_tree_builder.h"

#include <memory>
#include <utility>

#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
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

class MojoTreeBuilderTest : public ::testing::Test {
 public:
  MojoTreeBuilderTest() {
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());
    tab_strip_model_adapter_ =
        std::make_unique<tabs_api::TabStripModelAdapterImpl>(
            tab_strip_model_.get(),
            TabStripServiceImpl::get_passkey_for_testing());
    tree_builder_ = std::make_unique<tabs_api::MojoTreeBuilder>(
        TabStripServiceImpl::get_passkey_for_testing(),
        tab_strip_model_adapter_.get());
    collection_ = std::make_unique<tabs::UnpinnedTabCollection>();
  }

  ~MojoTreeBuilderTest() override { collection_.reset(); }

  MojoTreeBuilderTest(const MojoTreeBuilderTest&) = delete;
  MojoTreeBuilderTest& operator=(const MojoTreeBuilderTest&) = delete;

  std::unique_ptr<content::WebContents> MakeWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(testing_profile_.get()));
  }

  tabs::UnpinnedTabCollection* collection() { return collection_.get(); }
  TabStripModel* GetTabStripModel() { return tab_strip_model_.get(); }
  tabs_api::MojoTreeBuilder* tree_builder() { return tree_builder_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;

  // Use unpinned collection as it can have tabs and collection as children.
  std::unique_ptr<tabs::UnpinnedTabCollection> collection_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<tabs_api::TabStripModelAdapter> tab_strip_model_adapter_;
  std::unique_ptr<tabs_api::MojoTreeBuilder> tree_builder_;
  tabs::PreventTabFeatureInitialization prevent_;
};

TEST_F(MojoTreeBuilderTest, IteratorWithoutChildren) {
  auto result = tree_builder()->BuildTree(nullptr);
  ASSERT_TRUE(result.is_null());

  // Result should just be the collection.
  result = tree_builder()->BuildTree(collection());
  ASSERT_FALSE(result.is_null());
  ASSERT_EQ(result->elements.size(), 0u);
}

TEST_F(MojoTreeBuilderTest, IteratorWithOnlyCollection) {
  collection()->AddCollection(std::make_unique<tabs::TabGroupTabCollection>(
                                  tab_groups::TabGroupId::GenerateNew(),
                                  tab_groups::TabGroupVisualData()),
                              0);
  collection()->AddCollection(std::make_unique<tabs::TabGroupTabCollection>(
                                  tab_groups::TabGroupId::GenerateNew(),
                                  tab_groups::TabGroupVisualData()),
                              0);

  tabs_api::mojom::TabCollectionContainerPtr result =
      tree_builder()->BuildTree(collection());

  ASSERT_FALSE(result.is_null());
  const auto& root = result->collection;
  EXPECT_EQ(root->collection_type,
            tabs_api::mojom::TabCollection::CollectionType::kUnpinned);
  EXPECT_EQ(root->id.Type(), tabs_api::TabId::Type::kCollection);
  ASSERT_EQ(result->elements.size(), 2u);

  for (size_t i = 0; i < 2; i++) {
    const auto& container = result->elements[i];
    ASSERT_TRUE(container->is_tab_collection_container());
    const auto& tab_collection_container =
        container->get_tab_collection_container();
    EXPECT_TRUE(tab_collection_container->elements.empty());

    const auto& tab_collection = tab_collection_container->collection;
    EXPECT_EQ(tab_collection->collection_type,
              tabs_api::mojom::TabCollection::CollectionType::kTabGroup);
    EXPECT_EQ(tab_collection->id.Type(), tabs_api::TabId::Type::kCollection);
  }
}

TEST_F(MojoTreeBuilderTest, IteratorWithOnlyTabs) {
  for (int i = 0; i < 5; i++) {
    collection()->AddTab(
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
        0);
  }

  tabs_api::mojom::TabCollectionContainerPtr result =
      tree_builder()->BuildTree(collection());

  ASSERT_FALSE(result.is_null());

  const auto& root = result->collection;
  EXPECT_EQ(root->collection_type,
            tabs_api::mojom::TabCollection::CollectionType::kUnpinned);
  ASSERT_EQ(result->elements.size(), 5u);

  for (int i = 0; i < 5; i++) {
    const auto& container = result->elements[i];
    ASSERT_TRUE(container->is_tab_container());
    const auto& tab_container = container->get_tab_container();
    ASSERT_FALSE(tab_container.is_null());
    // TODO (crbug.com/409086859) Add additional testing for the tab within the
    // container.
  }
}

TEST_F(MojoTreeBuilderTest, IteratorWithMixedTabsAndCollections) {
  // Add a group with three tabs.
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

  auto result = tree_builder()->BuildTree(collection());

  ASSERT_FALSE(result.is_null());

  const auto& root = result->collection;
  EXPECT_EQ(root->collection_type,
            tabs_api::mojom::TabCollection::CollectionType::kUnpinned);
  // Check there are only 7 direct items within the root tab collection
  ASSERT_EQ(result->elements.size(), 7u);

  // Check the first item is the group container
  ASSERT_TRUE(result->elements[0]->is_tab_collection_container());
  const auto& group_container =
      result->elements[0]->get_tab_collection_container();
  const auto& group_one_collection = group_container->collection;
  EXPECT_EQ(group_one_collection->collection_type,
            tabs_api::mojom::TabCollection::CollectionType::kTabGroup);

  // Check there are 3 tabs within the group container
  ASSERT_EQ(group_container->elements.size(), 3u);
  for (int i = 0; i < 3; i++) {
    const auto& tab_container = group_container->elements[i];
    ASSERT_TRUE(tab_container->is_tab_container());
  }

  // Check there are 5 tabs within the root collection after the group container
  for (int i = 1; i < 6; i++) {
    const auto& tab_container = result->elements[i];
    ASSERT_TRUE(tab_container->is_tab_container());
  }

  // Check the 7th item is the split tab container
  ASSERT_TRUE(result->elements[6]->is_tab_collection_container());
  const auto& split_tab_container =
      result->elements[6]->get_tab_collection_container();
  const auto& split_tab_collection = split_tab_container->collection;
  EXPECT_EQ(split_tab_collection->collection_type,
            tabs_api::mojom::TabCollection::CollectionType::kSplitTab);

  // Check there are 2 tabs within the split tab container
  ASSERT_EQ(split_tab_container->elements.size(), 2u);
  for (int i = 0; i < 2; i++) {
    const auto& tab_container = split_tab_container->elements[i];
    ASSERT_TRUE(tab_container->is_tab_container());
  }
}
