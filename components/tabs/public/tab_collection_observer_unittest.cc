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
              (const TabCollection::Position&, const TabCollectionNodes&),
              (override));
  MOCK_METHOD(void, OnChildrenRemoved, (const TabCollectionNodes&), (override));
  MOCK_METHOD(void,
              OnChildMoved,
              (const TabCollection::Position& to_position,
               const NodeData& node_data),
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
      .WillOnce([&](tabs::TabGroupTabCollection* collection,
                    const tab_groups::TabGroupId& id,
                    const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });

  tabs::TabStripCollection* collection = GetTabstripCollection();
  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());
  grouped_collection->AddTab(CreateMockTab(), 0);

  collection->InsertTabCollectionAt(std::move(grouped_collection), 0, false,
                                    std::nullopt);

  MockTabCollectionObserver& observer = GetObserver();

  // Test for unpinned tab.
  {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    TabHandle tab_handle(tab->GetHandle());

    TabCollectionNodes expected_handles;
    expected_handles.push_back(tab_handle);

    const TabCollection::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
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

    const TabCollection::Position expected_position = {
        .parent_handle = collection->pinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
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

    const TabCollection::Position expected_position = {
        .parent_handle =
            collection->GetTabGroupCollection(group_id)->GetHandle(),
        .index = 1ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(expected_handles)))
        .Times(1);

    collection->AddTabRecursive(std::move(tab), 3, group_id, false);
  }
}

TEST_F(TabCollectionObserverTest, OnTabCollectionAttached) {
  MockTabCollectionObserver& observer = GetObserver();

  // Setup with one group
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillOnce([&](tabs::TabGroupTabCollection* collection,
                    const tab_groups::TabGroupId& id,
                    const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });

  tabs::TabStripCollection* collection = GetTabstripCollection();
  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());
  grouped_collection->AddTab(CreateMockTab(), 0);

  tabs::TabCollectionHandle group_handle = grouped_collection->GetHandle();
  TabCollectionNodes expected_handles;
  expected_handles.push_back(group_handle);

  const TabCollection::Position expected_position = {
      .parent_handle = collection->unpinned_collection()->GetHandle(),
      .index = 0ul,
  };

  EXPECT_CALL(
      observer,
      OnChildrenAdded(
          testing::AllOf(
              testing::Field(&TabCollection::Position::parent_handle,
                             testing::Eq(expected_position.parent_handle)),
              testing::Field(&TabCollection::Position::index,
                             testing::Eq(expected_position.index))),
          testing::Eq(expected_handles)))
      .Times(1);

  collection->InsertTabCollectionAt(std::move(grouped_collection), 0, false,
                                    std::nullopt);

  // Add a split to the group
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  std::unique_ptr<tabs::SplitTabCollection> split_collection_unique =
      std::make_unique<tabs::SplitTabCollection>(
          split_id, split_tabs::SplitTabVisualData(
                        split_tabs::SplitTabLayout::kVertical, 0.5));

  split_collection_unique->AddTab(CreateMockTab(), 0);
  split_collection_unique->AddTab(CreateMockTab(), 0);

  tabs::TabCollectionHandle split_handle = split_collection_unique->GetHandle();

  TabCollectionNodes expected_handles_split;
  expected_handles_split.push_back(split_handle);

  const TabCollection::Position expected_position_split = {
      .parent_handle = group_handle,
      .index = 1ul,
  };

  EXPECT_CALL(
      observer,
      OnChildrenAdded(
          testing::AllOf(
              testing::Field(
                  &TabCollection::Position::parent_handle,
                  testing::Eq(expected_position_split.parent_handle)),
              testing::Field(&TabCollection::Position::index,
                             testing::Eq(expected_position_split.index))),
          testing::Eq(expected_handles_split)))
      .Times(1);

  collection->InsertTabCollectionAt(std::move(split_collection_unique), 1,
                                    false, group_id);
}

