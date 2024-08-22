// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_collection.h"

#include <cstddef>
#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/pinned_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/tabs/unpinned_tab_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabCollectionBaseTest : public ::testing::Test {
 public:
  TabCollectionBaseTest() {
    scoped_feature_list_.InitWithFeatures({tabs::kTabStripCollectionStorage},
                                          {});
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());
  }
  TabCollectionBaseTest(const TabCollectionBaseTest&) = delete;
  TabCollectionBaseTest& operator=(const TabCollectionBaseTest&) = delete;
  ~TabCollectionBaseTest() override = default;

  TabStripModel* GetTabStripModel() { return tab_strip_model_.get(); }

  tabs::TabModel* GetTabInCollectionStorage(tabs::TabCollectionStorage* storage,
                                            size_t index) {
    const auto& child = storage->GetChildren().at(index);
    const auto tab_ptr = std::get_if<std::unique_ptr<tabs::TabModel>>(&child);
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

  void AddTabsToPinnedContainer(tabs::PinnedTabCollection* collection,
                                TabStripModel* tab_strip_model,
                                int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(MakeWebContents(), tab_strip_model);
      tabs::TabModel* tab_model_ptr = tab_model.get();
      collection->AppendTab(std::move(tab_model));
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
      collection->AppendTab(std::move(tab_model));
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
      collection->AppendTab(std::move(tab_model));
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::RenderViewHostTestEnabler test_enabler_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  tabs::PreventTabFeatureInitialization prevent_;
};

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
  pinned_collection_instance->AppendTab(std::move(tab_model_one));
  EXPECT_TRUE(tab_model_one_ptr->pinned());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            pinned_collection_instance);

  EXPECT_TRUE(
      pinned_collection_instance->ContainsTabRecursive(tab_model_one_ptr));

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
  EXPECT_TRUE(tab_model_one_ptr->pinned());

  // Remove `tab_model_one` from the collection.
  auto removed_tab_model =
      pinned_collection_instance->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_FALSE(removed_tab_model->pinned());
  EXPECT_FALSE(removed_tab_model->GetParentCollectionForTesting());

  EXPECT_EQ(pinned_collection_instance->ChildCount(), 4ul);
  EXPECT_EQ(removed_tab_model.get(), tab_model_one_ptr);
}

TEST_F(PinnedTabCollectionTest, CloseTabOperation) {
  // Setup phase of keeping track of a tab.
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::PinnedTabCollection* pinned_collection_instance = pinned_collection();
  pinned_collection_instance->AppendTab(std::move(tab_model_one));
  EXPECT_EQ(pinned_collection_instance->ChildCount(), 1ul);

  // Remove `tab_model_one` from the collection.
  pinned_collection_instance->CloseTab(tab_model_one_ptr);
  EXPECT_EQ(pinned_collection_instance->ChildCount(), 0ul);
}

TEST_F(PinnedTabCollectionTest, CollectionOperationsIsNoop) {
  // Setup phase of keeping track of a tab.
  tabs::PinnedTabCollection* pinned_collection_instance = pinned_collection();

  // Add four tabs to the collection.
  AddTabsToPinnedContainer(pinned_collection_instance, GetTabStripModel(), 4);

  std::unique_ptr<tabs::TabCollection> collection =
      std::make_unique<tabs::UnpinnedTabCollection>();
  EXPECT_EQ(pinned_collection_instance->MaybeRemoveCollection(collection.get()),
            nullptr);
  EXPECT_EQ(pinned_collection_instance->GetIndexOfCollection(collection.get()),
            std::nullopt);
}

TEST_F(PinnedTabCollectionTest, MoveOperation) {
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

  // Move `tab_model_one` to index 1.
  pinned_collection_instance->MoveTab(tab_model_one_ptr, 1ul);
  EXPECT_EQ(pinned_collection_instance->ChildCount(), 5ul);
  EXPECT_EQ(
      pinned_collection_instance->GetIndexOfTabRecursive(tab_model_one_ptr),
      1ul);

  // Move `tab_model_one` to index 4.
  pinned_collection_instance->MoveTab(tab_model_one_ptr, 4ul);
  EXPECT_EQ(pinned_collection_instance->ChildCount(), 5ul);
  EXPECT_EQ(
      pinned_collection_instance->GetIndexOfTabRecursive(tab_model_one_ptr),
      4ul);
}

