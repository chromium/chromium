// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/startup_helper.h"

#include "base/uuid.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/pref_names.h"
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
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDidSyncTabGroupsInLastSession, true);
    service_ = std::make_unique<MockTabGroupSyncService>();
    delegate_ = std::make_unique<MockTabGroupSyncDelegate>();
    startup_helper_ = std::make_unique<StartupHelper>(
        delegate_.get(), service_.get(), &pref_service_);
  }

  void TearDown() override {}

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MockTabGroupSyncService> service_;
  std::unique_ptr<MockTabGroupSyncDelegate> delegate_;
  std::unique_ptr<StartupHelper> startup_helper_;
  LocalTabGroupID local_group_id_1_;
  LocalTabGroupID local_group_id_2_;
  SavedTabGroup group_1_;
  SavedTabGroup group_2_;
};

TEST_F(StartupHelperTest, CloseDeletedGroups) {
  EXPECT_CALL(*service_, GetDeletedGroupIds())
      .WillOnce(Return(std::vector<LocalTabGroupID>{local_group_id_1_}));
  EXPECT_CALL(*delegate_, CloseLocalTabGroup(Eq(local_group_id_1_)));

  startup_helper_->CloseDeletedTabGroupsFromTabModel();
}

TEST_F(StartupHelperTest,
       SaveUnsavedLocalGroupsOnStartupForFirstTimeFeatureLaunch) {
  pref_service_.SetBoolean(prefs::kDidSyncTabGroupsInLastSession, false);
  group_1_.SetLocalGroupId(local_group_id_1_);
  EXPECT_CALL(*service_, GetGroup(local_group_id_1_))
      .WillOnce(Return(std::make_optional<SavedTabGroup>(group_1_)));
  EXPECT_CALL(*service_, GetGroup(local_group_id_2_))
      .WillOnce(Return(std::nullopt));

  EXPECT_CALL(*delegate_, GetLocalTabGroupIds())
      .WillRepeatedly(Return(
          std::vector<LocalTabGroupID>{local_group_id_1_, local_group_id_2_}));
  auto saved_tab_group_2 = std::make_unique<SavedTabGroup>(group_2_);
  EXPECT_CALL(*delegate_,
              CreateSavedTabGroupFromLocalGroup(Eq(local_group_id_2_)))
      .WillOnce(Return(std::move(saved_tab_group_2)));
  EXPECT_CALL(*service_, AddGroup(_)).Times(1);

  startup_helper_->HandleUnsavedLocalTabGroups();
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kDidSyncTabGroupsInLastSession));
}

TEST_F(StartupHelperTest, CloseUnsavedLocalGroupsOnStartup) {
  pref_service_.SetBoolean(prefs::kDidSyncTabGroupsInLastSession, true);
  group_1_.SetLocalGroupId(local_group_id_1_);
  EXPECT_CALL(*service_, GetGroup(local_group_id_1_))
      .WillOnce(Return(std::make_optional<SavedTabGroup>(group_1_)));
  EXPECT_CALL(*service_, GetGroup(local_group_id_2_))
      .WillOnce(Return(std::nullopt));

  EXPECT_CALL(*delegate_, GetLocalTabGroupIds())
      .WillRepeatedly(Return(
          std::vector<LocalTabGroupID>{local_group_id_1_, local_group_id_2_}));
  EXPECT_CALL(*delegate_, CloseLocalTabGroup(Eq(local_group_id_2_)));

  startup_helper_->HandleUnsavedLocalTabGroups();
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kDidSyncTabGroupsInLastSession));
}

}  // namespace tab_groups
