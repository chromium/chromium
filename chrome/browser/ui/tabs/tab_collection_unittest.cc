// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/tabs/pinned_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/unpinned_tab_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabCollectionBaseTest : public ::testing::Test {
 public:
  TabCollectionBaseTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kTabStripCollectionStorage}, {});
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());
  }
  TabCollectionBaseTest(const TabCollectionBaseTest&) = delete;
  TabCollectionBaseTest& operator=(const TabCollectionBaseTest&) = delete;
  ~TabCollectionBaseTest() override = default;

  TabStripModel* GetTabStripModel() { return tab_strip_model_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
};

class PinnedTabCollectionTest : public TabCollectionBaseTest {
 public:
  PinnedTabCollectionTest() {
    pinned_collection_ = std::make_unique<tabs::PinnedTabCollection>();
  }
  PinnedTabCollectionTest(const PinnedTabCollectionTest&) = delete;
  PinnedTabCollectionTest& operator=(const PinnedTabCollectionTest&) = delete;
  ~PinnedTabCollectionTest() override { pinned_collection_.reset(); }

  tabs::PinnedTabCollection* GetPinnedCollection() {
    return pinned_collection_.get();
  }

  void AddTabs(int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
      tabs::TabModel* tab_model_ptr = tab_model.get();
      GetPinnedCollection()->AppendTab(std::move(tab_model));
      EXPECT_EQ(GetPinnedCollection()->GetIndexOfTabRecursive(tab_model_ptr),
                GetPinnedCollection()->ChildCount() - 1);
    }
  }

 private:
  // TODO(shibalik): Create a parent collection after implementation of
  // TabStripCollection.
  std::unique_ptr<tabs::PinnedTabCollection> pinned_collection_;
};

TEST_F(PinnedTabCollectionTest, AddOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  auto tab_model_two =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabModel* tab_model_two_ptr = tab_model_two.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  tabs::PinnedTabCollection* pinned_collection = GetPinnedCollection();

  pinned_collection->AppendTab(std::move(tab_model_one));
  EXPECT_TRUE(tab_model_one_ptr->pinned());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            pinned_collection);

  EXPECT_TRUE(pinned_collection->ContainsTabRecursive(tab_model_one_ptr));

  AddTabs(4);

  EXPECT_EQ(pinned_collection->ChildCount(), 5ul);
  EXPECT_EQ(pinned_collection->TabCountRecursive(), 5ul);

  pinned_collection->AddTab(std::move(tab_model_two), 3ul);
  EXPECT_EQ(pinned_collection->GetIndexOfTabRecursive(tab_model_two_ptr), 3ul);
}

TEST_F(PinnedTabCollectionTest, RemoveOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::PinnedTabCollection* pinned_collection = GetPinnedCollection();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  pinned_collection->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(pinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 3ul);
  EXPECT_EQ(pinned_collection->ChildCount(), 5ul);
  EXPECT_TRUE(tab_model_one_ptr->pinned());

  auto removed_tab_model = pinned_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_FALSE(removed_tab_model->pinned());
  EXPECT_FALSE(removed_tab_model->GetParentCollectionForTesting());

  EXPECT_EQ(pinned_collection->ChildCount(), 4ul);
  EXPECT_EQ(removed_tab_model.get(), tab_model_one_ptr);
}

TEST_F(PinnedTabCollectionTest, MoveOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::PinnedTabCollection* pinned_collection = GetPinnedCollection();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  pinned_collection->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(pinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 3ul);
  EXPECT_EQ(pinned_collection->ChildCount(), 5ul);

  pinned_collection->MoveTab(tab_model_one_ptr, 1ul);
  EXPECT_EQ(pinned_collection->ChildCount(), 5ul);
  EXPECT_EQ(pinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 1ul);

  pinned_collection->MoveTab(tab_model_one_ptr, 4ul);
  EXPECT_EQ(pinned_collection->ChildCount(), 5ul);
  EXPECT_EQ(pinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 4ul);
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

  void AddTabs(int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
      tabs::TabModel* tab_model_ptr = tab_model.get();
      GetCollection()->AppendTab(std::move(tab_model));
      EXPECT_EQ(GetCollection()->GetIndexOfTabRecursive(tab_model_ptr),
                GetCollection()->ChildCount() - 1);
    }
  }

 private:
  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection_;
};

TEST_F(TabGroupTabCollectionTest, AddOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  auto tab_model_two =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabModel* tab_model_two_ptr = tab_model_two.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  tabs::TabGroupTabCollection* grouped_collection = GetCollection();

  grouped_collection->AppendTab(std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->group(), grouped_collection->GetTabGroupId());
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            grouped_collection);
  EXPECT_TRUE(grouped_collection->ContainsTabRecursive(tab_model_one_ptr));

  AddTabs(4);

  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);
  EXPECT_EQ(grouped_collection->TabCountRecursive(), 5ul);

  grouped_collection->AddTab(std::move(tab_model_two), 3ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_two_ptr), 3ul);
}

TEST_F(TabGroupTabCollectionTest, RemoveOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::TabGroupTabCollection* grouped_collection = GetCollection();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  grouped_collection->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 3ul);
  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);

  EXPECT_EQ(tab_model_one_ptr->group(), grouped_collection->GetTabGroupId());
  auto removed_tab_model =
      grouped_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_FALSE(removed_tab_model->group().has_value());
  EXPECT_FALSE(removed_tab_model->GetParentCollectionForTesting());
  EXPECT_EQ(grouped_collection->ChildCount(), 4ul);
  EXPECT_EQ(removed_tab_model.get(), tab_model_one_ptr);
}

