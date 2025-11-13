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
              (const TabCollection::Position&, const TabCollectionNodes&, bool),
              (override));
  MOCK_METHOD(void,
              OnChildrenRemoved,
              (const TabCollection::Position&, const TabCollectionNodes&),
              (override));
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
            testing::Eq(expected_handles), testing::Eq(false)))
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
            testing::Eq(expected_handles), testing::Eq(false)))
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
            testing::Eq(expected_handles), testing::Eq(false)))
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
          testing::Eq(expected_handles), testing::Eq(true)))
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
          testing::Eq(expected_handles_split), testing::Eq(true)))
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
    const TabCollection::Position expected_position = {
        .parent_handle = group_collection_handle,
        .index = 1ul,
    };
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{group_tab_1_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(3);
  }

  // Test for final group tab.
  {
    EXPECT_CALL(*group_tab_0_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(group_collection_ptr));
    EXPECT_CALL(*group_tab_0_ptr, GetGroup()).WillRepeatedly(Return(group_id));
    const TabCollection::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 1ul,
    };
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{group_collection_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(2);
  }

  // Test for unpinned tab.
  {
    EXPECT_CALL(*unpinned_tab_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(collection->unpinned_collection()));
    const TabCollection::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{unpinned_tab_handle})))
        .Times(1);

    collection->RemoveTabAtIndexRecursive(1);
  }

  // Test for pinned tab.
  {
    EXPECT_CALL(*pinned_tab_ptr, GetParentCollection(testing::_))
        .WillRepeatedly(Return(collection->pinned_collection()));
    const TabCollection::Position expected_position = {
        .parent_handle = collection->pinned_collection()->GetHandle(),
        .index = 0ul,
    };
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{pinned_tab_handle})))
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
    const TabCollection::Position expected_position = {
        .parent_handle = group_collection_ptr->GetHandle(),
        .index = 1ul,
    };
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{split_handle})))
        .Times(1);

    collection->RemoveTabCollection(split_collection_ptr);
  }

  // Remove the group.
  {
    const TabCollection::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };
    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{group_handle})))
        .Times(1);

    collection->RemoveTabCollection(group_collection_ptr);
  }
}

TEST_F(TabCollectionObserverTest, OnSplitCreated) {
  tabs::TabStripCollection* collection = GetTabstripCollection();

  // Create 5 tabs
  for (int i = 0; i < 5; i++) {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    EXPECT_CALL(*tab, GetParentCollection(testing::_))
        .WillRepeatedly(testing::Return(collection->unpinned_collection()));
    collection->AddTabRecursive(std::move(tab), 0, std::nullopt, false);
  }

  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();

  std::vector<TabInterface*> tabs_to_split = {
      collection->GetTabAtIndexRecursive(2),
      collection->GetTabAtIndexRecursive(3)};
  MockTabCollectionObserver& observer = GetObserver();

  TabHandle handle_0 = tabs_to_split[0]->GetHandle();
  TabHandle handle_1 = tabs_to_split[1]->GetHandle();

  // First Notification: The Split Collection is added to the parent
  const TabCollection::Position expected_split_position = {
      .parent_handle = collection->unpinned_collection()->GetHandle(),
      .index = 2ul,
  };

  tabs::TabCollectionHandle new_split_handle;

  EXPECT_CALL(
      observer,
      OnChildrenAdded(
          testing::AllOf(
              testing::Field(
                  &TabCollection::Position::parent_handle,
                  testing::Eq(expected_split_position.parent_handle)),
              testing::Field(&TabCollection::Position::index,
                             testing::Eq(expected_split_position.index))),
          testing::SizeIs(1), testing::Eq(false)))
      .WillOnce([&](const TabCollection::Position& position,
                    const TabCollectionNodes& handles,
                    bool detached_collection) {
        // Save the handle of the newly added split collection for the next
        // expectation.
        new_split_handle = std::get<tabs::TabCollection::Handle>(handles[0]);
        EXPECT_EQ(new_split_handle.Get()->type(), TabCollection::Type::SPLIT);

        auto src_position_matcher = testing::AllOf(
            testing::Field(
                &TabCollection::Position::parent_handle,
                testing::Eq(collection->unpinned_collection()->GetHandle())),
            testing::Field(&TabCollection::Position::index, testing::Eq(3ul)));

        EXPECT_CALL(
            observer,
            OnChildMoved(
                testing::AllOf(
                    testing::Field(&TabCollection::Position::parent_handle,
                                   testing::Eq(new_split_handle)),
                    testing::Field(&TabCollection::Position::index,
                                   testing::Eq(0))),
                testing::AllOf(
                    testing::Field(&TabCollectionObserver::NodeData::position,
                                   src_position_matcher),
                    testing::Field(&TabCollectionObserver::NodeData::handle,
                                   testing::VariantWith<TabHandle>(
                                       testing::Eq(handle_0))))))
            .Times(1);

        EXPECT_CALL(
            observer,
            OnChildMoved(
                testing::AllOf(
                    testing::Field(&TabCollection::Position::parent_handle,
                                   testing::Eq(new_split_handle)),
                    testing::Field(&TabCollection::Position::index,
                                   testing::Eq(1))),
                testing::AllOf(
                    testing::Field(&TabCollectionObserver::NodeData::position,
                                   src_position_matcher),
                    testing::Field(&TabCollectionObserver::NodeData::handle,
                                   testing::VariantWith<TabHandle>(
                                       testing::Eq(handle_1))))))
            .Times(1);
      });

  collection->CreateSplit(split_id, tabs_to_split,
                          split_tabs::SplitTabVisualData(
                              split_tabs::SplitTabLayout::kVertical, 0.5));
}

