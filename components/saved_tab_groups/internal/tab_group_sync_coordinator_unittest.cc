// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/uuid.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/internal/tab_group_sync_coordinator_impl.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_delegate.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace tab_groups {
namespace {

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class TabGroupSyncCoordinatorTest : public testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<MockTabGroupSyncService>();
    auto delegate = std::make_unique<MockTabGroupSyncDelegate>();
    delegate_ = delegate.get();
    coordinator_ = std::make_unique<TabGroupSyncCoordinatorImpl>(
        std::move(delegate), service_.get());
  }

  void TearDown() override { delegate_ = nullptr; }

 protected:
  std::unique_ptr<MockTabGroupSyncService> service_;
  raw_ptr<MockTabGroupSyncDelegate> delegate_;
  std::unique_ptr<TabGroupSyncCoordinatorImpl> coordinator_;
};

TEST_F(TabGroupSyncCoordinatorTest, HandleOpenTabGroupRequest) {
  base::Uuid sync_id = base::Uuid::GenerateRandomV4();
  auto context = std::make_unique<TabGroupActionContext>();

  EXPECT_CALL(*delegate_, HandleOpenTabGroupRequest(sync_id, testing::_))
      .Times(1);
  coordinator_->HandleOpenTabGroupRequest(sync_id, std::move(context));
}

TEST_F(TabGroupSyncCoordinatorTest, ConnectLocalTabGroup) {
  SavedTabGroup group(test::CreateTestSavedTabGroup());
  base::Uuid sync_id = group.saved_guid();
  LocalTabGroupID local_id = test::GenerateRandomTabGroupID();

  EXPECT_CALL(*service_, GetGroup(sync_id))
      .Times(2)
      .WillRepeatedly(Return(group));
  EXPECT_CALL(*service_,
              UpdateLocalTabGroupMapping(sync_id, local_id,
                                         OpeningSource::kUndoCloseAllTabs))
      .Times(1);
  EXPECT_CALL(*delegate_, UpdateLocalTabGroup(UuidEq(sync_id))).Times(1);

  coordinator_->ConnectLocalTabGroup(sync_id, local_id,
                                     OpeningSource::kUndoCloseAllTabs);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupAdded) {
  SavedTabGroup group(test::CreateTestSavedTabGroup());

  EXPECT_CALL(*delegate_, CreateLocalTabGroup(UuidEq(group.saved_guid())))
      .Times(1);
  coordinator_->OnTabGroupAdded(group, TriggerSource::REMOTE);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupAdded_DoesNotReopenClosedGroup) {
  SavedTabGroup group(test::CreateTestSavedTabGroup());

  ON_CALL(*service_, WasTabGroupClosedLocally(group.saved_guid()))
      .WillByDefault(Return(true));

  // Since the group is marked as locally-closed, it should *not* get opened!
  EXPECT_CALL(*delegate_, CreateLocalTabGroup(UuidEq(group.saved_guid())))
      .Times(0);
  coordinator_->OnTabGroupAdded(group, TriggerSource::REMOTE);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupUpdated) {
  SavedTabGroup group(test::CreateTestSavedTabGroup());

  EXPECT_CALL(*delegate_, UpdateLocalTabGroup(UuidEq(group.saved_guid())))
      .Times(1);
  coordinator_->OnTabGroupUpdated(group, TriggerSource::REMOTE);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupRemoved) {
  LocalTabGroupID local_id = test::GenerateRandomTabGroupID();

  EXPECT_CALL(
      *delegate_,
      CloseLocalTabGroup(testing::TypedEq<const LocalTabGroupID&>(local_id)))
      .Times(1);
  coordinator_->OnTabGroupRemoved(local_id, TriggerSource::REMOTE);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupRemovedWithSyncId) {
  base::Uuid sync_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*delegate_, CloseLocalTabGroup(testing::_)).Times(0);
  coordinator_->OnTabGroupRemoved(sync_id, TriggerSource::REMOTE);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupAddedFromLocal) {
  SavedTabGroup group(test::CreateTestSavedTabGroup());

  EXPECT_CALL(*delegate_, CreateLocalTabGroup(_)).Times(0);
  coordinator_->OnTabGroupAdded(group, TriggerSource::LOCAL);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupUpdatedFromLocal) {
  SavedTabGroup group(test::CreateTestSavedTabGroup());

  EXPECT_CALL(*delegate_, UpdateLocalTabGroup(_)).Times(0);
  coordinator_->OnTabGroupUpdated(group, TriggerSource::LOCAL);
}

TEST_F(TabGroupSyncCoordinatorTest, OnTabGroupRemovedFromLocal) {
  LocalTabGroupID local_id = test::GenerateRandomTabGroupID();

  EXPECT_CALL(*delegate_, CloseLocalTabGroup(_)).Times(0);
  coordinator_->OnTabGroupRemoved(local_id, TriggerSource::LOCAL);
}

}  // namespace tab_groups
