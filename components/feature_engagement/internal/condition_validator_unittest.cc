// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/condition_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

TEST(ConditionValidatorResultTest, TestAllOK) {
  EXPECT_TRUE(ConditionValidator::Result(true).NoErrors());
}

TEST(ConditionValidatorResultTest, TestAllErrors) {
  EXPECT_FALSE(ConditionValidator::Result(false).NoErrors());
}

TEST(ConditionValidatorResultTest, TestModelNotReady) {
  ConditionValidator::Result result(true);
  result.event_model_ready_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestCurrentlyShowing) {
  ConditionValidator::Result result(true);
  result.currently_showing_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestFeatureEnabled) {
  ConditionValidator::Result result(true);
  result.feature_enabled_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestInvalidConfig) {
  ConditionValidator::Result result(true);
  result.config_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestUsedFailed) {
  ConditionValidator::Result result(true);
  result.used_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestTriggerFailed) {
  ConditionValidator::Result result(true);
  result.trigger_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestPreconditionsFailed) {
  ConditionValidator::Result result(true);
  result.preconditions_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestSessionRateFailed) {
  ConditionValidator::Result result(true);
  result.session_rate_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestAvailabilityModelNotReady) {
  ConditionValidator::Result result(true);
  result.availability_model_ready_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestAvailabilityFailed) {
  ConditionValidator::Result result(true);
  result.availability_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestDisplayLockFailed) {
  ConditionValidator::Result result(true);
  result.display_lock_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestPriorityNotificationFailed) {
  ConditionValidator::Result result(true);
  result.priority_notification_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestMultipleErrors) {
  ConditionValidator::Result result(true);
  result.preconditions_ok = false;
  result.session_rate_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestSnoozeExpirationFailed) {
  ConditionValidator::Result result(true);
  result.snooze_expiration_ok = false;
  EXPECT_FALSE(result.NoErrors());
}

TEST(ConditionValidatorResultTest, TestShouldShowSnoozeFailed) {
  ConditionValidator::Result result(true);
  result.should_show_snooze = false;
  EXPECT_TRUE(result.NoErrors());
}

}  // namespace feature_engagement