TEST_F(TabCollectionObserverTest, OnUnsplit) {
  MockTabCollectionObserver& observer = GetObserver();
  tabs::TabStripCollection* collection = GetTabstripCollection();

  // Set up a split.
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  std::unique_ptr<tabs::SplitTabCollection> split_collection =
      std::make_unique<tabs::SplitTabCollection>(
          split_id, split_tabs::SplitTabVisualData(
                        split_tabs::SplitTabLayout::kVertical, 0.5));
  tabs::TabCollectionHandle split_handle = split_collection->GetHandle();

  for (int i = 0; i < 2; i++) {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    EXPECT_CALL(*tab, GetParentCollection(testing::_))
        .WillRepeatedly(testing::Return(split_collection.get()));
    split_collection->AddTab(std::move(tab), i);
  }

  collection->InsertTabCollectionAt(std::move(split_collection), 0, false,
                                    std::nullopt);

  // Unsplit.
  {
    EXPECT_CALL(
        observer,
        OnChildMoved(
            testing::AllOf(
                testing::Field(
                    &TabCollection::Position::parent_handle,
                    testing::Eq(
                        collection->unpinned_collection()->GetHandle())),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(0))),
            testing::AllOf(
                testing::Field(
                    &TabCollectionObserver::NodeData::position,
                    testing::AllOf(
                        testing::Field(&TabCollection::Position::parent_handle,
                                       testing::Eq(split_handle)),
                        testing::Field(&TabCollection::Position::index,
                                       testing::Eq(0ul)))),
                testing::Field(
                    &TabCollectionObserver::NodeData::handle,
                    testing::VariantWith<TabHandle>(testing::Eq(
                        collection->GetTabAtIndexRecursive(0)->GetHandle()))))))
        .Times(1);

    EXPECT_CALL(
        observer,
        OnChildMoved(
            testing::AllOf(
                testing::Field(
                    &TabCollection::Position::parent_handle,
                    testing::Eq(
                        collection->unpinned_collection()->GetHandle())),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(1))),
            testing::AllOf(
                testing::Field(
                    &TabCollectionObserver::NodeData::position,
                    testing::AllOf(
                        testing::Field(&TabCollection::Position::parent_handle,
                                       testing::Eq(split_handle)),
                        testing::Field(&TabCollection::Position::index,
                                       testing::Eq(0ul)))),
                testing::Field(
                    &TabCollectionObserver::NodeData::handle,
                    testing::VariantWith<TabHandle>(testing::Eq(
                        collection->GetTabAtIndexRecursive(1)->GetHandle()))))))
        .Times(1);

    const TabCollection::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 2ul,  // Split collection will now be after tabs.
    };

    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{split_handle})))
        .Times(1);
    collection->Unsplit(split_id);
  }
}

