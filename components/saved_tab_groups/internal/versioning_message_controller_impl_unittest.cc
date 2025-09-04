// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/versioning_message_controller_impl.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/public/features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/saved_tab_groups/public/versioning_message_controller.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
using MessageType = VersioningMessageController::MessageType;

namespace {

class VersioningMessageControllerImplTest : public testing::Test {
 public:
  VersioningMessageControllerImplTest()
      : tab_group1_(test::CreateTestSavedTabGroup()) {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kEligibleForVersionOutOfDateInstantMessage, false);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kEligibleForVersionOutOfDatePersistentMessage, false);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kEligibleForVersionUpdatedMessage, false);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kHasShownAnyVersionOutOfDateMessage, false);

    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
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

  void SetTabGroupSyncServiceCurrentExpectation(bool has_shared_tab_groups) {
    if (has_shared_tab_groups) {
      tab_group1_.SetCollaborationId(
          syncer::CollaborationId("collaboration_id"));
    }
    EXPECT_CALL(mock_tab_group_sync_service_, ReadAllGroups())
        .WillRepeatedly(
            testing::Return(std::vector<const SavedTabGroup*>({&tab_group1_})));
  }

  void MimicTabGroupAdded(bool is_shared_group) {
    if (is_shared_group) {
      tab_group1_.SetCollaborationId(
          syncer::CollaborationId("collaboration_id"));
    }
    SetTabGroupSyncServiceCurrentExpectation(is_shared_group);
    controller_->OnTabGroupAdded(tab_group1_, TriggerSource::REMOTE);
  }

  void InitializeController() {
    controller_ = std::make_unique<VersioningMessageControllerImpl>(
        &pref_service_, &mock_tab_group_sync_service_);
    controller_->OnInitialized();
  }

  bool RunShouldShowMessageUiAsync(MessageType message_type) {
    base::RunLoop run_loop;
    bool actual_result = false;

    controller_->ShouldShowMessageUiAsync(
        message_type, base::BindOnce(
                          [](base::RunLoop* run_loop, bool* out_actual_value,
                             bool received_value) {
                            *out_actual_value = received_value;
                            run_loop->Quit();
                          },
                          &run_loop, &actual_result));
    run_loop.Run();

    return actual_result;
  }

  bool ShouldShowMessageUi(MessageType message_type) {
    bool sync_result = controller_->ShouldShowMessageUi(message_type);
    bool async_result = RunShouldShowMessageUiAsync(message_type);
    EXPECT_EQ(sync_result, async_result);
    return sync_result;
  }

  void SetupFeatureList(bool version_up_to_date) {
    SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/version_up_to_date,
                               /*version_ui_flag_enabled=*/!version_up_to_date,
                               /*data_sharing_enabled=*/true);
  }

  void SetupFeatureListWithUiFlag(bool shared_data_types_enabled,
                                  bool version_ui_flag_enabled,
                                  bool data_sharing_enabled = true) {
    scoped_feature_list_.Reset();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (shared_data_types_enabled) {
      disabled_features.emplace_back(
          data_sharing::features::kSharedDataTypesKillSwitch);
    } else {
      enabled_features.emplace_back(
          data_sharing::features::kSharedDataTypesKillSwitch);
    }
    if (version_ui_flag_enabled) {
      enabled_features.emplace_back(
          data_sharing::features::kDataSharingEnableUpdateChromeUI);
    } else {
      disabled_features.emplace_back(
          data_sharing::features::kDataSharingEnableUpdateChromeUI);
    }
    if (data_sharing_enabled) {
      enabled_features.emplace_back(
          data_sharing::features::kDataSharingFeature);
    } else {
      disabled_features.emplace_back(
          data_sharing::features::kDataSharingFeature);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  MockTabGroupSyncService mock_tab_group_sync_service_;
  std::unique_ptr<VersioningMessageControllerImpl> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  SavedTabGroup tab_group1_;
};

TEST_F(VersioningMessageControllerImplTest,
       ShouldShowMessageUiAsync_VersionOutOfDate_HasSharedGroups) {
  SetupFeatureList(/*version_up_to_date=*/false);
  SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                    /*had_open_shared_tab_groups=*/true);
  InitializeController();
  EXPECT_TRUE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_TRUE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(VersioningMessageControllerImplTest,
       ShouldShowMessageUiAsync_VersionOutOfDate_HasSharedGroups_NoUiFlag) {
  SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/false,
                             /*version_ui_flag_enabled=*/false);
  SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                    /*had_open_shared_tab_groups=*/true);
  InitializeController();
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

  // Verify prefs.
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDateInstantMessage));
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage));
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionUpdatedMessage));
  EXPECT_FALSE(GetPref(prefs::kHasShownAnyVersionOutOfDateMessage));
}

