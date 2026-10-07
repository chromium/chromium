// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_collection_storage.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

class TabCollectionStorageTest : public ::testing::Test {
 public:
  TabCollectionStorageTest() {
    pinned_collection_ = std::make_unique<PinnedTabCollection>();
  }
  TabCollectionStorageTest(const TabCollectionStorageTest&) = delete;
  TabCollectionStorageTest& operator=(const TabCollectionStorageTest&) = delete;
  ~TabCollectionStorageTest() override { pinned_collection_.reset(); }

  TabCollectionStorage* GetTabCollectionStorage() {
    return pinned_collection_->GetTabCollectionStorageForTesting();
  }

  std::unique_ptr<MockTabInterface> CreateMockTab() {
    return std::make_unique<MockTabInterface>();
  }

  void AddTabs(int num) {
    for (int i = 0; i < num; i++) {
      std::unique_ptr<MockTabInterface> tab_model = CreateMockTab();
      TabInterface* tab_model_ptr = tab_model.get();

      TabInterface* inserted_tab_model_ptr = GetTabCollectionStorage()->AddTab(
          std::move(tab_model), GetTabCollectionStorage()->GetChildrenCount());
      EXPECT_EQ(tab_model_ptr, inserted_tab_model_ptr);
      EXPECT_EQ(GetTabCollectionStorage()->GetIndexOfTab(tab_model_ptr),
                GetTabCollectionStorage()->GetChildrenCount() - 1);
    }
  }

  void SetTabID(TabInterface* tab_model, int id) {
    std::string identifier =
        "T" + base::NumberToString(reinterpret_cast<uintptr_t>(tab_model));
    storage_children_to_id_map_[identifier] = "T" + base::NumberToString(id);
  }

  void SetCollectionID(TabCollection* collection, int id) {
    std::string identifier =
        "C" + base::NumberToString(reinterpret_cast<uintptr_t>(collection));
    storage_children_to_id_map_[identifier] = "C" + base::NumberToString(id);
  }

  void ResetStorageChildrenIDs(int start) {
    int tab_i = 0;
    int collection_i = 0;
    const auto& children = GetTabCollectionStorage()->GetChildren();
    for (const auto& child : children) {
      if (std::holds_alternative<ScopedTab>(child)) {
        SetTabID(std::get<ScopedTab>(child).get(), start + tab_i);
        tab_i += 1;
      } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(
                     child)) {
        SetCollectionID(std::get<std::unique_ptr<TabCollection>>(child).get(),
                        start + collection_i);
        collection_i += 1;
      }
    }
  }

  std::vector<std::string> StorageCollectionChildrenString() {
    std::vector<std::string> ids;
    const auto& children = GetTabCollectionStorage()->GetChildren();
    for (const auto& child : children) {
      std::string identifier;
      if (std::holds_alternative<ScopedTab>(child)) {
        TabInterface* tab = std::get<ScopedTab>(child).get();
        identifier =
            "T" + base::NumberToString(reinterpret_cast<uintptr_t>(tab));
      } else if (std::holds_alternative<std::unique_ptr<TabCollection>>(
                     child)) {
        TabCollection* collection =
            std::get<std::unique_ptr<TabCollection>>(child).get();
        identifier =
            "C" + base::NumberToString(reinterpret_cast<uintptr_t>(collection));
      }
      ids.push_back(storage_children_to_id_map_[identifier]);
    }
    return ids;
  }

 private:
  std::unique_ptr<PinnedTabCollection> pinned_collection_;
  std::map<std::string, std::string> storage_children_to_id_map_;
};

TEST_F(TabCollectionStorageTest, AddTabOperation) {
  auto tab_model_one = CreateMockTab();
  auto tab_model_two = CreateMockTab();

  TabInterface* tab_model_one_ptr = tab_model_one.get();
  TabInterface* tab_model_two_ptr = tab_model_two.get();

  TabCollectionStorage* collection_storage = GetTabCollectionStorage();
  collection_storage->AddTab(std::move(tab_model_one), 0);

  EXPECT_TRUE(collection_storage->ContainsTab(tab_model_one_ptr));
  EXPECT_FALSE(collection_storage->ContainsTab(tab_model_two.get()));

  // Add four more tabs.
  AddTabs(4);
  ResetStorageChildrenIDs(0);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);

  // Annotate `tab_model_two_ptr` with an id of 5.
  SetTabID(tab_model_two_ptr, 5);
  collection_storage->AddTab(std::move(tab_model_two), 3ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_two_ptr), 3ul);
  EXPECT_EQ(collection_storage->GetTabAtIndex(3ul), tab_model_two_ptr);
  EXPECT_EQ(StorageCollectionChildrenString(),
            (std::vector<std::string>{"T0", "T1", "T2", "T5", "T3", "T4"}));

  // TODO(b/332586827):Add death testing for out of bounds index.
}

