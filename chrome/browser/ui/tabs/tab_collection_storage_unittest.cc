// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/tabs/pinned_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection.h"
#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_group_tab_collection.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabCollectionStorageTest : public ::testing::Test {
 public:
  TabCollectionStorageTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kTabStripCollectionStorage}, {});
    pinned_collection_ = std::make_unique<tabs::PinnedTabCollection>();
    testing_profile_ = std::make_unique<TestingProfile>();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), testing_profile_.get());
  }
  TabCollectionStorageTest(const TabCollectionStorageTest&) = delete;
  TabCollectionStorageTest& operator=(const TabCollectionStorageTest&) = delete;
  ~TabCollectionStorageTest() override { pinned_collection_.reset(); }

  tabs::TabCollectionStorage* GetTabCollectionStorage() {
    return pinned_collection_->GetTabCollectionStorageForTesting();
  }

  TabStripModel* GetTabStripModel() { return tab_strip_model_.get(); }

  void AddTabs(int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<tabs::TabModel> tab_model =
          std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
      tabs::TabModel* tab_model_ptr = tab_model.get();

      tabs::TabModel* inserted_tab_model_ptr =
          GetTabCollectionStorage()->AddTab(
              std::move(tab_model),
              GetTabCollectionStorage()->GetChildrenCount());
      EXPECT_EQ(tab_model_ptr, inserted_tab_model_ptr);
      EXPECT_EQ(GetTabCollectionStorage()->GetIndexOfTab(tab_model_ptr),
                GetTabCollectionStorage()->GetChildrenCount() - 1);
    }
  }

  void SetTabID(tabs::TabModel* tab_model, int id) {
    tab_handle_to_id_map_[tab_model->GetHandle()] = id;
  }

  void ResetTabIDs(int start) {
    int i = 0;
    const auto& children = GetTabCollectionStorage()->GetChildren();
    for (const auto& child : children) {
      if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
        SetTabID(std::get<std::unique_ptr<tabs::TabModel>>(child).get(),
                 start + i);
        i += 1;
      }
    }
  }

  std::vector<int> TabIDString() {
    std::vector<int> res;
    const auto& children = GetTabCollectionStorage()->GetChildren();
    for (const auto& child : children) {
      if (std::holds_alternative<std::unique_ptr<tabs::TabModel>>(child)) {
        tabs::TabModel* tab_model =
            std::get<std::unique_ptr<tabs::TabModel>>(child).get();
        res.push_back(tab_handle_to_id_map_[tab_model->GetHandle()]);
      }
    }
    return res;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<tabs::PinnedTabCollection> pinned_collection_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<Profile> testing_profile_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::map<tabs::TabHandle, int> tab_handle_to_id_map_;
};

TEST_F(TabCollectionStorageTest, AddTabOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  auto tab_model_two =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());

  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();
  tabs::TabModel* tab_model_two_ptr = tab_model_two.get();

  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();
  collection_storage->AddTab(std::move(tab_model_one), 0);

  EXPECT_TRUE(collection_storage->ContainsTab(tab_model_one_ptr));
  EXPECT_FALSE(collection_storage->ContainsTab(tab_model_two.get()));

  // Add four more tabs.
  AddTabs(4);
  ResetTabIDs(0);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);

  // Annotate `tab_model_two_ptr` with an id of 5.
  SetTabID(tab_model_two_ptr, 5);
  collection_storage->AddTab(std::move(tab_model_two), 3ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_two_ptr), 3ul);
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 5, 3, 4}));
}

TEST_F(TabCollectionStorageTest, RemoveTabOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  collection_storage->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  ResetTabIDs(0);

  auto removed_tab_model = collection_storage->RemoveTab(tab_model_one_ptr);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 4ul);
  EXPECT_EQ(removed_tab_model.get(), tab_model_one_ptr);
  // `tab_model_one_ptr` was removed from index 3.
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 4}));
}

TEST_F(TabCollectionStorageTest, CloseTabOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  collection_storage->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  ResetTabIDs(0);

  collection_storage->CloseTab(tab_model_one_ptr);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 4ul);
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 4}));
}

TEST_F(TabCollectionStorageTest, MoveTabOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  collection_storage->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 3ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  ResetTabIDs(0);

  collection_storage->MoveTab(tab_model_one_ptr, 1ul);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 1ul);
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 3, 1, 2, 4}));

  collection_storage->MoveTab(tab_model_one_ptr, 4ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 4ul);
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 4, 3}));
}

