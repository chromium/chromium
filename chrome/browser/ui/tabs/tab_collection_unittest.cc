// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_collection.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_desktop.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_collection_storage.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/392950857): Refactor tests so that things that are shared
// between all tab collections are tested once at the TabCollectionBaseTest
// level. Then only things specific to individual collections will be tested in
// their respective test suites.
class TabCollectionBaseTest : public ::testing::Test {
 public:
  TabCollectionBaseTest() {
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());
  }
  TabCollectionBaseTest(const TabCollectionBaseTest&) = delete;
  TabCollectionBaseTest& operator=(const TabCollectionBaseTest&) = delete;
  ~TabCollectionBaseTest() override = default;

  TabStripModel* GetTabStripModel() { return tab_strip_model_.get(); }

  tabs::TabInterface* GetTabInCollectionStorage(
      tabs::TabCollectionStorage* storage,
      size_t index) {
    const auto& child = storage->GetChildren().at(index);
    const auto tab_ptr =
        std::get_if<std::unique_ptr<tabs::TabInterface>>(&child);
    return tab_ptr ? tab_ptr->get() : nullptr;
  }

  tabs::TabCollection* GetCollectionInCollectionStorage(
      tabs::TabCollectionStorage* storage,
      size_t index) {
    const auto& child = storage->GetChildren().at(index);
    const auto tab_collection_ptr =
        std::get_if<std::unique_ptr<tabs::TabCollection>>(&child);
    return tab_collection_ptr ? tab_collection_ptr->get() : nullptr;
  }

  std::unique_ptr<content::WebContents> MakeWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(testing_profile_.get()));
  }

  // Adds a tab to the end of a collection.
  tabs::TabInterface* AppendTab(tabs::TabCollection* collection,
                                std::unique_ptr<tabs::TabInterface> tab) {
    return collection->AddTab(std::move(tab), collection->ChildCount());
  }

  // Returns true if the tab model is a direct child of the collection.
  bool ContainsTab(tabs::TabCollection* collection,
                   const tabs::TabInterface* tab) {
    return collection->GetIndexOfTab(tab).has_value();
  }

  // Returns true if the tab collection tree contains the tab.
  bool ContainsTabRecursive(tabs::TabCollection* collection,
                            const tabs::TabInterface* tab) {
    return collection->GetIndexOfTabRecursive(tab).has_value();
  }

  void AddTabsToPinnedContainer(tabs::PinnedTabCollection* collection,
                                TabStripModel* tab_strip_model,
                                int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(MakeWebContents(), tab_strip_model);
      tabs::TabModel* tab_model_ptr = tab_model.get();
      AppendTab(collection, std::move(tab_model));
      EXPECT_EQ(collection->GetIndexOfTabRecursive(tab_model_ptr),
                collection->ChildCount() - 1);
    }
  }

  void AddTabsToGroupContainer(tabs::TabGroupTabCollection* collection,
                               TabStripModel* tab_strip_model,
                               int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(MakeWebContents(), tab_strip_model);
      tabs::TabModel* tab_model_ptr = tab_model.get();
      AppendTab(collection, std::move(tab_model));
      EXPECT_EQ(collection->GetIndexOfTabRecursive(tab_model_ptr),
                collection->ChildCount() - 1);
    }
  }

  void AddTabsToUnpinnedContainer(tabs::UnpinnedTabCollection* collection,
                                  TabStripModel* tab_strip_model,
                                  int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(MakeWebContents(), tab_strip_model);
      AppendTab(collection, std::move(tab_model));
    }
  }

  Profile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_enabler_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(TabCollectionBaseTest, GetDirectChildIndexOfCollectionContainingTab) {
  std::unique_ptr<tabs::UnpinnedTabCollection> unpinned_collection =
      std::make_unique<tabs::UnpinnedTabCollection>();
  TabGroupDesktop::Factory factory(profile());
  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData());
  std::unique_ptr<tabs::SplitTabCollection> split_collection =
      std::make_unique<tabs::SplitTabCollection>(
          split_tabs::SplitTabId::GenerateNew(),
          split_tabs::SplitTabVisualData());
  tabs::TabGroupTabCollection* group_collection_ptr = group_collection.get();
  tabs::SplitTabCollection* split_collection_ptr = split_collection.get();

  std::vector<std::unique_ptr<tabs::TabModel>> tabs;
  std::vector<tabs::TabModel*> tab_ptrs;
  for (size_t i = 0; i < 4; i++) {
    tabs.push_back(std::make_unique<tabs::TabModel>(MakeWebContents(),
                                                    GetTabStripModel()));
    tab_ptrs.push_back(tabs[i].get());
  }

  AppendTab(unpinned_collection.get(), std::move(tabs[0]));
  AppendTab(group_collection.get(), std::move(tabs[1]));
  AppendTab(split_collection.get(), std::move(tabs[2]));
  AppendTab(split_collection.get(), std::move(tabs[3]));
  group_collection->AddCollection(std::move(split_collection), 1);
  unpinned_collection->AddCollection(std::move(group_collection), 1);
  // u{0, g{1, s{2, 3}}}

  EXPECT_EQ(0,
            split_collection_ptr->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[2]));
  EXPECT_EQ(1,
            split_collection_ptr->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[3]));

  EXPECT_EQ(0,
            group_collection_ptr->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[1]));
  EXPECT_EQ(1,
            group_collection_ptr->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[2]));
  EXPECT_EQ(1,
            group_collection_ptr->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[3]));

  EXPECT_EQ(0,
            unpinned_collection->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[0]));
  EXPECT_EQ(1,
            unpinned_collection->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[1]));
  EXPECT_EQ(1,
            unpinned_collection->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[2]));
  EXPECT_EQ(1,
            unpinned_collection->GetDirectChildIndexOfCollectionContainingTab(
                tab_ptrs[3]));
}

class PinnedTabCollectionTest : public TabCollectionBaseTest {
 public:
  PinnedTabCollectionTest() {
    pinned_collection_ = std::make_unique<tabs::PinnedTabCollection>();
  }
  PinnedTabCollectionTest(const PinnedTabCollectionTest&) = delete;
  PinnedTabCollectionTest& operator=(const PinnedTabCollectionTest&) = delete;
  ~PinnedTabCollectionTest() override { pinned_collection_.reset(); }