TEST_F(TabCollectionStorageTest, RemoveTabOperation) {
  auto tab_model_one = CreateMockTab();
  TabInterface* tab_model_one_ptr = tab_model_one.get();

  TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  collection_storage->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  ResetStorageChildrenIDs(0);

  auto removed_tab = collection_storage->RemoveTab(tab_model_one_ptr);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 4ul);
  EXPECT_EQ(removed_tab.get(), tab_model_one_ptr);
  // `tab_model_one_ptr` was removed from index 3.
  EXPECT_EQ(StorageCollectionChildrenString(),
            (std::vector<std::string>{"T0", "T1", "T2", "T4"}));
}

TEST_F(TabCollectionStorageTest, MoveTabOperation) {
  auto tab_model_one = CreateMockTab();
  TabInterface* tab_model_one_ptr = tab_model_one.get();

  TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four tabs
  AddTabs(4);

  // Add `tab_model_one` to index 3.
  collection_storage->AddTab(std::move(tab_model_one), 3ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 3ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  ResetStorageChildrenIDs(0);

  collection_storage->MoveTab(tab_model_one_ptr, 1ul);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 1ul);
  EXPECT_EQ(StorageCollectionChildrenString(),
            (std::vector<std::string>{"T0", "T3", "T1", "T2", "T4"}));

  collection_storage->MoveTab(tab_model_one_ptr, 4ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);
  EXPECT_EQ(collection_storage->GetIndexOfTab(tab_model_one_ptr), 4ul);
  EXPECT_EQ(StorageCollectionChildrenString(),
            (std::vector<std::string>{"T0", "T1", "T2", "T4", "T3"}));
}

// TODO(b/332586827): Re-enable death testing.
TEST_F(TabCollectionStorageTest, DISABLED_InvalidArgumentsTabOperations) {
  auto tab_model_one = CreateMockTab();
  TabCollectionStorage* collection_storage = GetTabCollectionStorage();
  std::unique_ptr<MockTabInterface> empty_ptr;

  EXPECT_DEATH_IF_SUPPORTED(collection_storage->AddTab(CreateMockTab(), 10ul),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(
      collection_storage->AddTab(std::move(empty_ptr), 1ul), "");

  EXPECT_DEATH_IF_SUPPORTED(
      { ScopedTab tab = collection_storage->RemoveTab(tab_model_one.get()); },
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      { ScopedTab tab = collection_storage->RemoveTab(nullptr); }, "");

  EXPECT_DEATH_IF_SUPPORTED(
      collection_storage->MoveTab(tab_model_one.get(), 0ul), "");
  collection_storage->AddTab(std::move(tab_model_one), 0ul);
  EXPECT_DEATH_IF_SUPPORTED(
      collection_storage->MoveTab(tab_model_one.get(), 10ul), "");
  EXPECT_DEATH_IF_SUPPORTED(collection_storage->MoveTab(nullptr, 10ul), "");
}

TEST_F(TabCollectionStorageTest, AddMixedTabAndCollectionOperation) {
  auto tab_collection_one = std::make_unique<UnpinnedTabCollection>();
  auto tab_collection_two = std::make_unique<UnpinnedTabCollection>();

  TabCollection* tab_collection_one_ptr = tab_collection_one.get();
  TabCollection* tab_collection_two_ptr = tab_collection_two.get();

  // This is the top level collection storage.
  TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  collection_storage->AddCollection(std::move(tab_collection_one), 0);

  EXPECT_TRUE(collection_storage->ContainsCollection(tab_collection_one_ptr));
  EXPECT_FALSE(collection_storage->ContainsCollection(tab_collection_two_ptr));

  // Add four more tabs.
  AddTabs(4);
  ResetStorageChildrenIDs(0);

  // Set collection ids of both the sub collections.
  SetCollectionID(tab_collection_one_ptr, 0);
  SetCollectionID(tab_collection_two_ptr, 1);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 5ul);

  collection_storage->AddCollection(std::move(tab_collection_two), 3ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_two_ptr),
            3ul);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_one_ptr),
            0ul);
  EXPECT_EQ(StorageCollectionChildrenString(),
            (std::vector<std::string>{"C0", "T0", "T1", "C1", "T2", "T3"}));
}

