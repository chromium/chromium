// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/versioning_message_controller_impl.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/public/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
using MessageType = VersioningMessageController::MessageType;

namespace {

class VersioningMessageControllerImplTest : public testing::Test {
 public:
  VersioningMessageControllerImplTest() {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDataSharingHasShownVersionOutOfDateInstantMessage, false);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage,
        false);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDataSharingHasShownVersionUpdatedMessage, false);

    SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/false,
                                      /*has_open_shared_tab_groups=*/false);
    controller_ = std::make_unique<VersioningMessageControllerImpl>(
        &pref_service_, &mock_tab_group_sync_service_);
  }

  void SetPref(const std::string& pref_name, bool value) {
    pref_service_.SetBoolean(pref_name, value);
  }

  bool GetPref(const std::string& pref_name) {
    return pref_service_.GetBoolean(pref_name);
  }

  void SetTabGroupSyncServiceExpectation(bool had_shared_tab_groups,
                                         bool had_open_shared_tab_groups) {
    EXPECT_CALL(mock_tab_group_sync_service_,
                HadSharedTabGroupsLastSession(/*open_shared_tab_groups=*/false))
        .WillRepeatedly(testing::Return(had_shared_tab_groups));
    EXPECT_CALL(mock_tab_group_sync_service_,
                HadSharedTabGroupsLastSession(/*open_shared_tab_groups=*/true))
        .WillRepeatedly(testing::Return(had_open_shared_tab_groups));
  }

  void InitializeController() { controller_->OnInitialized(); }

  void ExpectMessageUiShouldBeShown(MessageType message_type,
                                    bool expected_value) {
    base::RunLoop run_loop;
    controller_->ShouldShowMessageUiAsync(
        message_type, base::BindOnce(
                          [](base::RunLoop* run_loop, bool expected_value,
                             bool actual_value) {
                            EXPECT_EQ(expected_value, actual_value);
                            run_loop->Quit();
                          },
                          &run_loop, expected_value));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  MockTabGroupSyncService mock_tab_group_sync_service_;
  std::unique_ptr<VersioningMessageControllerImpl> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(VersioningMessageControllerImplTest,
       TestPrefs_Startup_VersionOutOfDate) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetPref(prefs::kDataSharingHasShownVersionOutOfDateInstantMessage, true);
  SetPref(prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage,
          true);
  SetPref(prefs::kDataSharingHasShownVersionUpdatedMessage, true);
  InitializeController();
  controller_ = std::make_unique<VersioningMessageControllerImpl>(
      &pref_service_, &mock_tab_group_sync_service_);

  EXPECT_TRUE(
      GetPref(prefs::kDataSharingHasShownVersionOutOfDateInstantMessage));
  EXPECT_TRUE(GetPref(
      prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage));
  EXPECT_FALSE(GetPref(prefs::kDataSharingHasShownVersionUpdatedMessage));
}

TEST_F(VersioningMessageControllerImplTest, TestPrefs_Startup_VersionUpdated) {
  scoped_feature_list_.InitAndDisableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetPref(prefs::kDataSharingHasShownVersionOutOfDateInstantMessage, true);
  SetPref(prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage,
          true);
  SetPref(prefs::kDataSharingHasShownVersionUpdatedMessage, true);
  InitializeController();
  controller_ = std::make_unique<VersioningMessageControllerImpl>(
      &pref_service_, &mock_tab_group_sync_service_);

  EXPECT_FALSE(
      GetPref(prefs::kDataSharingHasShownVersionOutOfDateInstantMessage));
  EXPECT_FALSE(GetPref(
      prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage));
  EXPECT_TRUE(GetPref(prefs::kDataSharingHasShownVersionUpdatedMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_InstantMessage_VersionOutOfDate) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/true,
                                    /*has_open_shared_tab_groups=*/true);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
                               true);
}