  tabs::PinnedTabCollection* pinned_collection() {
    return pinned_collection_.get();
  }

 private:
  std::unique_ptr<tabs::PinnedTabCollection> pinned_collection_;
};

TEST_F(PinnedTabCollectionTest, AddOperation) {
  // Setup phase of keeping track of two tabs.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  auto tab_model_two =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabModel* tab_model_two_ptr = tab_model_two.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  tabs::PinnedTabCollection* pinned_collection_instance = pinned_collection();

  // Add a tab to the end of the pinned collection.
  AppendTab(pinned_collection_instance, std::move(tab_model_one));
  EXPECT_TRUE(tab_model_one_ptr->IsPinned());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            pinned_collection_instance);

  EXPECT_TRUE(
      ContainsTabRecursive(pinned_collection_instance, tab_model_one_ptr));

  // Add four more tabs to the collection.
  AddTabsToPinnedContainer(pinned_collection_instance, GetTabStripModel(), 4);

  EXPECT_EQ(pinned_collection_instance->ChildCount(), 5ul);
  EXPECT_EQ(pinned_collection_instance->TabCountRecursive(), 5ul);

  // Add the second tab to index 3.
  pinned_collection_instance->AddTab(std::move(tab_model_two), 3ul);
  EXPECT_EQ(
      pinned_collection_instance->GetIndexOfTabRecursive(tab_model_two_ptr),
      3ul);
}

TEST_F(PinnedTabCollectionTest, RemoveOperation) {
  // Setup phase of keeping track of a tab.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::PinnedTabCollection* pinned_collection_instance = pinned_collection();

  // Add four tabs to the collection.
  AddTabsToPinnedContainer(pinned_collection_instance, GetTabStripModel(), 4);

  // Add `tab_model_one` to index 3.
  pinned_collection_instance->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(
      pinned_collection_instance->GetIndexOfTabRecursive(tab_model_one_ptr),
      3ul);
  EXPECT_EQ(pinned_collection_instance->ChildCount(), 5ul);
  EXPECT_TRUE(tab_model_one_ptr->IsPinned());

  tab_model_one_ptr->set_will_be_detaching_for_testing(true);

  // Remove `tab_model_one` from the collection.
  auto removed_tab =
      pinned_collection_instance->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_EQ(tab_model_one_ptr, removed_tab.get());
  EXPECT_FALSE(tab_model_one_ptr->IsPinned());
  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());

  EXPECT_EQ(pinned_collection_instance->ChildCount(), 4ul);
}

TEST_F(PinnedTabCollectionTest, CollectionOperations) {
  // Setup phase of keeping track of a tab.
  tabs::PinnedTabCollection* pinned_collection_instance = pinned_collection();

  // Add four tabs to the collection.
  AddTabsToPinnedContainer(pinned_collection_instance, GetTabStripModel(), 4);

  std::unique_ptr<tabs::TabCollection> collection =
      std::make_unique<tabs::SplitTabCollection>(
          split_tabs::SplitTabId::GenerateNew(),
          split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                         0.5));
  tabs::TabCollection* collection_ptr = collection.get();
  EXPECT_EQ(pinned_collection_instance->GetIndexOfCollection(collection_ptr),
            std::nullopt);
  EXPECT_FALSE(pinned_collection_instance->ContainsCollection(collection_ptr));

  EXPECT_EQ(collection_ptr, pinned_collection_instance->AddCollection(
                                std::move(collection), 2));
  EXPECT_TRUE(pinned_collection_instance->ContainsCollection(collection_ptr));
  EXPECT_EQ(2,
            pinned_collection_instance->GetIndexOfCollection(collection_ptr));
  EXPECT_EQ(
      collection_ptr,
      pinned_collection_instance->MaybeRemoveCollection(collection_ptr).get());
  EXPECT_FALSE(pinned_collection_instance->ContainsCollection(collection_ptr));
  EXPECT_EQ(std::nullopt,
            pinned_collection_instance->GetIndexOfCollection(collection_ptr));
}

class TabGroupTabCollectionTest : public TabCollectionBaseTest {
 public:
  TabGroupTabCollectionTest() {
    TabGroupDesktop::Factory factory(profile());
    grouped_collection_ = std::make_unique<tabs::TabGroupTabCollection>(
        factory, tab_groups::TabGroupId::GenerateNew(),
        tab_groups::TabGroupVisualData());
  }
  TabGroupTabCollectionTest(const TabGroupTabCollectionTest&) = delete;
  TabGroupTabCollectionTest& operator=(const TabGroupTabCollectionTest&) =
      delete;
  ~TabGroupTabCollectionTest() override { grouped_collection_.reset(); }

  tabs::TabGroupTabCollection* GetCollection() {
    return grouped_collection_.get();
  }

 private:
  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(TabGroupTabCollectionTest, AddOperation) {
  // Setup phase of keeping track of two tabs.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  auto tab_model_two =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabModel* tab_model_two_ptr = tab_model_two.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  tabs::TabGroupTabCollection* grouped_collection = GetCollection();

  // Add `tab_model_one` to the end of the collection.
  AppendTab(grouped_collection, std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->group(), grouped_collection->GetTabGroupId());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            grouped_collection);
  EXPECT_TRUE(ContainsTabRecursive(grouped_collection, tab_model_one_ptr));

  // Add four tabs to the collection.
  AddTabsToGroupContainer(GetCollection(), GetTabStripModel(), 4);

  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);
  EXPECT_EQ(grouped_collection->TabCountRecursive(), 5ul);

  // Add `tab_model_two` to index 3.
  grouped_collection->AddTab(std::move(tab_model_two), 3ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_two_ptr), 3ul);
}

TEST_F(TabGroupTabCollectionTest, RemoveOperation) {
  // Setup phase of keeping track of a tab.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::TabGroupTabCollection* grouped_collection = GetCollection();

  // Add four tabs to the collection.
  AddTabsToGroupContainer(GetCollection(), GetTabStripModel(), 4);

  // Add `tab_model_one` to index 3.
  grouped_collection->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 3ul);
  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);
  EXPECT_EQ(tab_model_one_ptr->group(), grouped_collection->GetTabGroupId());

  tab_model_one_ptr->set_will_be_detaching_for_testing(true);

  // Remove `tab_model_one` from the collection.
  auto removed_tab = grouped_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_EQ(tab_model_one_ptr, removed_tab.get());
  EXPECT_FALSE(tab_model_one_ptr->group().has_value());
  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  EXPECT_EQ(grouped_collection->ChildCount(), 4ul);
}