TEST_F(TabCollectionStorageTest, RemoveMixedTabAndCollectionOperation) {
  auto tab_collection_one = std::make_unique<UnpinnedTabCollection>();
  auto tab_collection_two = std::make_unique<UnpinnedTabCollection>();

  TabCollection* tab_collection_one_ptr = tab_collection_one.get();
  TabCollection* tab_collection_two_ptr = tab_collection_two.get();

  // This is the top level collection storage.
  TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  // Add four more tabs.
  AddTabs(4);
  ResetStorageChildrenIDs(0);

  SetCollectionID(tab_collection_one_ptr, 0);
  SetCollectionID(tab_collection_two_ptr, 1);

  collection_storage->AddCollection(std::move(tab_collection_one), 2ul);
  collection_storage->AddCollection(std::move(tab_collection_two), 4ul);

  EXPECT_EQ(collection_storage->GetChildrenCount(), 6ul);

  auto removed_collection =
      collection_storage->RemoveCollection(tab_collection_one_ptr);
  EXPECT_EQ(removed_collection.get(), tab_collection_one_ptr);
  EXPECT_EQ(collection_storage->GetIndexOfCollection(tab_collection_two_ptr),
            3ul);
  EXPECT_FALSE(collection_storage->ContainsCollection(tab_collection_one_ptr));
  EXPECT_EQ(StorageCollectionChildrenString(),
            (std::vector<std::string>{"T0", "T1", "T2", "C1", "T3"}));
}

TEST_F(TabCollectionStorageTest, MoveMixedTabAndCollectionOperation) {
  auto tab_model_one = CreateMockTab();
  TabInterface* tab_model_one_ptr = tab_model_one.get();

  auto tab_collection_one = std::make_unique<UnpinnedTabCollection>();
  TabCollection* tab_collection_one_ptr = tab_collection_one.get();

  auto tab_collection_two = std::make_unique<UnpinnedTabCollection>();
  TabCollection* tab_collection_two_ptr = tab_collection_two.get();

  TabCollectionStorage* collection_storage = GetTabCollectionStorage();

  collection_storage->AddTab(std::move(tab_model_one), 0);
  AddTabs(4);
  ResetStorageChildrenIDs(0);

  collection_storage->AddCollection(std::move(tab_collection_one), 3ul);
  collection_storage->AddCollection(std::move(tab_collection_two), 1ul);
  EXPECT_EQ(collection_storage->GetChildrenCount(), 7ul);

  // Set collection ids of both the sub collections.
  SetCollectionID(tab_collection_one_ptr, 0);
  SetCollectionID(tab_collection_two_ptr, 1);

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
  EXPECT_EQ(
      StorageCollectionChildrenString(),
      (std::vector<std::string>{"C0", "T1", "T2", "T3", "T4", "C1", "T0"}));
}

namespace {

class NonOwningTestTab : public MockTabInterface {
 public:
  NonOwningTestTab() {
    ON_CALL(*this, OnReparented)
        .WillByDefault(
            [this](TabCollection* parent, base::PassKey<TabCollection>) {
              parent_collection_ = parent;
            });
    ON_CALL(*this,
            GetParentCollection(testing::An<base::PassKey<TabCollection>>()))
        .WillByDefault([this] { return parent_collection_; });
    ON_CALL(*this, GetParentCollection()).WillByDefault([this] {
      return parent_collection_;
    });
  }
  ~NonOwningTestTab() override = default;

  void DeleteSelf() override {
    // Mimic TabAndroid where lifetime is managed externally (e.g. by Java).
  }

 private:
  raw_ptr<TabCollection> parent_collection_ = nullptr;
};

}  // namespace

TEST_F(TabCollectionStorageTest,
       DestructionClearsParentPointerOnSurvivingTabs) {
  NonOwningTestTab tab;
  EXPECT_EQ(tab.GetParentCollection(), nullptr);

  auto pinned_collection = std::make_unique<PinnedTabCollection>();
  TabCollection* pinned_collection_ptr = pinned_collection.get();
  pinned_collection->AddTab(ScopedTab(&tab), 0);
  EXPECT_EQ(tab.GetParentCollection(), pinned_collection_ptr);

  // Destroy the collection holding the tab.
  pinned_collection.reset();

  // TabInterface's parent pointer should be cleanly cleared to nullptr so that
  // surviving tabs (e.g. TabAndroid on Android) do not hold dangling pointers.
  EXPECT_EQ(tab.GetParentCollection(), nullptr);
}

}  // namespace tabs
