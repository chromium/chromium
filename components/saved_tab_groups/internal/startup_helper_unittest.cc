// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/startup_helper.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_delegate.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;

namespace tab_groups {
namespace {

MATCHER_P(UuidEq, uuid, "") {
  return arg.saved_guid() == uuid;
}

}  // namespace

class StartupHelperTest : public testing::Test {
 public:
  StartupHelperTest()
      : local_group_id_1_(test::GenerateRandomTabGroupID()),
        local_group_id_2_(test::GenerateRandomTabGroupID()),
        group_1_(test::CreateTestSavedTabGroup()),
        group_2_(test::CreateTestSavedTabGroup()) {}

  ~StartupHelperTest() override = default;

  void SetUp() override {
    service_ = std::make_unique<MockTabGroupSyncService>();
    delegate_ = std::make_unique<MockTabGroupSyncDelegate>();
    startup_helper_ =
        std::make_unique<StartupHelper>(delegate_.get(), service_.get());
  }

  void TearDown() override {}

 protected:
  std::unique_ptr<MockTabGroupSyncService> service_;
  std::unique_ptr<MockTabGroupSyncDelegate> delegate_;
  std::unique_ptr<StartupHelper> startup_helper_;
  LocalTabGroupID local_group_id_1_;
  LocalTabGroupID local_group_id_2_;
  SavedTabGroup group_1_;
  SavedTabGroup group_2_;
};

TEST_F(StartupHelperTest, HandleOpenTabGroupRequest) {
  startup_helper_->InitializeTabGroupSync();
}

TEST_F(StartupHelperTest, CloseDeletedGroups) {
  EXPECT_CALL(*service_, GetDeletedGroupIds())
      .WillOnce(Return(std::vector<LocalTabGroupID>{local_group_id_1_}));
  EXPECT_CALL(*delegate_, CloseLocalTabGroup(Eq(local_group_id_1_)));

  startup_helper_->InitializeTabGroupSync();
}

TEST_F(StartupHelperTest, CreateRemoteGroupForNewLocalGroup) {
  group_1_.SetLocalGroupId(local_group_id_1_);
  EXPECT_CALL(*service_, GetGroup(local_group_id_1_))
      .WillOnce(Return(std::make_optional<SavedTabGroup>(group_1_)));
  EXPECT_CALL(*service_, GetGroup(local_group_id_2_))
      .WillOnce(Return(std::nullopt));

  EXPECT_CALL(*delegate_, GetLocalTabGroupIds())
      .WillRepeatedly(Return(
          std::vector<LocalTabGroupID>{local_group_id_1_, local_group_id_2_}));
  EXPECT_CALL(*delegate_, CreateRemoteTabGroup(_)).Times(1);

  startup_helper_->InitializeTabGroupSync();
}

TEST_F(StartupHelperTest, ReconcileGroupsToSync) {
  group_1_.SetLocalGroupId(local_group_id_1_);
  std::vector<SavedTabGroup> groups = {group_1_};

  EXPECT_CALL(*service_, GetAllGroups()).WillRepeatedly(Return(groups));
  EXPECT_CALL(*service_, GetGroup(group_1_.saved_guid()))
      .WillRepeatedly(Return(group_1_));

  EXPECT_CALL(*delegate_, UpdateLocalTabGroup(_)).Times(1);

  startup_helper_->InitializeTabGroupSync();
}

TEST_F(StartupHelperTest, UpdateTabIdMappings) {
  group_1_ = test::CreateTestSavedTabGroupWithNoTabs();
  group_1_.SetLocalGroupId(local_group_id_1_);

  SavedTabGroupTab tab1 =
      test::CreateSavedTabGroupTab("A_Link", u"Tab1", group_1_.saved_guid());
  SavedTabGroupTab tab2 =
      test::CreateSavedTabGroupTab("B_Link", u"Tab2", group_1_.saved_guid());
  group_1_.AddTabLocally(tab1);
  group_1_.AddTabLocally(tab2);

  std::vector<SavedTabGroup> groups = {group_1_};
  EXPECT_CALL(*service_, GetAllGroups()).WillRepeatedly(Return(groups));

  EXPECT_CALL(*delegate_, GetLocalTabGroupIds())
      .WillRepeatedly(Return(std::vector<LocalTabGroupID>{local_group_id_1_}));

  const LocalTabID kTab1LocalId = test::GenerateRandomTabID();
  const LocalTabID kTab2LocalId = test::GenerateRandomTabID();
  EXPECT_CALL(*delegate_, GetLocalTabIdsForTabGroup(local_group_id_1_))
      .WillOnce(Return(std::vector<LocalTabID>{kTab1LocalId, kTab2LocalId}));

  // Expect calls to map local tab ID for each tab.
  EXPECT_CALL(*service_, UpdateLocalTabId(_, _, _)).Times(2);

  startup_helper_->InitializeTabGroupSync();
}

}  // namespace tab_groups