class SplitTabCollectionTest : public TabCollectionBaseTest {
 public:
  SplitTabCollectionTest() {
    split_collection_ = std::make_unique<tabs::SplitTabCollection>(
        split_tabs::SplitTabId::GenerateNew(),
        split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                       0.5));
  }
  SplitTabCollectionTest(const SplitTabCollectionTest&) = delete;
  SplitTabCollectionTest& operator=(const SplitTabCollectionTest&) = delete;
  ~SplitTabCollectionTest() override { split_collection_.reset(); }

  tabs::SplitTabCollection* GetCollection() { return split_collection_.get(); }

  void AddTabsToSplitContainer(tabs::SplitTabCollection* collection,
                               TabStripModel* tab_strip_model,
                               int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(MakeWebContents(), tab_strip_model);
      tabs::TabModel* tab_model_ptr = tab_model.get();
      AppendTab(collection, std::move(tab_model));
      EXPECT_EQ(collection->GetIndexOfTabRecursive(tab_model_ptr),
                collection->ChildCount() - 1);
    }
  }

 private:
  std::unique_ptr<tabs::SplitTabCollection> split_collection_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(SplitTabCollectionTest, AddOperation) {
  // Setup phase of keeping track of two tabs.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  auto tab_model_two =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabModel* tab_model_two_ptr = tab_model_two.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  tabs::SplitTabCollection* split_collection = GetCollection();

  // Add `tab_model_one` to the end of the collection.
  AppendTab(split_collection, std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->GetSplit(), split_collection->GetSplitTabId());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            split_collection);
  EXPECT_TRUE(ContainsTabRecursive(split_collection, tab_model_one_ptr));

  // Add two tabs to the collection.
  AddTabsToSplitContainer(GetCollection(), GetTabStripModel(), 2);

  EXPECT_EQ(split_collection->ChildCount(), 3ul);
  EXPECT_EQ(split_collection->TabCountRecursive(), 3ul);

  // Add `tab_model_two` to index 1.
  split_collection->AddTab(std::move(tab_model_two), 1ul);
  EXPECT_EQ(split_collection->GetIndexOfTabRecursive(tab_model_two_ptr), 1ul);
}

TEST_F(SplitTabCollectionTest, RemoveOperation) {
  // Setup phase of keeping track of a tab.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::SplitTabCollection* split_collection = GetCollection();

  // Add three tabs to the collection.
  AddTabsToSplitContainer(GetCollection(), GetTabStripModel(), 3);

  // Add `tab_model_one` to index 2.
  split_collection->AddTab(std::move(tab_model_one), 2ul);
  EXPECT_EQ(split_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 2ul);
  EXPECT_EQ(split_collection->ChildCount(), 4ul);
  EXPECT_EQ(tab_model_one_ptr->GetSplit(), split_collection->GetSplitTabId());

  tab_model_one_ptr->set_will_be_detaching_for_testing(true);

  // Remove `tab_model_one` from the collection.
  auto removed_tab = split_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_EQ(tab_model_one_ptr, removed_tab.get());
  EXPECT_FALSE(tab_model_one_ptr->GetSplit().has_value());
  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  EXPECT_EQ(split_collection->ChildCount(), 3ul);
}

class UnpinnedTabCollectionTest : public TabCollectionBaseTest {
 public:
  UnpinnedTabCollectionTest() {
    unpinned_collection_ = std::make_unique<tabs::UnpinnedTabCollection>();
  }
  UnpinnedTabCollectionTest(const UnpinnedTabCollectionTest&) = delete;
  UnpinnedTabCollectionTest& operator=(const UnpinnedTabCollectionTest&) =
      delete;
  ~UnpinnedTabCollectionTest() override { unpinned_collection_.reset(); }

  tabs::UnpinnedTabCollection* GetCollection() {
    return unpinned_collection_.get();
  }

  // Creates a basic setup of the unpinned collection with -
  // 1. Two tabs at the start of the collection. Followed by
  // 2. A group with two tabs. Followed by
  // 3. Two tabs.
  void PerformBasicSetup() {
    AddTabsToUnpinnedContainer(GetCollection(), GetTabStripModel(), 2);
    tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
    TabGroupDesktop::Factory factory(profile());
    auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(
        factory, group_id, tab_groups::TabGroupVisualData());
    AddTabsToGroupContainer(tab_group_one.get(), GetTabStripModel(), 2);
    GetCollection()->AddCollection(std::move(tab_group_one), 2);
    AddTabsToUnpinnedContainer(GetCollection(), GetTabStripModel(), 2);

    EXPECT_EQ(GetCollection()->ChildCount(), 5ul);
    EXPECT_EQ(GetCollection()->TabCountRecursive(), 6ul);

    EXPECT_EQ(GetCollection()->GetDirectChildIndexOfCollectionContainingTab(
                  GetCollection()->GetTabAtIndexRecursive(1)),
              1ul);
    EXPECT_EQ(GetCollection()->GetDirectChildIndexOfCollectionContainingTab(
                  GetCollection()->GetTabAtIndexRecursive(3)),
              2ul);
  }

 private:
  std::unique_ptr<tabs::UnpinnedTabCollection> unpinned_collection_;
};