TEST_F(TabCollectionObserverTest, OnTabRemoved) {
  // Setup with one group tab
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillOnce([&](tabs::TabGroupTabCollection* collection,
                    const tab_groups::TabGroupId& id,
                    const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });

  std::unique_ptr<tabs::MockTabInterface> pinned_tab = CreateMockTab();
  std::unique_ptr<tabs::MockTabInterface> unpinned_tab = CreateMockTab();
  std::unique_ptr<tabs::MockTabInterface> group_tab_0 = CreateMockTab();
  std::unique_ptr<tabs::MockTabInterface> group_tab_1 = CreateMockTab();
  tabs::MockTabInterface* pinned_tab_ptr = pinned_tab.get();
  tabs::MockTabInterface* unpinned_tab_ptr = unpinned_tab.get();
  tabs::MockTabInterface* group_tab_0_ptr = group_tab_0.get();
  tabs::MockTabInterface* group_tab_1_ptr = group_tab_1.get();
  TabHandle pinned_tab_handle = pinned_tab->GetHandle();
  TabHandle unpinned_tab_handle = unpinned_tab->GetHandle();
  TabHandle group_tab_1_handle = group_tab_1->GetHandle();

  tabs::TabStripCollection* collection = GetTabstripCollection();
  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());
  tabs::TabGroupTabCollection* group_collection_ptr = group_collection.get();
  TabCollectionHandle group_collection_handle = group_collection->GetHandle();
  group_collection->AddTab(std::move(group_tab_0), 0);
  collection->AddTabRecursive(std::move(pinned_tab), 0, std::nullopt, true);
  collection->AddTabRecursive(std::move(unpinned_tab), 1, std::nullopt, false);
  collection->InsertTabCollectionAt(std::move(group_collection), 2, false,
                                    std::nullopt);
  collection->AddTabRecursive(std::move(group_tab_1), 3, group_id, false);

  MockTabCollectionObserver& observer = GetObserver();

  // Test for non-final group tab.
  {
    EXPECT_CALL(*group_tab_1_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(group_collection_ptr));
    EXPECT_CALL(*group_tab_1_ptr, GetGroup()).WillRepeatedly(Return(group_id));
    EXPECT_CALL(observer, OnChildrenRemoved(testing::Eq(
                              tabs::TabCollectionNodes{group_tab_1_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(3);
  }

  // Test for final group tab.
  {
    EXPECT_CALL(*group_tab_0_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(group_collection_ptr));
    EXPECT_CALL(*group_tab_0_ptr, GetGroup()).WillRepeatedly(Return(group_id));
    EXPECT_CALL(observer,
                OnChildrenRemoved(testing::Eq(
                    tabs::TabCollectionNodes{group_collection_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(2);
  }

  // Test for unpinned tab.
  {
    EXPECT_CALL(*unpinned_tab_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(collection->unpinned_collection()));
    EXPECT_CALL(observer, OnChildrenRemoved(testing::Eq(
                              tabs::TabCollectionNodes{unpinned_tab_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(1);
  }

  // Test for pinned tab.
  {
    EXPECT_CALL(*pinned_tab_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(collection->pinned_collection()));
    EXPECT_CALL(observer, OnChildrenRemoved(testing::Eq(
                              tabs::TabCollectionNodes{pinned_tab_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(0);
  }
}

TEST_F(TabCollectionObserverTest, OnCollectionRemoved) {
  MockTabCollectionObserver& observer = GetObserver();
  tabs::TabStripCollection* collection = GetTabstripCollection();

  // Set up a group.
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillOnce([&](tabs::TabGroupTabCollection* collection,
                    const tab_groups::TabGroupId& id,
                    const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });
  std::unique_ptr<tabs::TabGroupTabCollection> group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());
  tabs::TabGroupTabCollection* group_collection_ptr = group_collection.get();
  tabs::TabCollectionHandle group_handle = group_collection->GetHandle();
  group_collection->AddTab(CreateMockTab(), 0);
  collection->InsertTabCollectionAt(std::move(group_collection), 0, false,
                                    std::nullopt);

  // Add a split to the group.
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  std::unique_ptr<tabs::SplitTabCollection> split_collection =
      std::make_unique<tabs::SplitTabCollection>(
          split_id, split_tabs::SplitTabVisualData(
                        split_tabs::SplitTabLayout::kVertical, 0.5));
  tabs::SplitTabCollection* split_collection_ptr = split_collection.get();
  tabs::TabCollectionHandle split_handle = split_collection->GetHandle();
  split_collection->AddTab(CreateMockTab(), 0);
  split_collection->AddTab(CreateMockTab(), 0);
  collection->InsertTabCollectionAt(std::move(split_collection), 1, false,
                                    group_id);

  // Remove the split.
  {
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(testing::Eq(tabs::TabCollectionNodes{split_handle})))
        .Times(1);

    collection->RemoveTabCollection(split_collection_ptr);
  }

  // Remove the group.
  {
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(testing::Eq(tabs::TabCollectionNodes{group_handle})))
        .Times(1);

    collection->RemoveTabCollection(group_collection_ptr);
  }
}

}  // namespace tabs