TEST_F(TabCollectionObserverTest, MoveTab) {
  tabs::TabStripCollection* collection = GetTabstripCollection();

  // Create 5 tabs
  for (int i = 0; i < 5; i++) {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    EXPECT_CALL(*tab, GetParentCollection(testing::_))
        .WillRepeatedly(testing::Return(collection->unpinned_collection()));
    collection->AddTabRecursive(std::move(tab), 0, std::nullopt, false);
  }

  // Add a tab group with one tab
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillRepeatedly([&](tabs::TabGroupTabCollection* collection,
                          const tab_groups::TabGroupId& id,
                          const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });

  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());

  std::unique_ptr<MockTabInterface> grouped_tab = CreateMockTab();
  MockTabInterface* mock_tab_to_move = grouped_tab.get();
  EXPECT_CALL(*grouped_tab, GetParentCollection(testing::_))
      .WillRepeatedly(testing::Return(grouped_collection.get()));

  grouped_collection->AddTab(std::move(grouped_tab), 0);

  tabs::TabCollectionHandle old_group_handle = grouped_collection->GetHandle();

  collection->InsertTabCollectionAt(std::move(grouped_collection), 0, false,
                                    std::nullopt);

  // Create a detached group for the move.
  tab_groups::TabGroupId new_group_id = tab_groups::TabGroupId::GenerateNew();
  std::unique_ptr<tabs::TabGroupTabCollection> new_grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), new_group_id, tab_groups::TabGroupVisualData());

  tabs::TabCollectionHandle new_group_handle =
      new_grouped_collection->GetHandle();
  collection->CreateTabGroup(std::move(new_grouped_collection));

  tabs::TabGroupTabCollection* old_group_ptr =
      collection->GetTabGroupCollection(group_id);
  EXPECT_CALL(*mock_tab_to_move, GetParentCollection(testing::_))
      .WillRepeatedly([&old_group_ptr, &collection, &new_group_id,
                       &mock_tab_to_move]() -> tabs::TabCollection* {
        if (old_group_ptr->GetIndexOfTab(mock_tab_to_move).has_value()) {
          return old_group_ptr;
        } else {
          return collection->GetTabGroupCollection(new_group_id);
        }
      });

  MockTabCollectionObserver& observer = GetObserver();

  // Move tab from previous tabgroup to new tabgroup
  {
    // Get the tab pointer and handle (index 0 is the tab in the group)
    tabs::TabInterface* tab_to_move = collection->GetTabAtIndexRecursive(0);
    TabHandle tab_handle = tab_to_move->GetHandle();

    const TabCollection::Position expected_new_group_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 6ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenAdded(
            testing::AllOf(
                testing::Field(
                    &TabCollection::Position::parent_handle,
                    testing::Eq(expected_new_group_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_new_group_position.index))),
            testing::Eq(tabs::TabCollectionNodes{new_group_handle}),
            testing::Eq(false)));

    // 2. The tab is moved from old group to new group
    const TabCollection::Position expected_to_position = {
        .parent_handle = new_group_handle,
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildMoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(testing::ByRef(new_group_handle))),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_to_position.index))),
            testing::AllOf(
                testing::Field(
                    &TabCollectionObserver::NodeData::position,
                    testing::AllOf(
                        testing::Field(&TabCollection::Position::parent_handle,
                                       testing::Eq(old_group_handle)),
                        testing::Field(&TabCollection::Position::index,
                                       testing::Eq(0ul)))),
                testing::Field(
                    &TabCollectionObserver::NodeData::handle,
                    testing::VariantWith<TabHandle>(testing::Eq(tab_handle))))))
        .Times(1);

    const TabCollection::Position expected_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildrenRemoved(
            testing::AllOf(
                testing::Field(&TabCollection::Position::parent_handle,
                               testing::Eq(expected_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_position.index))),
            testing::Eq(tabs::TabCollectionNodes{old_group_handle})))
        .Times(1);
    collection->MoveTabRecursive(0, 5, new_group_id, false);
  }
}

