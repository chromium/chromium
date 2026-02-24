// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter_impl.h"

#include <memory>

#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/browser_apis/tab_strip/types/position.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {

class TabStripModelAdapterImplTest : public testing::Test {
 public:
  TabStripModelAdapterImplTest() = default;
  ~TabStripModelAdapterImplTest() override = default;

  void SetUp() override {
    model_ = std::make_unique<TabStripModel>(&delegate_, &profile_);
    adapter_ = std::make_unique<TabStripModelAdapterImpl>(model_.get(), "1");
  }

  void TearDown() override {
    adapter_.reset();
    model_.reset();
  }

  void AddTabs(int count) {
    for (int i = 0; i < count; ++i) {
      model_->AppendWebContents(
          content::WebContentsTester::CreateTestWebContents(&profile_, nullptr),
          true);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  TestingProfile profile_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> model_;
  std::unique_ptr<TabStripModelAdapterImpl> adapter_;
};

TEST_F(TabStripModelAdapterImplTest, GetPositionForAbsoluteIndex) {
  AddTabs(3);
  Position pos = adapter_->GetPositionForAbsoluteIndex(0);
  EXPECT_EQ(pos.index(), 0u);
  EXPECT_FALSE(pos.path().components().empty());

  // Test pinned collection.
  model_->SetTabPinned(0, true);
  pos = adapter_->GetPositionForAbsoluteIndex(0);
  EXPECT_EQ(pos.index(), 0u);
  EXPECT_EQ(
      pos.path().components().back(),
      NodeId::FromTabCollectionHandle(model_->GetPinnedTabsCollectionHandle(
          TabStripModelAdapterImpl::PassKeyForTesting())));

  // Test unpinned collection.
  pos = adapter_->GetPositionForAbsoluteIndex(1);
  EXPECT_EQ(pos.index(), 0u);  // First tab of unpinned collection.
  EXPECT_EQ(
      pos.path().components().back(),
      NodeId::FromTabCollectionHandle(model_->GetUnpinnedTabsCollectionHandle(
          TabStripModelAdapterImpl::PassKeyForTesting())));

  // Test tab group.
  tab_groups::TabGroupId group_id = model_->AddToNewGroup({1});
  pos = adapter_->GetPositionForAbsoluteIndex(1);
  EXPECT_EQ(pos.index(), 0u);  // First tab of group.
  EXPECT_EQ(
      pos.path().components().back(),
      NodeId::FromTabCollectionHandle(
          model_->group_model()->GetTabGroup(group_id)->GetCollectionHandle()));

  // Test last tab in unpinned.
  pos = adapter_->GetPositionForAbsoluteIndex(2);
  EXPECT_EQ(pos.index(), 1u);  // Second tab after the group in unpinned.
  EXPECT_EQ(
      pos.path().components().back(),
      NodeId::FromTabCollectionHandle(model_->GetUnpinnedTabsCollectionHandle(
          TabStripModelAdapterImpl::PassKeyForTesting())));
}

TEST_F(TabStripModelAdapterImplTest, GetPathForCollection) {
  AddTabs(1);
  tab_groups::TabGroupId group_id = model_->AddToNewGroup({0});

  tabs::TabCollectionHandle group_handle =
      model_->group_model()->GetTabGroup(group_id)->GetCollectionHandle();

  Path path = adapter_->GetPathForCollection(group_handle);
  // Path is: Window -> TabStrip -> Unpinned -> Group
  ASSERT_GE(path.components().size(), 4u);
  EXPECT_EQ(path.components()[0], NodeId::FromWindowId("1"));
  EXPECT_EQ(path.components()[1],
            NodeId::FromTabCollectionHandle(
                model_->GetRootForTesting()->GetHandle()));
  EXPECT_EQ(path.components().back(),
            NodeId::FromTabCollectionHandle(group_handle));
}

TEST_F(TabStripModelAdapterImplTest, CalculateInsertionParams) {
  AddTabs(2);
  model_->SetTabPinned(0, true);
  tab_groups::TabGroupId group_id = model_->AddToNewGroup({1});

  // Test Pinned insertion.
  Path pinned_path =
      adapter_->GetPathForCollection(model_->GetPinnedTabsCollectionHandle(
          TabStripModelAdapterImpl::PassKeyForTesting()));
  Position pinned_pos(1, pinned_path);
  InsertionParams params = adapter_->CalculateInsertionParams(pinned_pos);
  EXPECT_EQ(params.index, 1u);
  EXPECT_TRUE(params.pinned);
  EXPECT_FALSE(params.group_id.has_value());

  // Test Group insertion.
  Path group_path = adapter_->GetPathForCollection(
      model_->group_model()->GetTabGroup(group_id)->GetCollectionHandle());
  Position group_pos(0, group_path);
  params = adapter_->CalculateInsertionParams(group_pos);
  EXPECT_EQ(params.index, 0u);
  EXPECT_FALSE(params.pinned);
  ASSERT_TRUE(params.group_id.has_value());
  EXPECT_EQ(params.group_id.value(), group_id);
}

TEST_F(TabStripModelAdapterImplTest, MoveTab) {
  AddTabs(3);
  tabs::TabHandle handle0 = model_->GetTabAtIndex(0)->GetHandle();

  // Move t0 to Index 2 (unpinned)
  adapter_->MoveTab(handle0, Position(2));
  EXPECT_EQ(model_->GetIndexOfTab(handle0.Get()), 2);

  // Pin t0 by moving to pinned
  Path pinned_path =
      adapter_->GetPathForCollection(model_->GetPinnedTabsCollectionHandle(
          TabStripModelAdapterImpl::PassKeyForTesting()));
  adapter_->MoveTab(handle0, Position(0, pinned_path));
  EXPECT_TRUE(handle0.Get()->IsPinned());
  EXPECT_EQ(model_->GetIndexOfTab(handle0.Get()), 0);

  // Move t1 into a group1
  tab_groups::TabGroupId group_id = model_->AddToNewGroup({1});
  Path group_path = adapter_->GetPathForCollection(
      model_->group_model()->GetTabGroup(group_id)->GetCollectionHandle());

  // Move t2 into group1
  tabs::TabHandle handle2 = model_->GetTabAtIndex(2)->GetHandle();
  adapter_->MoveTab(handle2, Position(1, group_path));
  EXPECT_EQ(model_->GetTabGroupForTab(2), group_id);
}

TEST_F(TabStripModelAdapterImplTest, MoveCollection) {
  AddTabs(3);
  tab_groups::TabGroupId group_id = model_->AddToNewGroup({0, 1});

  tabs::TabCollectionHandle group_handle =
      model_->group_model()->GetTabGroup(group_id)->GetCollectionHandle();
  NodeId group_node = NodeId::FromTabCollectionHandle(group_handle);

  // Move group to index 1 (after t2)
  adapter_->MoveCollection(group_node, Position(1));
  // Tab order should be: [t2] [g:{t0, t1}]
  // So t2 is at index 0, and group starts at index 1.
  EXPECT_EQ(model_->GetTabGroupForTab(1), group_id);
  EXPECT_EQ(model_->GetTabGroupForTab(2), group_id);
  EXPECT_FALSE(model_->GetTabGroupForTab(0).has_value());
}

}  // namespace tabs_api