TEST_F(
    VersioningMessageControllerImplTest,
    QueryIfMessageUiShouldBeShown_InstantMessage_VersionOutOfDate_NoSharedGroups) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/false,
                                    /*has_open_shared_tab_groups=*/false);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
                               false);
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_InstantMessage_VersionOutOfDate_PrefSet) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetPref(prefs::kDataSharingHasShownVersionOutOfDateInstantMessage, true);
  SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/true,
                                    /*has_open_shared_tab_groups=*/true);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
                               false);
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_InstantMessage_VersionUpdated) {
  scoped_feature_list_.InitAndDisableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
                               false);
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_PersistentMessage_VersionOutOfDate) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/true,
                                    /*has_open_shared_tab_groups=*/false);
  InitializeController();
  ExpectMessageUiShouldBeShown(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE, true);
}

TEST_F(
    VersioningMessageControllerImplTest,
    QueryIfMessageUiShouldBeShown_PersistentMessage_VersionOutOfDate_NoSharedGroups) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/false,
                                    /*has_open_shared_tab_groups=*/false);
  InitializeController();
  ExpectMessageUiShouldBeShown(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE, false);
}

TEST_F(
    VersioningMessageControllerImplTest,
    QueryIfMessageUiShouldBeShown_PersistentMessage_VersionOutOfDate_PrefSet) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetPref(prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage,
          true);
  SetTabGroupSyncServiceExpectation(/*has_shared_tab_groups=*/true,
                                    /*has_open_shared_tab_groups=*/false);
  InitializeController();
  ExpectMessageUiShouldBeShown(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE, false);
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_PersistentMessage_VersionUpdated) {
  scoped_feature_list_.InitAndDisableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  InitializeController();
  ExpectMessageUiShouldBeShown(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE, false);
}

TEST_F(VersioningMessageControllerImplTest,
       ExpectMessageUiShouldBeShown_ReenabledMessage_VersionOutOfDate) {
  scoped_feature_list_.InitAndEnableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_UPDATED_MESSAGE, false);
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_ReenabledMessage_VersionUpdated) {
  scoped_feature_list_.InitAndDisableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_UPDATED_MESSAGE, true);
}

TEST_F(VersioningMessageControllerImplTest,
       QueryIfMessageUiShouldBeShown_ReenabledMessage_VersionUpdated_PrefSet) {
  scoped_feature_list_.InitAndDisableFeature(
      data_sharing::features::kDataSharingEnableUpdateChromeUI);
  SetPref(prefs::kDataSharingHasShownVersionUpdatedMessage, true);
  InitializeController();
  ExpectMessageUiShouldBeShown(MessageType::VERSION_UPDATED_MESSAGE, false);
}

TEST_F(VersioningMessageControllerImplTest, OnMessageUiShown_InstantMessage) {
  InitializeController();
  controller_->OnMessageUiShown(
      MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
  EXPECT_TRUE(
      GetPref(prefs::kDataSharingHasShownVersionOutOfDateInstantMessage));
}

TEST_F(VersioningMessageControllerImplTest, OnMessageUiShown_ReenabledMessage) {
  controller_->OnMessageUiShown(MessageType::VERSION_UPDATED_MESSAGE);
  EXPECT_TRUE(GetPref(prefs::kDataSharingHasShownVersionUpdatedMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       OnMessageUiDismissed_PersistentMessage) {
  InitializeController();
  controller_->OnMessageUiDismissed(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
  EXPECT_TRUE(GetPref(
      prefs::kDataSharingHasDismissedVersionOutOfDatePersistentMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       Initialization_QueuedCallbacksFlushed) {
  bool callback_called = false;
  controller_->ShouldShowMessageUiAsync(
      MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
      base::BindOnce(
          [](bool* callback_called, bool result) { *callback_called = true; },
          &callback_called));
  EXPECT_FALSE(callback_called);
  InitializeController();
  EXPECT_TRUE(callback_called);
}

}  // namespace
}  // namespace tab_groups
