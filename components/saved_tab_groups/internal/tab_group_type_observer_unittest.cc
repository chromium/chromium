// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_type_observer.h"

#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/synthetic_field_trial_helper.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace tab_groups {
namespace {
const char kGroupId[] = "/?-group_id";

class MockSyntheticFieldTrialHelper : public SyntheticFieldTrialHelper {
 public:
  MockSyntheticFieldTrialHelper()
      : SyntheticFieldTrialHelper(base::DoNothing(), base::DoNothing()) {}
  MOCK_METHOD(void,
              UpdateHadSavedTabGroupIfNeeded,
              (bool has_saved_tab_group),
              (override));
  MOCK_METHOD(void,
              UpdateHadSharedTabGroupIfNeeded,
              (bool has_shared_tab_group),
              (override));
};

class TabGroupTypeObserverTest : public testing::Test {
 public:
  TabGroupTypeObserverTest() : group_1_(test::CreateTestSavedTabGroup()) {}

  ~TabGroupTypeObserverTest() override {
    EXPECT_CALL(*service_.get(), RemoveObserver(_)).Times(1);
  }

  void SetUp() override {
    service_ = std::make_unique<MockTabGroupSyncService>();
    EXPECT_CALL(*service_.get(), AddObserver(_)).Times(1);
    observer_ = std::make_unique<TabGroupTypeObserver>(
        service_.get(), &synthetic_field_trial_helper_);
  }

 protected:
  MockSyntheticFieldTrialHelper synthetic_field_trial_helper_;
  std::unique_ptr<MockTabGroupSyncService> service_;
  std::unique_ptr<TabGroupTypeObserver> observer_;
  SavedTabGroup group_1_;
};

TEST_F(TabGroupTypeObserverTest, NoTabGroupAvailableOnServiceInitialization) {
  EXPECT_CALL(*service_.get(), ReadAllGroups())
      .WillOnce(Return(std::vector<const SavedTabGroup*>()));
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSavedTabGroupIfNeeded(false))
      .Times(1);
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSharedTabGroupIfNeeded(false))
      .Times(1);
  observer_->OnInitialized();
}

TEST_F(TabGroupTypeObserverTest,
       SavedTabGroupAvailableOnServiceInitialization) {
  EXPECT_CALL(*service_.get(), ReadAllGroups())
      .WillOnce(Return(std::vector<const SavedTabGroup*>{&group_1_}));
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSavedTabGroupIfNeeded(true))
      .Times(1);
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSharedTabGroupIfNeeded(false))
      .Times(1);
  observer_->OnInitialized();
}

TEST_F(TabGroupTypeObserverTest,
       SharedTabGroupAvailableOnServiceInitialization) {
  group_1_.SetCollaborationId(CollaborationId(std::string(kGroupId)));
  EXPECT_CALL(*service_.get(), ReadAllGroups())
      .WillOnce(Return(std::vector<const SavedTabGroup*>{&group_1_}));
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSavedTabGroupIfNeeded(true))
      .Times(1);
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSharedTabGroupIfNeeded(true))
      .Times(1);
  observer_->OnInitialized();
}

TEST_F(TabGroupTypeObserverTest, OnTabGroupAdded) {
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSavedTabGroupIfNeeded(true))
      .Times(4);
  observer_->OnTabGroupAdded(group_1_, TriggerSource::LOCAL);
  observer_->OnTabGroupAdded(group_1_, TriggerSource::LOCAL);

  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSharedTabGroupIfNeeded(true))
      .Times(2);
  group_1_.SetCollaborationId(CollaborationId(std::string(kGroupId)));
  observer_->OnTabGroupAdded(group_1_, TriggerSource::REMOTE);
  observer_->OnTabGroupAdded(group_1_, TriggerSource::REMOTE);
}

TEST_F(TabGroupTypeObserverTest, OnTabGroupMigrated) {
  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSavedTabGroupIfNeeded(true))
      .Times(1);
  observer_->OnTabGroupAdded(group_1_, TriggerSource::LOCAL);

  EXPECT_CALL(synthetic_field_trial_helper_,
              UpdateHadSharedTabGroupIfNeeded(true))
      .Times(2);
  group_1_.SetCollaborationId(CollaborationId(std::string(kGroupId)));
  observer_->OnTabGroupMigrated(group_1_, base::Uuid::GenerateRandomV4(),
                                TriggerSource::LOCAL);
  observer_->OnTabGroupMigrated(group_1_, base::Uuid::GenerateRandomV4(),
                                TriggerSource::LOCAL);
}

}  // namespace
}  // namespace tab_groups