TEST_F(UnpinnedTabCollectionTest, AddOperation) {
  // Use the basic setup scenario and track a tab and group.
  PerformBasicSetup();
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  TabGroupDesktop::Factory factory(profile());
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(
      factory, group_id, tab_groups::TabGroupVisualData());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  EXPECT_FALSE(tab_group_one_ptr->GetParentCollection());
  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the `tab_model_one` to the collection.
  AppendTab(unpinned_collection, std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            unpinned_collection);
  EXPECT_TRUE(ContainsTabRecursive(unpinned_collection, tab_model_one_ptr));
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));

  // Add a group to the collection at index 2.
  unpinned_collection->AddCollection(std::move(tab_group_one), 2ul);
  EXPECT_EQ(tab_group_one_ptr->GetParentCollection(), unpinned_collection);
  EXPECT_TRUE(ContainsTabRecursive(unpinned_collection, tab_model_one_ptr));
  EXPECT_TRUE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr),
            6ul);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 2ul);

  auto tab_model_in_group =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_in_group_ptr = tab_model_in_group.get();

  // Add tabs to the group and validate index and size. Track one of the tabs.
  AppendTab(tab_group_one_ptr, std::make_unique<tabs::TabModel>(
                                   MakeWebContents(), GetTabStripModel()));
  AppendTab(tab_group_one_ptr, std::move(tab_model_in_group));

  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_in_group_ptr),
            3);
  EXPECT_EQ(unpinned_collection->ChildCount(), 7ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 9ul);
}

TEST_F(UnpinnedTabCollectionTest, RemoveOperation) {
  // Use the basic setup scenario and track a tab and group.
  PerformBasicSetup();
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  TabGroupDesktop::Factory factory(profile());
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(
      factory, group_id, tab_groups::TabGroupVisualData());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();

  // Add two tabs to the group
  AddTabsToGroupContainer(tab_group_one_ptr, GetTabStripModel(), 2);

  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the tab and the group at index 2 and index 4 respectively.
  unpinned_collection->AddTab(std::move(tab_model_one), 2ul);
  unpinned_collection->AddCollection(std::move(tab_group_one), 4ul);

  // Remove the tab
  tab_model_one_ptr->set_will_be_detaching_for_testing(true);
  std::unique_ptr<tabs::TabInterface> removed_tab =
      unpinned_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_EQ(removed_tab.get(), tab_model_one_ptr);
  EXPECT_EQ(unpinned_collection->ChildCount(), 6ul);
  EXPECT_FALSE(ContainsTabRecursive(unpinned_collection, tab_model_one_ptr));
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 8ul);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 3ul);

  // Remove the collection
  std::unique_ptr<tabs::TabCollection> removed_collection =
      unpinned_collection->MaybeRemoveCollection(tab_group_one_ptr);
  EXPECT_EQ(removed_collection.get(), tab_group_one_ptr);
  EXPECT_EQ(unpinned_collection->ChildCount(), 5ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 6ul);
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  EXPECT_EQ(removed_collection->GetParentCollection(), nullptr);
}

class TabStripCollectionTest : public TabCollectionBaseTest {
 public:
  TabStripCollectionTest() {
    tab_strip_collection_ = std::make_unique<tabs::TabStripCollection>();
  }
  TabStripCollectionTest(const TabStripCollectionTest&) = delete;
  TabStripCollectionTest& operator=(const TabStripCollectionTest&) = delete;
  ~TabStripCollectionTest() override { tab_strip_collection_.reset(); }

  tabs::TabStripCollection* GetCollection() {
    return tab_strip_collection_.get();
  }

  void PerformBasicSetup() {
    tabs::TabStripCollection* tab_strip_collection = GetCollection();
    tabs::PinnedTabCollection* pinned_collection =
        tab_strip_collection->pinned_collection();
    tabs::UnpinnedTabCollection* unpinned_collection =
        tab_strip_collection->unpinned_collection();

    // Add four pinned tabs.
    AddTabsToPinnedContainer(pinned_collection, GetTabStripModel(), 4);
    AddTabsToUnpinnedContainer(unpinned_collection, GetTabStripModel(), 2);

    // Add a group to the unpinned collection with two tabs.
    TabGroupDesktop::Factory factory(profile());
    std::unique_ptr<tabs::TabGroupTabCollection> group_one =
        std::make_unique<tabs::TabGroupTabCollection>(
            factory, tab_groups::TabGroupId::GenerateNew(),
            tab_groups::TabGroupVisualData());
    tabs::TabGroupTabCollection* group_one_ptr = group_one.get();
    AddTabsToGroupContainer(group_one_ptr, GetTabStripModel(), 2);
    tab_strip_collection->InsertTabCollectionAt(std::move(group_one), 6, false,
                                                std::nullopt);

    // Add one more tab.
    AppendTab(unpinned_collection, std::make_unique<tabs::TabModel>(
                                       MakeWebContents(), GetTabStripModel()));

    tabs::TabCollectionStorage* pinned_storage =
        pinned_collection->GetTabCollectionStorageForTesting();
    tabs::TabCollectionStorage* unpinned_storage =
        unpinned_collection->GetTabCollectionStorageForTesting();
    tabs::TabCollectionStorage* group_one_storage =
        group_one_ptr->GetTabCollectionStorageForTesting();

    EXPECT_EQ(tab_strip_collection->TabCountRecursive(), 9ul);

    // GetTabAtIndex checks
    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(0),
              GetTabInCollectionStorage(pinned_storage, 0ul));
    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(1),
              GetTabInCollectionStorage(pinned_storage, 1ul));
    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(2),
              GetTabInCollectionStorage(pinned_storage, 2ul));
    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(3),
              GetTabInCollectionStorage(pinned_storage, 3ul));

    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(4),
              GetTabInCollectionStorage(unpinned_storage, 0ul));
    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(5),
              GetTabInCollectionStorage(unpinned_storage, 1ul));

    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(6),
              GetTabInCollectionStorage(group_one_storage, 0ul));
    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(7),
              GetTabInCollectionStorage(group_one_storage, 1ul));

    EXPECT_EQ(tab_strip_collection->GetTabAtIndexRecursive(8),
              GetTabInCollectionStorage(unpinned_storage, 3ul));
  }

  void TestAddTabRecursive(size_t index,
                           std::optional<tab_groups::TabGroupId> new_group_id,
                           bool new_pinned_state) {
    tabs::TabStripCollection* tab_strip_collection = GetCollection();
    std::unique_ptr<tabs::TabModel> tab =
        std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
    tabs::TabModel* tab_ptr = tab.get();
    tab_strip_collection->AddTabRecursive(std::move(tab), index, new_group_id,
                                          new_pinned_state);
    EXPECT_EQ(tab_ptr, tab_strip_collection->GetTabAtIndexRecursive(index));
  }

 private:
  std::unique_ptr<tabs::TabStripCollection> tab_strip_collection_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
};

