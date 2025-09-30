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

  collection->InsertTabCollectionAt(std::move(grouped_collection), 0, false,
                                    std::nullopt);

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

TEST_F(TabCollectionObserverTest, OnTabCollectionAttached) {
  MockTabCollectionObserver& observer = GetObserver();

  // Setup with one group
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

  tabs::TabCollectionHandle group_handle = grouped_collection->GetHandle();
  TabCollectionNodes expected_handles;
  expected_handles.push_back(group_handle);

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

  const tabs::TabCollectionObserver::Position expected_position_split = {
      .parent_handle = group_handle,
      .index = 1ul,
  };

  EXPECT_CALL(
      observer,
      OnChildrenAdded(
          testing::AllOf(
              testing::Field(
                  &TabCollectionObserver::Position::parent_handle,
                  testing::Eq(expected_position_split.parent_handle)),
              testing::Field(&TabCollectionObserver::Position::index,
                             testing::Eq(expected_position_split.index))),
          testing::Eq(expected_handles_split)))
      .Times(1);

  collection->InsertTabCollectionAt(std::move(split_collection_unique), 1,
                                    false, group_id);
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

  // First Notification: The Split Collection is added to the parent
  const tabs::TabCollectionObserver::Position expected_split_position = {
      .parent_handle = collection->unpinned_collection()->GetHandle(),
      .index = 2ul,
  };

  tabs::TabCollectionHandle new_split_handle;

  EXPECT_CALL(
      observer,
      OnChildrenAdded(
          testing::AllOf(
              testing::Field(
                  &TabCollectionObserver::Position::parent_handle,
                  testing::Eq(expected_split_position.parent_handle)),
              testing::Field(&TabCollectionObserver::Position::index,
                             testing::Eq(expected_split_position.index))),
          testing::SizeIs(1)))
      .WillOnce(
          testing::Invoke([&](const TabCollectionObserver::Position& position,
                              const TabCollectionNodes& handles) {
            // Save the handle of the newly added split collection for the next
            // expectation.
            new_split_handle =
                std::get<tabs::TabCollection::Handle>(handles[0]);
            EXPECT_EQ(new_split_handle.Get()->type(),
                      TabCollection::Type::SPLIT);
          }))
      .RetiresOnSaturation();

  // Second Notification: Tabs are added to the split collection.
  TabCollectionNodes expected_tab_handles;
  expected_tab_handles.push_back(
      collection->GetTabAtIndexRecursive(2)->GetHandle());
  expected_tab_handles.push_back(
      collection->GetTabAtIndexRecursive(3)->GetHandle());

  EXPECT_CALL(
      observer,
      OnChildrenAdded(
          testing::AllOf(
              testing::Field(&TabCollectionObserver::Position::parent_handle,
                             testing::Eq(testing::ByRef(new_split_handle))),
              testing::Field(&TabCollectionObserver::Position::index,
                             testing::Eq(0ul))),
          testing::Eq(expected_tab_handles)))
      .Times(1);

  collection->CreateSplit(split_id, tabs_to_split,
                          split_tabs::SplitTabVisualData(
                              split_tabs::SplitTabLayout::kVertical, 0.5));
}

}  // namespace tabs