TEST_F(
    VersioningMessageControllerImplTest,
    ShouldShowMessageUiAsync_VersionOutOfDate_HasSharedGroups_InvalidStateOfFlags) {
  // This is a test for invalid combinations of flags where data type is enabled
  // but update chrome UI flag is also incorrectly enabled.
  SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/true,
                             /*version_ui_flag_enabled=*/true);
  SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                    /*had_open_shared_tab_groups=*/true);
  InitializeController();
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

  // Verify prefs.
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDateInstantMessage));
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage));
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionUpdatedMessage));
  EXPECT_FALSE(GetPref(prefs::kHasShownAnyVersionOutOfDateMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       ShouldShowMessageUiAsync_VersionOutOfDate_NoSharedGroups) {
  SetupFeatureList(/*version_up_to_date=*/false);
  SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                    /*had_open_shared_tab_groups=*/false);
  InitializeController();
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(VersioningMessageControllerImplTest,
       ShouldShowMessageUiAsync_VersionOutOfDate_NoOpenSharedGroups) {
  SetupFeatureList(/*version_up_to_date=*/false);
  SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                    /*had_open_shared_tab_groups=*/false);
  InitializeController();
  if (AreLocalIdsPersisted()) {
    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  } else {
    EXPECT_TRUE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  }
  EXPECT_TRUE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(
    VersioningMessageControllerImplTest,
    ShouldShowMessageUiAsync_VersionOutOfDate_HasShownButNotDismissedBefore) {
  SetupFeatureList(/*version_up_to_date=*/false);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  SetPref(prefs::kEligibleForVersionOutOfDateInstantMessage, false);
  SetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage, true);

  InitializeController();
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_TRUE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(
    VersioningMessageControllerImplTest,
    ShouldShowMessageUiAsync_VersionUpdatedMessage_VersionUpdated_NotShownIfOutOfDateMessageNeverShown) {
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, false);
  SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
  InitializeController();

  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(
    VersioningMessageControllerImplTest,
    ShouldShowMessageUiAsync_VersionUpdatedMessage_VersionUpdated_ShownIfOutOfDateMessageShownBefore) {
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
  InitializeController();

  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_TRUE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(VersioningMessageControllerImplTest, OnMessageUiShown_InstantMessage) {
  SetupFeatureList(/*version_up_to_date=*/false);
  SetPref(prefs::kEligibleForVersionOutOfDateInstantMessage, true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, false);
  InitializeController();
  controller_->OnMessageUiShown(
      MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDateInstantMessage));
  EXPECT_TRUE(GetPref(prefs::kHasShownAnyVersionOutOfDateMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       OnMessageUiShown_PersistentMessage) {
  SetupFeatureList(/*version_up_to_date=*/false);
  SetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage, true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  InitializeController();
  controller_->OnMessageUiShown(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
  EXPECT_TRUE(GetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage));
  EXPECT_TRUE(GetPref(prefs::kHasShownAnyVersionOutOfDateMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       OnMessageUiShown_VersionUpdatedMessage) {
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kEligibleForVersionUpdatedMessage, true);
  InitializeController();
  controller_->OnMessageUiShown(MessageType::VERSION_UPDATED_MESSAGE);
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionUpdatedMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       OnMessageUiDismissed_PersistentMessage) {
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage, true);
  InitializeController();
  controller_->OnMessageUiDismissed(
      MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage));
}

TEST_F(VersioningMessageControllerImplTest,
       Initialization_QueuedCallbacksFlushed) {
  SetupFeatureList(/*version_up_to_date=*/true);
  controller_ = std::make_unique<VersioningMessageControllerImpl>(
      &pref_service_, &mock_tab_group_sync_service_);
  bool callback_called = false;
  controller_->ShouldShowMessageUiAsync(
      MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
      base::BindOnce(
          [](bool* callback_called, bool result) { *callback_called = true; },
          &callback_called));
  EXPECT_FALSE(callback_called);
  controller_->OnInitialized();
  EXPECT_TRUE(callback_called);
}

TEST_F(VersioningMessageControllerImplTest,
       VersionUpdatedCallback_WaitsForGroup) {
  // Setup: Eligible for updated message, but no shared groups yet.
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
  InitializeController();

  base::RunLoop run_loop;
  bool result = false;
  controller_->ShouldShowMessageUiAsync(
      MessageType::VERSION_UPDATED_MESSAGE,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* result, bool received_value) {
            *result = received_value;
            run_loop->Quit();
          },
          &run_loop, &result));

  // The callback should not have been called yet because we are waiting for a
  // shared group to be added.
  task_environment_.RunUntilIdle();

  // Simulate a shared group being added.
  MimicTabGroupAdded(/*is_shared_group=*/true);

  // Now the callback should be called with true, quitting the run loop.
  run_loop.Run();
  EXPECT_TRUE(result);
}