TEST_F(TabStripCollectionTest, CollectionOperations) {
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  EXPECT_EQ(tab_strip_collection->ChildCount(), 2ul);

  EXPECT_TRUE(tab_strip_collection->ContainsCollection(
      tab_strip_collection->pinned_collection()));
  EXPECT_TRUE(tab_strip_collection->ContainsCollection(
      tab_strip_collection->unpinned_collection()));

  EXPECT_EQ(tab_strip_collection->GetIndexOfCollection(
                tab_strip_collection->pinned_collection()),
            0ul);
  EXPECT_EQ(tab_strip_collection->GetIndexOfCollection(
                tab_strip_collection->unpinned_collection()),
            1ul);

  EXPECT_EQ(tab_strip_collection->MaybeRemoveCollection(
                tab_strip_collection->unpinned_collection()),
            nullptr);
}

TEST_F(TabStripCollectionTest, GroupOperations) {
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  EXPECT_EQ(tab_strip_collection->ChildCount(), 2ul);

  tab_groups::TabGroupId group_two_id = tab_groups::TabGroupId::GenerateNew();
  TabGroupDesktop::Factory factory(profile());
  std::unique_ptr<tabs::TabGroupTabCollection> group_two =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, group_two_id, tab_groups::TabGroupVisualData());
  tabs::TabGroupTabCollection* group_two_ptr = group_two.get();

  EXPECT_EQ(nullptr, tab_strip_collection->GetTabGroupCollection(group_two_id));

  // TODO(crbug.com/332586827): Re-enable death testing.
  // Add group to pinned container.
  // EXPECT_DEATH_IF_SUPPORTED(
  //     tab_strip_collection->AddTabGroup(std::move(group_two), 2), "");

  // Add group to unpinned container.
  tab_strip_collection->InsertTabCollectionAt(std::move(group_two), 5, false,
                                              std::nullopt);
  EXPECT_EQ(group_two_ptr,
            tab_strip_collection->GetTabGroupCollection(group_two_id));
  EXPECT_EQ(group_two_ptr,
            tab_strip_collection->GetTabGroupCollection(group_two_id));
  EXPECT_EQ(1ul,
            tab_strip_collection->unpinned_collection()->GetIndexOfCollection(
                group_two_ptr));
  EXPECT_EQ(group_two_ptr,
            tab_strip_collection->RemoveTabCollection(group_two_ptr).get());
  EXPECT_EQ(nullptr, tab_strip_collection->GetTabGroupCollection(group_two_id));
}

TEST_F(TabStripCollectionTest, SplitOperations) {
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  tabs::PinnedTabCollection* pinned_collection =
      tab_strip_collection->pinned_collection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  // Get the group collection from the basic setup.
  tabs::TabGroupTabCollection* group_collection =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              unpinned_collection->GetTabCollectionStorageForTesting(), 2));

  auto createSplitAtIndices = [tab_strip_collection](std::vector<int> indices) {
    std::vector<tabs::TabInterface*> tabs;
    for (int i : indices) {
      tabs.push_back(tab_strip_collection->GetTabAtIndexRecursive(i));
    }
    split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
    tab_strip_collection->CreateSplit(
        split_id, tabs,
        split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                       0.5));
    return std::tuple{
        tabs, tab_strip_collection->GetSplitTabCollection(split_id), split_id};
  };

  // Add split to pinned container
  // 0p 1ps 2ps 3p 4u 5u 6ug 7ug 8u
  EXPECT_EQ(4ul, pinned_collection->ChildCount());
  EXPECT_EQ(4ul, pinned_collection->TabCountRecursive());

  auto [tabs, split, split_id] = createSplitAtIndices({1, 2});
  EXPECT_EQ(split_id, split->GetSplitTabId());
  EXPECT_EQ(3ul, pinned_collection->ChildCount());
  EXPECT_EQ(4ul, pinned_collection->TabCountRecursive());
  EXPECT_EQ(1ul, pinned_collection->GetIndexOfCollection(split));
  EXPECT_EQ(2ul, split->ChildCount());
  EXPECT_EQ(tabs, split->GetTabsRecursive());

  tab_strip_collection->Unsplit(split_id);
  EXPECT_EQ(nullptr, tab_strip_collection->GetSplitTabCollection(split_id));
  EXPECT_EQ(4ul, pinned_collection->ChildCount());
  EXPECT_EQ(4ul, pinned_collection->TabCountRecursive());

  // Add split to unpinned container.
  // 0p 1p 2p 3p 4us 5us 6ug 7ug 8u
  EXPECT_EQ(4ul, unpinned_collection->ChildCount());
  EXPECT_EQ(5ul, unpinned_collection->TabCountRecursive());

  std::tie(tabs, split, split_id) = createSplitAtIndices({4, 5});
  EXPECT_EQ(split_id, split->GetSplitTabId());
  EXPECT_EQ(3ul, unpinned_collection->ChildCount());
  EXPECT_EQ(5ul, unpinned_collection->TabCountRecursive());
  EXPECT_EQ(0ul, unpinned_collection->GetIndexOfCollection(split));
  EXPECT_EQ(2ul, split->ChildCount());
  EXPECT_EQ(tabs, split->GetTabsRecursive());

  tab_strip_collection->Unsplit(split_id);
  EXPECT_EQ(nullptr, tab_strip_collection->GetSplitTabCollection(split_id));
  EXPECT_EQ(4ul, unpinned_collection->ChildCount());
  EXPECT_EQ(5ul, unpinned_collection->TabCountRecursive());

  // Add split to group container.
  // 0p 1p 2p 3p 4u 5u 6ugs 7ugs 8u
  EXPECT_EQ(2ul, group_collection->ChildCount());
  EXPECT_EQ(2ul, group_collection->TabCountRecursive());

  std::tie(tabs, split, split_id) = createSplitAtIndices({6, 7});
  EXPECT_EQ(split_id, split->GetSplitTabId());
  EXPECT_EQ(1ul, group_collection->ChildCount());
  EXPECT_EQ(2ul, group_collection->TabCountRecursive());
  EXPECT_EQ(0ul, group_collection->GetIndexOfCollection(split));
  EXPECT_EQ(2ul, split->ChildCount());
  EXPECT_EQ(tabs, split->GetTabsRecursive());

  tab_strip_collection->Unsplit(split_id);
  EXPECT_EQ(nullptr, tab_strip_collection->GetSplitTabCollection(split_id));
  EXPECT_EQ(2ul, group_collection->ChildCount());
  EXPECT_EQ(2ul, group_collection->TabCountRecursive());
}

