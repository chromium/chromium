// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_settings_data_type_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/service/data_type_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DataTypeController;
using ::testing::Return;

class SupervisedUserSettingsDataTypeControllerTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(prefs::kSupervisedUserId,
                                                 std::string());
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SupervisedUserSettingsDataTypeControllerTest,
       SupervisedUserMeetsPreconditions) {
  pref_service_.SetString(prefs::kSupervisedUserId,
                          supervised_user::kChildAccountSUID);
  SupervisedUserSettingsDataTypeController controller(
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr, &pref_service_);
  EXPECT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller.GetPreconditionState());
}

TEST_F(SupervisedUserSettingsDataTypeControllerTest,
       NonSupervisedUserDoesNotMeetPreconditions) {
  SupervisedUserSettingsDataTypeController controller(
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr, &pref_service_);
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndClearData,
            controller.GetPreconditionState());
}

TEST_F(SupervisedUserSettingsDataTypeControllerTest,
       HasTransportModeDelegate) {
  SupervisedUserSettingsDataTypeController controller(
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr, &pref_service_);
  EXPECT_TRUE(
      controller.GetDelegateForTesting(syncer::SyncMode::kTransportOnly));
}