TEST_F(VersioningMessageControllerImplTest,
       VersionUpdatedCallback_ResolvesImmediatelyIfIneligible) {
  // Setup: Not eligible for updated message because version is out of date.
  SetupFeatureList(/*version_up_to_date=*/false);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
  InitializeController();

  // The callback should be called immediately with false.
  EXPECT_FALSE(
      RunShouldShowMessageUiAsync(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(VersioningMessageControllerImplTest,
       VersionUpdatedCallback_ResolvesImmediatelyIfGroupExists) {
  // Setup: Eligible and a shared group already exists.
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
  InitializeController();

  // The callback should be called immediately with true.
  EXPECT_TRUE(
      RunShouldShowMessageUiAsync(MessageType::VERSION_UPDATED_MESSAGE));
}

TEST_F(VersioningMessageControllerImplTest,
       VersionUpdatedCallback_MultipleCallbacksAreQueued) {
  // Setup: Eligible for updated message, but no shared groups yet.
  SetupFeatureList(/*version_up_to_date=*/true);
  SetPref(prefs::kHasShownAnyVersionOutOfDateMessage, true);
  SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
  InitializeController();

  bool first_callback_called = false;
  bool second_callback_called = false;

  controller_->ShouldShowMessageUiAsync(
      MessageType::VERSION_UPDATED_MESSAGE,
      base::BindOnce([](bool* called, bool /*result*/) { *called = true; },
                     &first_callback_called));
  controller_->ShouldShowMessageUiAsync(
      MessageType::VERSION_UPDATED_MESSAGE,
      base::BindOnce([](bool* called, bool /*result*/) { *called = true; },
                     &second_callback_called));

  // No callbacks should have been called yet.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(first_callback_called);
  EXPECT_FALSE(second_callback_called);

  // Simulate a shared group being added.
  MimicTabGroupAdded(/*is_shared_group=*/true);

  // Both callbacks should now be called.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first_callback_called);
  EXPECT_TRUE(second_callback_called);
}

TEST_F(VersioningMessageControllerImplTest, MultipleRestarts) {
  {
    // Start with version up-to-date.
    SetupFeatureList(/*version_up_to_date=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                      /*had_open_shared_tab_groups=*/true);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
  }

  {
    // Restart with version out-of-date.
    SetupFeatureList(/*version_up_to_date=*/false);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                      /*had_open_shared_tab_groups=*/true);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_TRUE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

    // Don't show the messages yet.
  }

  {
    // Restart again with version out-of-date. Since no message was actually
    // shown last session, they should still be eligible to be shown.
    SetupFeatureList(/*version_up_to_date=*/false);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_TRUE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

    controller_->OnMessageUiShown(
        MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
    controller_->OnMessageUiShown(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
  }

  {
    // Restart with version out-of-date. Only persistent message can be shown
    // here since it isn't dismissed yet.
    SetupFeatureList(/*version_up_to_date=*/false);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

    controller_->OnMessageUiShown(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    controller_->OnMessageUiDismissed(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
  }

  {
    // Restart with version out-of-date. No message can be shown since all the
    // message have been already shown and dismissed.
    SetupFeatureList(/*version_up_to_date=*/false);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
  }

  {
    // Restart with version up-to-date. Expect IPH to be eligible to be shown.
    SetupFeatureList(/*version_up_to_date=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

    // Don't show the IPH yet. Show it in next session.
  }

  {
    // Restart with version up-to-date. IPH wasn't shown in last session, so it
    // should still be eligible to be shown.
    SetupFeatureList(/*version_up_to_date=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

    controller_->OnMessageUiShown(MessageType::VERSION_UPDATED_MESSAGE);
  }

  {
    // Restart with version up-to-date. IPH shouldn't be shown as it was shown
    // before.
    SetupFeatureList(/*version_up_to_date=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
  }
}

TEST_F(VersioningMessageControllerImplTest, MultipleRestarts_ButWithoutUiFlag) {
  {
    // Start test with version out-of-date, but UI flag enabled.
    SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/false,
                               /*version_ui_flag_enabled=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                      /*had_open_shared_tab_groups=*/true);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_TRUE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

    controller_->OnMessageUiShown(
        MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
    controller_->OnMessageUiShown(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
  }

  {
    // Restart with version out-of-date, UI flag disabled.
    SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/false,
                               /*version_ui_flag_enabled=*/false);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
  }

  {
    // Restart with version out-of-date, UI flag enabled again.
    SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/false,
                               /*version_ui_flag_enabled=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ false);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
  }

  {
    // Restart with version up-to-date. IPH wasn't shown in last session, so it
    // should still be eligible to be shown.
    SetupFeatureList(/*version_up_to_date=*/true);
    SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/false,
                                      /*had_open_shared_tab_groups=*/false);
    SetTabGroupSyncServiceCurrentExpectation(/*has_shared_tab_groups*/ true);
    InitializeController();

    EXPECT_FALSE(
        ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
    EXPECT_FALSE(ShouldShowMessageUi(
        MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
    EXPECT_TRUE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));
  }
}

TEST_F(VersioningMessageControllerImplTest,
       TestShouldShowMessageUi_DataSharingDisabled) {
  SetupFeatureListWithUiFlag(/*shared_data_types_enabled=*/false,
                             /*version_ui_flag_enabled=*/true,
                             /*data_sharing_enabled*/ false);
  SetTabGroupSyncServiceExpectation(/*had_shared_tab_groups=*/true,
                                    /*had_open_shared_tab_groups=*/true);
  InitializeController();

  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
  EXPECT_FALSE(
      ShouldShowMessageUi(MessageType::VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
  EXPECT_FALSE(ShouldShowMessageUi(MessageType::VERSION_UPDATED_MESSAGE));

  // Verify prefs are not changed on startup in this state.
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDateInstantMessage));
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionOutOfDatePersistentMessage));
  EXPECT_FALSE(GetPref(prefs::kEligibleForVersionUpdatedMessage));
  EXPECT_FALSE(GetPref(prefs::kHasShownAnyVersionOutOfDateMessage));
}

}  // namespace
}  // namespace tab_groups