// TODO(b/327925372): Re-enable the test.
TEST_F(TabCollectionStorageTest, DISABLED_InvalidArgumentsTabOperations) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();
  std::unique_ptr<tabs::TabModel> empty_ptr;

  EXPECT_DEATH(
      collection_storage->AddTab(
          std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel()), 10ul),
      "");
  EXPECT_DEATH(collection_storage->AddTab(std::move(empty_ptr), 1ul), "");

  EXPECT_DEATH(
      {
        std::unique_ptr<tabs::TabModel> tab_model =
            collection_storage->RemoveTab(tab_model_one.get());
      },
      "");
  EXPECT_DEATH(
      {
        std::unique_ptr<tabs::TabModel> tab_model =
            collection_storage->RemoveTab(nullptr);
      },
      "");

  EXPECT_DEATH(collection_storage->MoveTab(tab_model_one.get(), 0ul), "");
  collection_storage->AddTab(std::move(tab_model_one), 0ul);
  EXPECT_DEATH(collection_storage->MoveTab(tab_model_one.get(), 10ul), "");
  EXPECT_DEATH(collection_storage->MoveTab(nullptr, 10ul), "");
}

TEST_F(TabCollectionStorageTest, AddMixedTabAndCollectionOperation) {
  auto tab_collection_one = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());
  auto tab_collection_two = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());

  tabs::TabCollection* tab_collection_one_ptr = tab_collection_one.get();
  tabs::TabCollection* tab_collection_two_ptr = tab_collection_two.get();

  // This is the top level collection storage.
  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  collection_storage->AddCollection(std::move(tab_collection_one), 0);

  EXPECT_TRUE(collection_storage->ContainsCollection(tab_collection_one_ptr));
  EXPECT_FALSE(collection_storage->ContainsCollection(tab_collection_two_ptr));

  // Add four more tabs.
  AddTabs(4);
  ResetTabIDs(0);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);

  collection_storage->AddCollection(std::move(tab_collection_two), 3ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_two_ptr),
            3ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_one_ptr),
            0ul);
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 3}));
}

TEST_F(TabCollectionStorageTest, RemoveMixedTabAndCollectionOperation) {
  auto tab_collection_one = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());
  auto tab_collection_two = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());

  tabs::TabCollection* tab_collection_one_ptr = tab_collection_one.get();
  tabs::TabCollection* tab_collection_two_ptr = tab_collection_two.get();

  // This is the top level collection storage.
  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four more tabs.
  AddTabs(4);
  ResetTabIDs(0);

  collection_storage->AddCollection(std::move(tab_collection_one), 2ul);
  collection_storage->AddCollection(std::move(tab_collection_two), 4ul);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 6ul);

  auto removed_collection =
      collection_storage->RemoveCollection(tab_collection_one_ptr);
  EXPECT_EQ(removed_collection.get(), tab_collection_one_ptr);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_two_ptr),
            3ul);
  EXPECT_FALSE(collection_storage->ContainsCollection(tab_collection_one_ptr));
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 3}));
}

TEST_F(TabCollectionStorageTest, CloseMixedTabAndCollectionOperation) {
  auto tab_collection_one = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());
  tabs::TabCollection* tab_collection_one_ptr = tab_collection_one.get();
  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four tabs
  AddTabs(4);

  collection_storage->AddCollection(std::move(tab_collection_one), 3ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  ResetTabIDs(0);

  collection_storage->CloseCollection(tab_collection_one_ptr);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 4ul);
  EXPECT_FALSE(collection_storage->ContainsCollection(tab_collection_one_ptr));
  EXPECT_EQ(TabIDString(), (std::vector<int>{0, 1, 2, 3}));
}

TEST_F(TabCollectionStorageTest, MoveMixedTabAndCollectionOperation) {
  auto tab_model_one =
      std::make_unique<tabs::TabModel>(nullptr, GetTabStripModel());
  tabs::TabModel* tab_model_one_ptr = tab_model_one.get();

  auto tab_collection_one = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());
  tabs::TabCollection* tab_collection_one_ptr = tab_collection_one.get();

  auto tab_collection_two = std::make_unique<tabs::TabGroupTabCollection>(
      tab_groups::TabGroupId::GenerateNew());
  tabs::TabCollection* tab_collection_two_ptr = tab_collection_two.get();

  tabs::TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  collection_storage->AddTab(std::move(tab_model_one), 0);
  AddTabs(4);
  ResetTabIDs(0);

  collection_storage->AddCollection(std::move(tab_collection_one), 3ul);
  collection_storage->AddCollection(std::move(tab_collection_two), 1ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 7ul);

  // Move `collection_one` to index 1.
  collection_storage->MoveCollection(tab_collection_one_ptr, 1ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_one_ptr),
            1ul);
  // Move `collection_two` to index 6.
  collection_storage->MoveCollection(tab_collection_two_ptr, 6ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_two_ptr),
            6ul);
  // Move `tab_model_one` to index 6.
  collection_storage->MoveTab(tab_model_one_ptr, 6ul);

  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 6ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_one_ptr),
            0ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_two_ptr),
            5ul);
}