class TabGroupTabCollectionTest : public TabCollectionBaseTest {
 public:
  TabGroupTabCollectionTest() {
    grouped_collection_ = std::make_unique<tabs::TabGroupTabCollection>(
        tab_groups::TabGroupId::GenerateNew());
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
  tabs::PreventTabFeatureInitialization prevent_;
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
  grouped_collection->AppendTab(std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->group(), grouped_collection->GetTabGroupId());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            grouped_collection);
  EXPECT_TRUE(grouped_collection->ContainsTabRecursive(tab_model_one_ptr));

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

  // Remove `tab_model_one` from the collection.
  auto removed_tab_model =
      grouped_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_FALSE(removed_tab_model->group().has_value());
  EXPECT_FALSE(removed_tab_model->GetParentCollectionForTesting());
  EXPECT_EQ(grouped_collection->ChildCount(), 4ul);
  EXPECT_EQ(removed_tab_model.get(), tab_model_one_ptr);
}

TEST_F(TabGroupTabCollectionTest, MoveOperation) {
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

  // Move `tab_model_one` to index 1.
  grouped_collection->MoveTab(tab_model_one_ptr, 1ul);
  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 1ul);

  // Move `tab_model_one` to index 4.
  grouped_collection->MoveTab(tab_model_one_ptr, 4ul);
  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 4ul);
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
    auto tab_group_one =
        std::make_unique<tabs::TabGroupTabCollection>(group_id);
    AddTabsToGroupContainer(tab_group_one.get(), GetTabStripModel(), 2);
    GetCollection()->AddTabGroup(std::move(tab_group_one), 2);
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
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  EXPECT_FALSE(tab_group_one_ptr->GetParentCollection());
  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the `tab_model_one` to the collection.
  unpinned_collection->AppendTab(std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            unpinned_collection);
  EXPECT_TRUE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));

  // Add a group to the collection at index 2.
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 2ul);
  EXPECT_EQ(tab_group_one_ptr->GetParentCollection(), unpinned_collection);
  EXPECT_TRUE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  EXPECT_TRUE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr),
            6ul);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 2ul);
  EXPECT_EQ(unpinned_collection->GetTabGroupCollection(group_id),
            tab_group_one_ptr);

  auto tab_model_in_group =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabModel* tab_model_in_group_ptr = tab_model_in_group.get();

  // Add tabs to the group and validate index and size. Track one of the tabs.
  tab_group_one_ptr->AppendTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()));
  tab_group_one_ptr->AppendTab(std::move(tab_model_in_group));

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
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();

  // Add two tabs to the group
  AddTabsToGroupContainer(tab_group_one_ptr, GetTabStripModel(), 2);

  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the tab and the group at index 2 and index 4 respectively.
  unpinned_collection->AddTab(std::move(tab_model_one), 2ul);
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 4ul);

  // Remove the tab
  std::unique_ptr<tabs::TabModel> removed_tab =
      unpinned_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_EQ(removed_tab.get(), tab_model_one_ptr);
  EXPECT_EQ(unpinned_collection->ChildCount(), 6ul);
  EXPECT_FALSE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
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

TEST_F(UnpinnedTabCollectionTest, CloseTabOperation) {
  // Use the basic setup scenario and track a tab.
  PerformBasicSetup();
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the tab and the group to the collection.
  unpinned_collection->AddTab(std::move(tab_model_one), 0ul);

  EXPECT_TRUE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  unpinned_collection->CloseTab(tab_model_one_ptr);
  EXPECT_FALSE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
}

TEST_F(UnpinnedTabCollectionTest, CloseGroupOperation) {
  // Use the basic setup scenario and track a group.
  PerformBasicSetup();
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();
  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the tab and the group to the collection.
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 2ul);

  EXPECT_TRUE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  unpinned_collection->CloseTabGroup(tab_group_one_ptr);
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
}