TEST_F(TabStripCollectionTest, RemoveAndInsertSplit) {
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  tabs::PinnedTabCollection* pinned_collection =
      tab_strip_collection->pinned_collection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  // Get the group collection from the basic setup.
  tabs::TabGroupTabCollection* group_collection =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              unpinned_collection->GetTabCollectionStorageForTesting(), 2));

  auto createSplitAtIndices = [tab_strip_collection](std::vector<int> indices) {
    std::vector<tabs::TabInterface*> tabs;
    for (int i : indices) {
      tabs.push_back(tab_strip_collection->GetTabAtIndexRecursive(i));
    }
    split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
    tab_strip_collection->CreateSplit(
        split_id, tabs,
        split_tabs::SplitTabVisualData(split_tabs::SplitTabLayout::kVertical,
                                       0.5));
    return std::tuple{
        tabs, tab_strip_collection->GetSplitTabCollection(split_id), split_id};
  };

  // Add split to pinned container
  // 0p 1ps 2ps 3p 4u 5u 6ug 7ug 8u
  EXPECT_EQ(4ul, pinned_collection->ChildCount());
  EXPECT_EQ(4ul, pinned_collection->TabCountRecursive());

  auto [tabs, split, split_id] = createSplitAtIndices({1, 2});
  EXPECT_EQ(split_id, split->GetSplitTabId());
  EXPECT_EQ(3ul, pinned_collection->ChildCount());
  EXPECT_EQ(4ul, pinned_collection->TabCountRecursive());
  EXPECT_EQ(1ul, pinned_collection->GetIndexOfCollection(split));
  EXPECT_EQ(2ul, split->ChildCount());
  EXPECT_EQ(tabs, split->GetTabsRecursive());

  // Remove split from pinned container
  // 0p 3p 4u 5u 6ug 7ug 8u
  std::unique_ptr<tabs::SplitTabCollection> removed_split_collection =
      base::WrapUnique(static_cast<tabs::SplitTabCollection*>(
          tab_strip_collection->RemoveTabCollection(split).release()));

  EXPECT_EQ(2ul, pinned_collection->TabCountRecursive());
  EXPECT_FALSE(
      tab_strip_collection->GetSplitTabCollection(split->GetSplitTabId()));

  // Insert back into pinned container
  // 0p 1ps 2ps 3p 4u 5u 6ug 7ug 8u
  tab_strip_collection->InsertTabCollectionAt(
      std::move(removed_split_collection), 1, true, std::nullopt);
  EXPECT_TRUE(
      tab_strip_collection->GetSplitTabCollection(split->GetSplitTabId()));
  EXPECT_EQ(4ul, pinned_collection->TabCountRecursive());

  // Remove split and insert into unpinned container
  // 0p 3p 4u 1s 2s 5u 6ug 7ug 8u

  removed_split_collection =
      base::WrapUnique(static_cast<tabs::SplitTabCollection*>(
          tab_strip_collection->RemoveTabCollection(split).release()));

  tab_strip_collection->InsertTabCollectionAt(
      std::move(removed_split_collection), 3, false, std::nullopt);
  EXPECT_EQ(7ul, unpinned_collection->TabCountRecursive());
  EXPECT_TRUE(
      tab_strip_collection->GetSplitTabCollection(split->GetSplitTabId()));

  // Remove split and insert into group container
  // 0p 3p 4u 5u 6ug 1gs 2gs 7ug 8u
  removed_split_collection =
      base::WrapUnique(static_cast<tabs::SplitTabCollection*>(
          tab_strip_collection->RemoveTabCollection(split).release()));

  tab_strip_collection->InsertTabCollectionAt(
      std::move(removed_split_collection), 5, false,
      group_collection->GetTabGroupId());
  EXPECT_EQ(7ul, unpinned_collection->TabCountRecursive());
  EXPECT_EQ(4ul, group_collection->TabCountRecursive());
  EXPECT_TRUE(
      tab_strip_collection->GetSplitTabCollection(split->GetSplitTabId()));
}

TEST_F(TabStripCollectionTest, TabOperations) {
  tabs::TabStripCollection* tab_strip_collection = GetCollection();

  tabs::PinnedTabCollection* pinned_collection =
      tab_strip_collection->pinned_collection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  tabs::TabCollectionStorage* pinned_storage =
      pinned_collection->GetTabCollectionStorageForTesting();
  tabs::TabCollectionStorage* unpinned_storage =
      unpinned_collection->GetTabCollectionStorageForTesting();

  // Add three tabs to the pinned collection.
  AddTabsToPinnedContainer(pinned_collection, GetTabStripModel(), 3);

  // Add one tab, a group with two tabs and another tab to the unpinned
  // collection.
  AppendTab(unpinned_collection, std::make_unique<tabs::TabModel>(
                                     MakeWebContents(), GetTabStripModel()));

  TabGroupDesktop::Factory factory(profile());
  std::unique_ptr<tabs::TabGroupTabCollection> group_one =
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, tab_groups::TabGroupId::GenerateNew(),
          tab_groups::TabGroupVisualData());
  tabs::TabGroupTabCollection* group_one_ptr = group_one.get();
  AddTabsToGroupContainer(group_one_ptr, GetTabStripModel(), 2);

  unpinned_collection->AddCollection(std::move(group_one), 1ul);
  AppendTab(unpinned_collection, std::make_unique<tabs::TabModel>(
                                     MakeWebContents(), GetTabStripModel()));

  std::unique_ptr<tabs::TabModel> tab_not_present =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabCollectionStorage* group_storage =
      group_one_ptr->GetTabCollectionStorageForTesting();

  // tab count test in tab strip.
  EXPECT_EQ(tab_strip_collection->TabCountRecursive(), 7ul);

  // tab contains test in tab strip.
  EXPECT_FALSE(
      ContainsTabRecursive(tab_strip_collection, tab_not_present.get()));
  EXPECT_TRUE(ContainsTabRecursive(
      tab_strip_collection, GetTabInCollectionStorage(pinned_storage, 2ul)));
  EXPECT_TRUE(ContainsTabRecursive(
      tab_strip_collection, GetTabInCollectionStorage(group_storage, 1ul)));
  EXPECT_FALSE(ContainsTab(tab_strip_collection,
                           GetTabInCollectionStorage(unpinned_storage, 2ul)));
  EXPECT_TRUE(ContainsTabRecursive(
      tab_strip_collection, GetTabInCollectionStorage(unpinned_storage, 2ul)));

  // tab recursive index test in tab strip.
  EXPECT_EQ(tab_strip_collection->GetIndexOfTabRecursive(
                GetTabInCollectionStorage(pinned_storage, 2ul)),
            2ul);
  EXPECT_EQ(tab_strip_collection->GetIndexOfTabRecursive(
                GetTabInCollectionStorage(group_storage, 0ul)),
            4ul);
  EXPECT_EQ(tab_strip_collection->GetIndexOfTabRecursive(
                GetTabInCollectionStorage(group_storage, 1ul)),
            5ul);
  EXPECT_EQ(tab_strip_collection->GetIndexOfTabRecursive(
                GetTabInCollectionStorage(unpinned_storage, 2ul)),
            6ul);

  // We cannot remove a tab as it is not a direct child of the tab strip
  // collection.
  EXPECT_EQ(tab_strip_collection->MaybeRemoveTab(
                GetTabInCollectionStorage(unpinned_storage, 2ul)),
            nullptr);
}