TEST_F(TabGroupTabCollectionTest, MoveOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::TabGroupTabCollection* grouped_collection = GetCollection();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  grouped_collection->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 3ul);
  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);

  grouped_collection->MoveTab(tab_model_one_ptr, 1ul);
  EXPECT_EQ(grouped_collection->ChildCount(), 5ul);
  EXPECT_EQ(grouped_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 1ul);

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

  void AddTabs(int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
      GetCollection()->AppendTab(std::move(tab_model));
    }
  }

 private:
  std::unique_ptr<tabs::UnpinnedTabCollection> unpinned_collection_;
};

TEST_F(UnpinnedTabCollectionTest, AddOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();

  EXPECT_FALSE(tab_model_one_ptr->GetParentCollectionForTesting());
  EXPECT_FALSE(tab_group_one_ptr->GetParentCollection());
  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  unpinned_collection->AppendTab(std::move(tab_model_one));
  EXPECT_EQ(tab_model_one_ptr->GetParentCollectionForTesting(),
            unpinned_collection);
  EXPECT_TRUE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));

  AddTabs(4);

  unpinned_collection->AddTabGroup(std::move(tab_group_one), 2ul);
  EXPECT_EQ(tab_group_one_ptr->GetParentCollection(), unpinned_collection);
  EXPECT_TRUE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  EXPECT_TRUE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr),
            0ul);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 2ul);
  EXPECT_EQ(unpinned_collection->GetTabGroupCollection(group_id),
            tab_group_one_ptr);

  auto tab_model_in_group =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_in_group_ptr = tab_model_in_group.get();

  tab_group_one_ptr->AppendTab(
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel()));
  tab_group_one_ptr->AppendTab(std::move(tab_model_in_group));

  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_in_group_ptr),
            3);
  EXPECT_EQ(unpinned_collection->ChildCount(), 6ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 7ul);
}

TEST_F(UnpinnedTabCollectionTest, RemoveOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();

  // Add two tabs to the group
  tab_group_one_ptr->AppendTab(
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel()));
  tab_group_one_ptr->AppendTab(
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel()));

  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  AddTabs(4);
  unpinned_collection->AddTab(std::move(tab_model_one), 2ul);
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 4ul);

  // Remove the tab
  std::unique_ptr<tabs::TabModel> removed_tab =
      unpinned_collection->MaybeRemoveTab(tab_model_one_ptr);
  EXPECT_EQ(removed_tab.get(), tab_model_one_ptr);
  EXPECT_EQ(unpinned_collection->ChildCount(), 5ul);
  EXPECT_FALSE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 6ul);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 3ul);

  // Remove the collection
  std::unique_ptr<tabs::TabCollection> removed_collection =
      unpinned_collection->MaybeRemoveCollection(tab_group_one_ptr);
  EXPECT_EQ(removed_collection.get(), tab_group_one_ptr);
  EXPECT_EQ(unpinned_collection->ChildCount(), 4ul);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 4ul);
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  EXPECT_EQ(removed_collection->GetParentCollection(), nullptr);

  // Add the collection again and close it.
  unpinned_collection->AddTabGroup(
      std::unique_ptr<tabs::TabGroupTabCollection>(
          static_cast<tabs::TabGroupTabCollection*>(
              removed_collection.release())),
      2);
  EXPECT_TRUE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 6ul);

  unpinned_collection->CloseTabGroup(tab_group_one_ptr);
  EXPECT_EQ(unpinned_collection->TabCountRecursive(), 4ul);
  EXPECT_FALSE(unpinned_collection->ContainsCollection(tab_group_one_ptr));
}

TEST_F(UnpinnedTabCollectionTest, CloseTabOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  unpinned_collection->AddTab(std::move(tab_model_one), 0ul);
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 1ul);
  AddTabs(4);

  EXPECT_TRUE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
  unpinned_collection->CloseTab(tab_model_one_ptr);
  EXPECT_FALSE(unpinned_collection->ContainsTabRecursive(tab_model_one_ptr));
}

TEST_F(UnpinnedTabCollectionTest, MoveOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  auto tab_group_one = std::make_unique<tabs::TabGroupTabCollection>(group_id);

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabGroupTabCollection* tab_group_one_ptr = tab_group_one.get();
  tab_group_one_ptr->AppendTab(
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel()));
  tab_group_one_ptr->AppendTab(
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel()));

  tabs::UnpinnedTabCollection* unpinned_collection = GetCollection();

  AddTabs(4);
  unpinned_collection->AddTab(std::move(tab_model_one), 1ul);
  unpinned_collection->AddTabGroup(std::move(tab_group_one), 3ul);

  unpinned_collection->MoveTab(tab_model_one_ptr, 3);
  unpinned_collection->MoveTabGroup(tab_group_one_ptr, 0);

  EXPECT_EQ(unpinned_collection->GetIndexOfTabRecursive(tab_model_one_ptr), 4);
  EXPECT_EQ(unpinned_collection->GetIndexOfCollection(tab_group_one_ptr), 0);
}
