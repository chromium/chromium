// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_collection_observer.h"

#include <memory>
#include <variant>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/mock_tab_group.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
using testing::Return;

// Mock TabCollectionObserver to record calls for verification.
class MockTabCollectionObserver : public TabCollectionObserver {
 public:
  MockTabCollectionObserver() = default;
  ~MockTabCollectionObserver() override = default;

  // TabCollectionObserver:
  MOCK_METHOD(void,
              OnChildrenAdded,
              (const Position&, const TabCollectionNodes&),
              (override));
  MOCK_METHOD(void, OnChildrenRemoved, (const TabCollectionNodes&), (override));
  MOCK_METHOD(void,
              OnChildrenMoved,
              (const Position&, const TabCollectionNodes&),
              (override));
};

class TabCollectionObserverTest : public ::testing::Test {
 public:
  TabCollectionObserverTest() = default;

  void SetUp() override {
    tab_strip_collection_ = std::make_unique<TabStripCollection>();
    tab_strip_collection_->AddObserver(&observer_);
    group_factory_ = std::make_unique<MockTabGroupFactory>(nullptr);
  }

  void TearDown() override {
    tab_strip_collection_->RemoveObserver(&observer_);
  }

  std::unique_ptr<MockTabInterface> CreateMockTab() {
    auto tab = std::make_unique<MockTabInterface>();
    return tab;
  }

  TabStripCollection* GetTabstripCollection() {
    return tab_strip_collection_.get();
  }

  MockTabGroupFactory* GetGroupFactory() { return group_factory_.get(); }

  MockTabCollectionObserver& GetObserver() { return observer_; }

 private:
  std::unique_ptr<TabStripCollection> tab_strip_collection_;
  MockTabCollectionObserver observer_;
  std::unique_ptr<MockTabGroupFactory> group_factory_;
};

TEST_F(TabCollectionObserverTest, OnTabAdded) {
  // Setup with one group tab
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillOnce(testing::Invoke(
          [&](tabs::TabGroupTabCollection* collection,
              const tab_groups::TabGroupId& id,
              const tab_groups::TabGroupVisualData& visual_data) {
            // Return a valid MockTabGroup object.
            return std::make_unique<MockTabGroup>(collection, id, visual_data);
          }));

  tabs::TabStripCollection* collection = GetTabstripCollection();
  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());
  grouped_collection->AddTab(CreateMockTab(), 0);

  collection->InsertTabGroupAt(std::move(grouped_collection), 0);

  MockTabCollectionObserver& observer = GetObserver();

  // Test for unpinned tab.
  {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    TabHandle tab_handle(tab->GetHandle());

    TabCollectionNodes expected_handles;
    expected_handles.push_back(tab_handle);

    const tabs::TabCollectionObserver::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(&TabCollectionObserver::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollectionObserver::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(expected_handles)))
        .Times(1);

    collection->AddTabRecursive(std::move(tab), 0, std::nullopt, false);
  }

  // Test for pinned tab.
  {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    TabHandle tab_handle(tab->GetHandle());

    TabCollectionNodes expected_handles;
    expected_handles.push_back(tab_handle);

    const tabs::TabCollectionObserver::Position expected_position = {
        .parent_handle = collection->pinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(&TabCollectionObserver::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollectionObserver::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(expected_handles)))
        .Times(1);

    collection->AddTabRecursive(std::move(tab), 0, std::nullopt, true);
  }

  // Test for group tab.
  {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    TabHandle tab_handle(tab->GetHandle());

    TabCollectionNodes expected_handles;
    expected_handles.push_back(tab_handle);

    const tabs::TabCollectionObserver::Position expected_position = {
        .parent_handle =
            collection->GetTabGroupCollection(group_id)->GetHandle(),
        .index = 1ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(&TabCollectionObserver::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollectionObserver::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(expected_handles)))
        .Times(1);

    collection->AddTabRecursive(std::move(tab), 3, group_id, false);
  }
}

}  // namespace tabs