// Test for `AddTabRecursive`.
TEST_F(TabStripCollectionTest, RecursiveTabIndexOperationTests) {
  // Setup for the main collections.
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  tabs::PinnedTabCollection* pinned_collection =
      tab_strip_collection->pinned_collection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  // Get the group collection from the basic setup.
  tabs::TabGroupTabCollection* group_one_ptr =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              unpinned_collection->GetTabCollectionStorageForTesting(), 2));

  // Insert Recursive checks -
  // 1. Add to pinned container.
  TestAddTabRecursive(2, std::nullopt, true);
  EXPECT_EQ(pinned_collection->TabCountRecursive(), 5ul);
  // 2. Add as a tab to unpinned container. Now pinned container has 5 tabs.
  TestAddTabRecursive(5, std::nullopt, false);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 6ul);
  EXPECT_EQ(unpinned_collection->ChildCount(), 5ul);

  // 3. Add to the end of the unpinned container.
  TestAddTabRecursive(11, std::nullopt, false);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 7ul);
  EXPECT_EQ(unpinned_collection->ChildCount(), 6ul);

  // 4. Add to group container.
  TestAddTabRecursive(9, group_one_ptr->GetTabGroupId(), false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 3ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 8ul);

  // 5. Corner case add to boundary of group container.
  TestAddTabRecursive(8, group_one_ptr->GetTabGroupId(), false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 4ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 9ul);

  TestAddTabRecursive(8, std::nullopt, false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 4ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 10ul);

  // Now group has 4. And 4 unpinned before the group.
  TestAddTabRecursive(13, group_one_ptr->GetTabGroupId(), false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 5ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 11ul);

  TestAddTabRecursive(14, std::nullopt, false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 5ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 12ul);
}

TEST_F(TabStripCollectionTest, RecursiveRemoveTabAtIndex) {
  // Setup for the main collections.
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  tabs::PinnedTabCollection* pinned_collection =
      tab_strip_collection->pinned_collection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  // Get the group collection from the basic setup.
  tabs::TabGroupTabCollection* group_one_ptr =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              unpinned_collection->GetTabCollectionStorageForTesting(), 2ul));

  // Remove a pinned tab.
  tabs::TabInterface* tab_to_check =
      tab_strip_collection->GetTabAtIndexRecursive(2ul);
  EXPECT_EQ(tab_to_check,
            tab_strip_collection->RemoveTabAtIndexRecursive(2).get());
  EXPECT_EQ(pinned_collection->TabCountRecursive(), 3ul);

  // Remove an unpinned tab. Now there are 3 pinned tabs.
  tab_to_check = tab_strip_collection->GetTabAtIndexRecursive(4ul);
  EXPECT_EQ(tab_to_check,
            tab_strip_collection->RemoveTabAtIndexRecursive(4ul).get());
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 4ul);

  // Remove a grouped tab. Now there are 3 pinned tabs and 1 unpinned tab
  // before the group.
  tab_to_check = tab_strip_collection->GetTabAtIndexRecursive(4ul);
  EXPECT_EQ(tab_to_check,
            tab_strip_collection->RemoveTabAtIndexRecursive(4ul).get());
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 1ul);
}

// TODO(b/332586827): Re-enable death testing.
TEST_F(TabStripCollectionTest, DISABLED_RecursiveTabAddBadInput) {
  // Setup for the main collections.
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();

  // Try to add an index OOB
  EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->AddTabRecursive(
                                std::make_unique<tabs::TabModel>(
                                    MakeWebContents(), GetTabStripModel()),
                                20ul, std::nullopt, false),
                            "");

  // Try to add a pinned tab to unpinned container index location.
  EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->AddTabRecursive(
                                std::make_unique<tabs::TabModel>(
                                    MakeWebContents(), GetTabStripModel()),
                                5ul, std::nullopt, true),
                            "");

  // Try to add a unpinned tab to pinned container index location.
  EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->AddTabRecursive(
                                std::make_unique<tabs::TabModel>(
                                    MakeWebContents(), GetTabStripModel()),
                                1ul, std::nullopt, false),
                            "");

  // Try to add a tab to pinned container index location.
  EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->AddTabRecursive(
                                std::make_unique<tabs::TabModel>(
                                    MakeWebContents(), GetTabStripModel()),
                                1ul, std::nullopt, false),
                            "");

  // Try to add a tab to pinned container index location with a group.
  tabs::TabGroupTabCollection* group_one_ptr =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              GetCollection()
                  ->unpinned_collection()
                  ->GetTabCollectionStorageForTesting(),
              2));
  EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->AddTabRecursive(
                                std::make_unique<tabs::TabModel>(
                                    MakeWebContents(), GetTabStripModel()),
                                1ul, group_one_ptr->GetTabGroupId(), true),
                            "");

  // Try to add a tab to unpinned container index that should not be a part of a
  // group but a group value is passed.
  EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->AddTabRecursive(
                                std::make_unique<tabs::TabModel>(
                                    MakeWebContents(), GetTabStripModel()),
                                5ul, group_one_ptr->GetTabGroupId(), true),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(
      tab_strip_collection->AddTabRecursive(
          std::make_unique<tabs::TabModel>(MakeWebContents(),
                                           GetTabStripModel()),
          6ul, tab_groups::TabGroupId::GenerateNew(), true),
      "");

  // Try to add a tab to unpinned container index that should not be a part of a
  // group but a different group id.
  EXPECT_DEATH_IF_SUPPORTED(
      tab_strip_collection->AddTabRecursive(
          std::make_unique<tabs::TabModel>(MakeWebContents(),
                                           GetTabStripModel()),
          7ul, tab_groups::TabGroupId::GenerateNew(), true),
      "");
}