TEST_F(TabCollectionObserverTest, MoveOnlyTabInGroup) {
  tabs::TabStripCollection* collection = GetTabstripCollection();

  // Create 5 tabs
  for (int i = 0; i < 5; i++) {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    EXPECT_CALL(*tab, GetParentCollection(testing::_))
        .WillRepeatedly(testing::Return(collection->unpinned_collection()));
    collection->AddTabRecursive(std::move(tab), 0, std::nullopt, false);
  }

  // Add a tab group with one tab
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillRepeatedly([&](tabs::TabGroupTabCollection* collection,
                          const tab_groups::TabGroupId& id,
                          const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });

  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());

  std::unique_ptr<MockTabInterface> grouped_tab = CreateMockTab();
  EXPECT_CALL(*grouped_tab, GetParentCollection(testing::_))
      .WillRepeatedly(testing::Return(grouped_collection.get()));
  EXPECT_CALL(*grouped_tab, GetGroup())
      .WillRepeatedly(testing::Return(group_id));
  grouped_collection->AddTab(std::move(grouped_tab), 0);

  tabs::TabCollectionHandle old_group_handle = grouped_collection->GetHandle();

  collection->InsertTabCollectionAt(std::move(grouped_collection), 0, false,
                                    std::nullopt);

  MockTabCollectionObserver& observer = GetObserver();

  // Move tab group from previous tabgroup to new tabgroup
  {
    EXPECT_CALL(
        observer,
        OnChildMoved(
            testing::AllOf(
                testing::Field(
                    &TabCollection::Position::parent_handle,
                    testing::Eq(
                        collection->unpinned_collection()->GetHandle())),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(5u))),
            testing::AllOf(
                testing::Field(
                    &TabCollectionObserver::NodeData::position,
                    testing::AllOf(
                        testing::Field(
                            &TabCollection::Position::parent_handle,
                            testing::Eq(collection->unpinned_collection()
                                            ->GetHandle())),
                        testing::Field(&TabCollection::Position::index,
                                       testing::Eq(0ul)))),
                testing::Field(&TabCollectionObserver::NodeData::handle,
                               testing::VariantWith<tabs::TabCollectionHandle>(
                                   testing::Eq(old_group_handle))))))
        .Times(1);

    collection->MoveTabRecursive(0, 5, group_id, false);
  }
}

