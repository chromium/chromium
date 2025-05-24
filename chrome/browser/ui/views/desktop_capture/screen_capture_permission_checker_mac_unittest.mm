// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/screen_capture_permission_checker_mac.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TaskEnvironment;
using ::testing::Return;

class ScreenCapturePermissionCheckerMacTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Default-enabled for 14.4+. Manually enabled for 14.3- for coverage.
    scoped_feature_list_.InitAndEnableFeature(
        kDesktopCapturePermissionCheckerPreMacos14_4);
  }

  // `permission_checker_` needs to be initialized after `EXPECT_CALL`
  void InitPermissionChecker() {
    permission_checker_ = std::make_unique<ScreenCapturePermissionCheckerMac>(
        on_permission_update_callback_.Get(),
        is_screen_capture_allowed_callback_.Get());
  }

 protected:
  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockRepeatingCallback<void(bool)> on_permission_update_callback_;
  base::MockRepeatingCallback<bool()> is_screen_capture_allowed_callback_;
  std::unique_ptr<ScreenCapturePermissionCheckerMac> permission_checker_;
};

TEST_F(ScreenCapturePermissionCheckerMacTest, TestWithoutPermission) {
  EXPECT_CALL(is_screen_capture_allowed_callback_, Run).WillOnce(Return(false));
  EXPECT_CALL(on_permission_update_callback_, Run(false)).Times(1);
  InitPermissionChecker();

  task_environment_.RunUntilIdle();
}

TEST_F(ScreenCapturePermissionCheckerMacTest, TestWithPermission) {
  EXPECT_CALL(is_screen_capture_allowed_callback_, Run).WillOnce(Return(true));
  EXPECT_CALL(on_permission_update_callback_, Run(true)).Times(1);
  InitPermissionChecker();

  task_environment_.RunUntilIdle();
}

TEST_F(ScreenCapturePermissionCheckerMacTest, TestRepeatedCalls) {
  EXPECT_CALL(is_screen_capture_allowed_callback_, Run)
      .WillOnce(Return(false))
      .WillOnce(Return(false));
  EXPECT_CALL(on_permission_update_callback_, Run).Times(2);
  InitPermissionChecker();

  task_environment_.RunUntilIdle();
  task_environment_.AdvanceClock(base::Seconds(1));
  task_environment_.RunUntilIdle();
}
