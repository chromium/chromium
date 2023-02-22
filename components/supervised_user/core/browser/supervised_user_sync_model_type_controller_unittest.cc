// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_sync_model_type_controller.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/driver/data_type_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DataTypeController;
using ::testing::Return;

class SupervisedUserSyncModelTypeControllerTest : public testing::Test {};

TEST_F(SupervisedUserSyncModelTypeControllerTest,
       SupervisedUserMeetsPreconditions) {
  base::MockRepeatingCallback<bool()> is_supervised_user_callback;
  EXPECT_CALL(is_supervised_user_callback, Run()).WillOnce(Return(true));

  SupervisedUserSyncModelTypeController controller(
      syncer::SUPERVISED_USER_SETTINGS, is_supervised_user_callback.Get(),
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr);
  EXPECT_EQ(DataTypeController::PreconditionState::kPreconditionsMet,
            controller.GetPreconditionState());
}

TEST_F(SupervisedUserSyncModelTypeControllerTest,
       NonSupervisedUserDoesNotMeetPreconditions) {
  base::MockRepeatingCallback<bool()> is_supervised_user_callback;
  EXPECT_CALL(is_supervised_user_callback, Run()).WillOnce(Return(false));

  SupervisedUserSyncModelTypeController controller(
      syncer::SUPERVISED_USER_SETTINGS, is_supervised_user_callback.Get(),
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr);
  EXPECT_EQ(DataTypeController::PreconditionState::kMustStopAndClearData,
            controller.GetPreconditionState());
}

TEST_F(SupervisedUserSyncModelTypeControllerTest, HasTransportModeDelegate) {
  base::MockRepeatingCallback<bool()> is_supervised_user_callback;
  EXPECT_CALL(is_supervised_user_callback, Run()).Times(0);

  SupervisedUserSyncModelTypeController controller(
      syncer::SUPERVISED_USER_SETTINGS, is_supervised_user_callback.Get(),
      /*dump_stack=*/base::DoNothing(),
      /*store_factory=*/base::DoNothing(),
      /*syncable_service=*/nullptr);
  EXPECT_TRUE(
      controller.GetDelegateForTesting(syncer::SyncMode::kTransportOnly));
}