TEST_F(UnpinnedTabCollectionTest, MoveOperation) {
  // Use the basic setup scenario and track a tab and a group.
  PerformBasicSetup();
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();
  AddTabsToGroupContainer(tab_group_one_ptr, GetTabStripModel(), 2);

  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  // Add the tab and the group.
  unpinned_collection->AddTab(std::move(tab_model_one), 1ul);
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 3ul);

  // Move the tab to index 3. Followed by the group to index 0.
  unpinned_collection->MoveTab(tab_model_one_ptr, 3);
  unpinned_collection->MoveTabGroup(tab_group_one_ptr, 0);

  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 4);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 0);
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
    std::unique_ptr<tabs::TabGroupTabCollection> group_one =
        std::make_unique<tabs::TabGroupTabCollection>(
            tab_groups::TabGroupId::GenerateNew());
    tabs::TabGroupTabCollection* group_one_ptr = group_one.get();
    AddTabsToGroupContainer(group_one_ptr, GetTabStripModel(), 2);
    unpinned_collection->AddTabGroup(std::move(group_one), 2);

    // Add one more tab.
    unpinned_collection->AppendTab(std::make_unique<tabs::TabModel>(
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

 private:
  std::unique_ptr<tabs::TabStripCollection> tab_strip_collection_;
  tabs::PreventTabFeatureInitialization prevent_;
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
  unpinned_collection->AppendTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()));

  std::unique_ptr<tabs::TabGroupTabCollection> group_one =
      std::make_unique<tabs::TabGroupTabCollection>(
          tab_groups::TabGroupId::GenerateNew());
  tabs::TabGroupTabCollection* group_one_ptr = group_one.get();
  AddTabsToGroupContainer(group_one_ptr, GetTabStripModel(), 2);

  unpinned_collection->AddTabGroup(std::move(group_one), 1ul);
  unpinned_collection->AppendTab(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()));

  std::unique_ptr<tabs::TabModel> tab_not_present =
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel());
  tabs::TabCollectionStorage* group_storage =
      group_one_ptr->GetTabCollectionStorageForTesting();

  // tab count test in tab strip.
  EXPECT_EQ(tab_strip_collection->TabCountRecursive(), 7ul);

  // tab contains test in tab strip.
  EXPECT_FALSE(
      tab_strip_collection->ContainsTabRecursive(tab_not_present.get()));
  EXPECT_TRUE(tab_strip_collection->ContainsTabRecursive(
      GetTabInCollectionStorage(pinned_storage, 2ul)));
  EXPECT_TRUE(tab_strip_collection->ContainsTabRecursive(
      GetTabInCollectionStorage(group_storage, 1ul)));
  EXPECT_FALSE(tab_strip_collection->ContainsTab(
      GetTabInCollectionStorage(unpinned_storage, 2ul)));
  EXPECT_TRUE(tab_strip_collection->ContainsTabRecursive(
      GetTabInCollectionStorage(unpinned_storage, 2ul)));

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
  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      2, std::nullopt, true);
  EXPECT_EQ(pinned_collection->TabCountRecursive(), 5ul);
  // 2. Add as a tab to unpinned container. Now pinned container has 5 tabs.
  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      5, std::nullopt, false);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 6ul);
  EXPECT_EQ(unpinned_collection->ChildCount(), 5ul);

  // 3. Add to the end of the unpinned container.
  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      11, std::nullopt, false);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 7ul);
  EXPECT_EQ(unpinned_collection->ChildCount(), 6ul);

  // 4. Add to group container.
  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      9, group_one_ptr->GetTabGroupId(), false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 3ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 8ul);

  // 5. Corner case add to boundary of group container.
  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      8, group_one_ptr->GetTabGroupId(), false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 4ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 9ul);

  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      8, std::nullopt, false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 4ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 10ul);

  // Now group has 4. And 4 unpinned before the group.
  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      13, group_one_ptr->GetTabGroupId(), false);
  EXPECT_EQ(group_one_ptr->TabCountRecursive(), 5ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 11ul);

  tab_strip_collection->AddTabRecursive(
      std::make_unique<tabs::TabModel>(MakeWebContents(), GetTabStripModel()),
      14, std::nullopt, false);
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
  tabs::TabModel* tab_to_check =
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