TEST_F(TabStripCollectionTest, UpdateProperties) {
  // Setup for the main collections.
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  tabs::PinnedTabCollection* pinned_collection =
      tab_strip_collection->pinned_collection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  // Get the group collection from the basic setup.
  tabs::TabGroupTabCollection* group_collection =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              unpinned_collection->GetTabCollectionStorageForTesting(), 2ul));

  std::unique_ptr<tabs::TabModel> tab_model =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_ptr = tab_model.get();
  ASSERT_FALSE(tab_model_ptr->IsPinned());
  ASSERT_FALSE(tab_model_ptr->IsSplit());
  ASSERT_EQ(std::nullopt, tab_model_ptr->GetGroup());

  // Move to pinned collection.
  ASSERT_EQ(tab_model_ptr, AppendTab(pinned_collection, std::move(tab_model)));
  EXPECT_TRUE(tab_model_ptr->IsPinned());
  EXPECT_FALSE(tab_model_ptr->IsSplit());
  EXPECT_EQ(std::nullopt, tab_model_ptr->GetGroup());

  // Move to group collection.
  ASSERT_EQ(tab_model_ptr,
            AppendTab(group_collection,
                      pinned_collection->MaybeRemoveTab(tab_model_ptr)));
  EXPECT_FALSE(tab_model_ptr->IsPinned());
  EXPECT_FALSE(tab_model_ptr->IsSplit());
  EXPECT_EQ(group_collection->GetTabGroupId(), tab_model_ptr->GetGroup());

  // Move to split collection.
  tabs::SplitTabCollection* split_collection =
      unpinned_collection->AddCollection(
          std::make_unique<tabs::SplitTabCollection>(
              split_tabs::SplitTabId::GenerateNew(),
              split_tabs::SplitTabVisualData(
                  split_tabs::SplitTabLayout::kVertical, 0.5)),
          unpinned_collection->ChildCount());
  AppendTab(split_collection, std::make_unique<tabs::TabModel>(
                                  MakeWebContents(), GetTabStripModel()));
  ASSERT_EQ(tab_model_ptr,
            AppendTab(split_collection,
                      group_collection->MaybeRemoveTab(tab_model_ptr)));
  EXPECT_FALSE(tab_model_ptr->IsPinned());
  EXPECT_EQ(split_collection->GetSplitTabId(), tab_model_ptr->GetSplit());
  EXPECT_EQ(std::nullopt, tab_model_ptr->GetGroup());

  // Move split collection to pinned collection
  ASSERT_EQ(split_collection,
            pinned_collection->AddCollection(
                unpinned_collection->MaybeRemoveCollection(split_collection),
                pinned_collection->ChildCount()));
  EXPECT_TRUE(tab_model_ptr->IsPinned());
  EXPECT_EQ(split_collection->GetSplitTabId(), tab_model_ptr->GetSplit());
  EXPECT_EQ(std::nullopt, tab_model_ptr->GetGroup());

  // Move split collection to group collection
  ASSERT_EQ(split_collection,
            group_collection->AddCollection(
                pinned_collection->MaybeRemoveCollection(split_collection),
                group_collection->ChildCount()));
  EXPECT_FALSE(tab_model_ptr->IsPinned());
  EXPECT_EQ(split_collection->GetSplitTabId(), tab_model_ptr->GetSplit());
  EXPECT_EQ(group_collection->GetTabGroupId(), tab_model_ptr->GetGroup());
}

TEST_F(TabStripCollectionTest, ValidateData) {
  // Setup for the main collections.
  PerformBasicSetup();
  tabs::TabStripCollection* tab_strip_collection = GetCollection();
  tabs::UnpinnedTabCollection* unpinned_collection =
      tab_strip_collection->unpinned_collection();

  // Get the group collection from the basic setup.
  tabs::TabGroupTabCollection* group_one_ptr =
      static_cast<tabs::TabGroupTabCollection*>(
          GetCollectionInCollectionStorage(
              unpinned_collection->GetTabCollectionStorageForTesting(), 2ul));

  tab_strip_collection->ValidateData();

  tab_groups::TabGroupId group_two_id = tab_groups::TabGroupId::GenerateNew();
  TabGroupDesktop::Factory factory(profile());
  tab_strip_collection->CreateTabGroup(
      std::make_unique<tabs::TabGroupTabCollection>(
          factory, group_two_id, tab_groups::TabGroupVisualData()));
  // TODO(crbug.com/332586827): Re-enable death testing.
  // EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->ValidateData(), "");

  tab_strip_collection->CloseDetachedTabGroup(group_two_id);
  tab_strip_collection->ValidateData();

  ASSERT_EQ(group_one_ptr->ChildCount(), 2ul);
  group_one_ptr->MaybeRemoveTab(group_one_ptr->GetTabAtIndexRecursive(0))
      .reset();
  group_one_ptr->MaybeRemoveTab(group_one_ptr->GetTabAtIndexRecursive(0))
      .reset();
  ASSERT_EQ(group_one_ptr->ChildCount(), 0ul);
  // TODO(crbug.com/332586827): Re-enable death testing.
  // EXPECT_DEATH_IF_SUPPORTED(tab_strip_collection->ValidateData(), "");
}