TEST_F(TabCollectionObserverTest, MoveTabAndGroup) {
  tabs::TabStripCollection* collection = GetTabstripCollection();

  // Create 5 tabs
  for (int i = 0; i < 5; i++) {
    std::unique_ptr<MockTabInterface> tab = CreateMockTab();
    EXPECT_CALL(*tab, GetParentCollection(testing::_))
        .WillRepeatedly(testing::Return(collection->unpinned_collection()));
    collection->AddTabRecursive(std::move(tab), 0, std::nullopt, false);
  }

  // Add a tab group with one tab
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();

  EXPECT_CALL(*GetGroupFactory(), Create)
      .WillRepeatedly([&](tabs::TabGroupTabCollection* collection,
                          const tab_groups::TabGroupId& id,
                          const tab_groups::TabGroupVisualData& visual_data) {
        // Return a valid MockTabGroup object.
        return std::make_unique<MockTabGroup>(collection, id, visual_data);
      });

  std::unique_ptr<tabs::TabGroupTabCollection> grouped_collection =
      std::make_unique<tabs::TabGroupTabCollection>(
          *GetGroupFactory(), group_id, tab_groups::TabGroupVisualData());

  std::unique_ptr<MockTabInterface> grouped_tab = CreateMockTab();
  EXPECT_CALL(*grouped_tab, GetParentCollection(testing::_))
      .WillRepeatedly(testing::Return(grouped_collection.get()));
  EXPECT_CALL(*grouped_tab, GetGroup())
      .WillRepeatedly(testing::Return(group_id));
  grouped_collection->AddTab(std::move(grouped_tab), 0);

  tabs::TabCollectionHandle old_group_handle = grouped_collection->GetHandle();

  collection->InsertTabCollectionAt(std::move(grouped_collection), 0, false,
                                    std::nullopt);

  MockTabCollectionObserver& observer = GetObserver();

  {
    TabHandle tab_handle = collection->GetTabAtIndexRecursive(1)->GetHandle();

    // --- EXPECTATION 1: Group Move ---
    const TabCollection::Position expected_group_to_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 2ul,
    };
    const TabCollection::Position expected_group_from_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildMoved(
            testing::AllOf(
                testing::Field(
                    &TabCollection::Position::parent_handle,
                    testing::Eq(expected_group_to_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_group_to_position.index))),
            testing::AllOf(
                testing::Field(
                    &TabCollectionObserver::NodeData::position,
                    testing::AllOf(
                        testing::Field(&TabCollection::Position::parent_handle,
                                       testing::Eq(expected_group_from_position
                                                       .parent_handle)),
                        testing::Field(
                            &TabCollection::Position::index,
                            testing::Eq(expected_group_from_position.index)))),
                testing::Field(&TabCollectionObserver::NodeData::handle,
                               testing::VariantWith<tabs::TabCollectionHandle>(
                                   testing::Eq(old_group_handle))))))
        .Times(1)
        .RetiresOnSaturation();

    // --- EXPECTATION 2: Tab Move ---
    const TabCollection::Position expected_tab_to_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 2ul,
    };
    const TabCollection::Position expected_tab_from_position = {
        .parent_handle = collection->unpinned_collection()->GetHandle(),
        .index = 0ul,
    };

    EXPECT_CALL(
        observer,
        OnChildMoved(
            testing::AllOf(
                testing::Field(
                    &TabCollection::Position::parent_handle,
                    testing::Eq(expected_tab_to_position.parent_handle)),
                testing::Field(&TabCollection::Position::index,
                               testing::Eq(expected_tab_to_position.index))),
            testing::AllOf(
                testing::Field(
                    &TabCollectionObserver::NodeData::position,
                    testing::AllOf(
                        testing::Field(&TabCollection::Position::parent_handle,
                                       testing::Eq(expected_tab_from_position
                                                       .parent_handle)),
                        testing::Field(
                            &TabCollection::Position::index,
                            testing::Eq(expected_tab_from_position.index)))),
                testing::Field(
                    &TabCollectionObserver::NodeData::handle,
                    testing::VariantWith<TabHandle>(testing::Eq(tab_handle))))))
        .Times(1)
        .RetiresOnSaturation();

    const std::set<tabs::TabCollection::Type> retain_collection_types =
        std::set<tabs::TabCollection::Type>({tabs::TabCollection::Type::GROUP});
    collection->MoveTabsRecursive({0, 1}, 1, std::nullopt, false,
                                  retain_collection_types);
  }
}

TEST_F(TabCollectionObserverTest, MoveTabToPinnedOnlyNotifiesMoveInTabstrip) {
  tabs::TabStripCollection* collection = GetTabstripCollection();
  MockTabCollectionObserver& observer = GetObserver();

  // 1. Setup: Add one unpinned tab.
  std::unique_ptr<MockTabInterface> unpinned_tab = CreateMockTab();
  MockTabInterface* unpinned_tab_ptr = unpinned_tab.get();

  EXPECT_CALL(*unpinned_tab_ptr, GetParentCollection(testing::_))
      .WillRepeatedly(
          [&collection, unpinned_tab_ptr]() -> tabs::TabCollection* {
            if (collection->unpinned_collection()
                    ->GetIndexOfTab(unpinned_tab_ptr)
                    .has_value()) {
              return collection->unpinned_collection();
            }
            if (collection->pinned_collection()
                    ->GetIndexOfTab(unpinned_tab_ptr)
                    .has_value()) {
              return collection->pinned_collection();
            }

            return nullptr;
          });

  collection->AddTabRecursive(std::move(unpinned_tab), 0, std::nullopt, false);

  // 2. Expectations: Check only OnChildMoved is called.
  EXPECT_CALL(observer, OnChildrenAdded(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(observer, OnChildrenRemoved(testing::_, testing::_)).Times(0);
  EXPECT_CALL(observer, OnChildMoved(testing::_, testing::_)).Times(1);

  // 3. Action: Move the tab at index 0 to index 0, but set `is_pinned` to true.
  collection->MoveTabRecursive(0, 0, std::nullopt, true);
}

}  // namespace tabs
