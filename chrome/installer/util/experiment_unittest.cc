// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment.h"

#include <cmath>

#include "base/time/time.h"
#include "chrome/installer/util/experiment_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

TEST(ExperimentTest, TestInitializeFromMetrics) {
  Experiment experiment;
  ExperimentMetrics metrics_with_group;
  metrics_with_group.state = ExperimentMetrics::kGroupAssigned;
  metrics_with_group.group = 5;
  experiment.InitializeFromMetrics(metrics_with_group);
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.state());
  EXPECT_EQ(5, experiment.group());
  experiment.SetInactiveDays(2);
  // Test initializing from empty ExperimentMetrics.
  experiment.InitializeFromMetrics(ExperimentMetrics());
  EXPECT_EQ(ExperimentMetrics::kUninitialized, experiment.state());
  EXPECT_EQ(0, experiment.group());
  EXPECT_EQ(0, experiment.inactive_days());
}

TEST(ExperimentTest, TestSetState) {
  Experiment experiment;
  EXPECT_EQ(ExperimentMetrics::kUninitialized, experiment.state());
  EXPECT_EQ(ExperimentMetrics::kUninitialized, experiment.metrics().state);
  experiment.SetState(ExperimentMetrics::kGroupAssigned);
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.metrics().state);
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.state());
}

TEST(ExperimentTest, TestAssignGroup) {
  Experiment experiment;
  experiment.AssignGroup(5);
  EXPECT_EQ(5, experiment.metrics().group);
  EXPECT_EQ(5, experiment.group());
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.metrics().state);
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.state());
}

TEST(ExperimentTest, TestSetInactiveDays) {
  Experiment experiment;
  experiment.AssignGroup(5);

  experiment.SetInactiveDays(0);
  EXPECT_EQ(0, experiment.metrics().last_used_bucket);
  EXPECT_EQ(0, experiment.inactive_days());

  experiment.SetInactiveDays(28);
  EXPECT_EQ(57, experiment.metrics().last_used_bucket);
  EXPECT_EQ(28, experiment.inactive_days());

  experiment.SetInactiveDays(35);
  EXPECT_EQ(61, experiment.metrics().last_used_bucket);
  EXPECT_EQ(35, experiment.inactive_days());

  experiment.SetInactiveDays(60);
  EXPECT_EQ(70, experiment.metrics().last_used_bucket);
  EXPECT_EQ(60, experiment.inactive_days());

  experiment.SetInactiveDays(ExperimentMetrics::kMaxLastUsed);
  EXPECT_EQ(127, experiment.metrics().last_used_bucket);
  EXPECT_EQ(ExperimentMetrics::kMaxLastUsed, experiment.inactive_days());

  experiment.SetInactiveDays(2 * ExperimentMetrics::kMaxLastUsed);
  EXPECT_EQ(127, experiment.metrics().last_used_bucket);
  EXPECT_EQ(2 * ExperimentMetrics::kMaxLastUsed, experiment.inactive_days());
}

TEST(ExperimentTest, TestSetDisplayTime) {
  Experiment experiment;
  experiment.AssignGroup(5);
  base::Time zero_day =
      base::Time::FromDoubleT(ExperimentMetrics::kExperimentStartSeconds);
  experiment.SetDisplayTime(zero_day);
  EXPECT_EQ(0, experiment.metrics().first_toast_offset_days);
  EXPECT_EQ(zero_day, experiment.first_display_time());
  EXPECT_EQ(zero_day, experiment.latest_display_time());

  base::Time one_day = zero_day + base::Days(1);
  experiment.SetDisplayTime(one_day);
  EXPECT_EQ(1, experiment.metrics().first_toast_offset_days);
  EXPECT_EQ(one_day, experiment.first_display_time());
  EXPECT_EQ(one_day, experiment.latest_display_time());

  // Test that calling SetDisplayTime again will not reset
  // first_toast_offset_days.
  base::Time two_day = zero_day + base::Days(2);
  experiment.SetDisplayTime(two_day);
  EXPECT_EQ(1, experiment.metrics().first_toast_offset_days);
  EXPECT_EQ(one_day, experiment.first_display_time());
  EXPECT_EQ(two_day, experiment.latest_display_time());

  // Test for maximum value.
  Experiment new_experiment;
  new_experiment.AssignGroup(5);
  base::Time max_day =
      zero_day + base::Days(ExperimentMetrics::kMaxFirstToastOffsetDays);
  new_experiment.SetDisplayTime(max_day);
  EXPECT_EQ(ExperimentMetrics::kMaxFirstToastOffsetDays,
            new_experiment.metrics().first_toast_offset_days);
  EXPECT_EQ(max_day, new_experiment.first_display_time());
  EXPECT_EQ(max_day, new_experiment.latest_display_time());

  // Test setting toast_hour. Since it depends on local time it is
  // tested by setting toast hour for two consecutive hours and verifying the
  // difference is 1.
  base::Time two_day_one_hour = two_day + base::Hours(1);
  Experiment hour_experiment;
  hour_experiment.AssignGroup(5);
  hour_experiment.SetDisplayTime(two_day_one_hour);
  int diff_hour =
      hour_experiment.metrics().toast_hour - experiment.metrics().toast_hour;
  EXPECT_EQ(1, diff_hour);
}

TEST(ExperimentTest, TestSetUserSessionUptime) {
  Experiment experiment;
  experiment.AssignGroup(5);
  experiment.SetUserSessionUptime(base::Minutes(0));
  EXPECT_EQ(0, experiment.metrics().session_length_bucket);
  EXPECT_EQ(0, experiment.user_session_uptime().InMinutes());

  experiment.SetUserSessionUptime(base::Minutes(60));
  EXPECT_EQ(24, experiment.metrics().session_length_bucket);
  EXPECT_EQ(60, experiment.user_session_uptime().InMinutes());

  experiment.SetUserSessionUptime(base::Minutes(60 * 24));
  EXPECT_EQ(43, experiment.metrics().session_length_bucket);
  EXPECT_EQ(60 * 24, experiment.user_session_uptime().InMinutes());

  experiment.SetUserSessionUptime(
      base::Minutes(ExperimentMetrics::kMaxSessionLength));
  EXPECT_EQ(63, experiment.metrics().session_length_bucket);
  EXPECT_EQ(ExperimentMetrics::kMaxSessionLength,
            experiment.user_session_uptime().InMinutes());

  experiment.SetUserSessionUptime(
      base::Minutes(2 * ExperimentMetrics::kMaxSessionLength));
  EXPECT_EQ(63, experiment.metrics().session_length_bucket);
  EXPECT_EQ(2 * ExperimentMetrics::kMaxSessionLength,
            experiment.user_session_uptime().InMinutes());
}

TEST(ExperimentTest, TestSetActionDelay) {
  Experiment experiment;
  experiment.AssignGroup(5);
  experiment.SetActionDelay(base::Seconds(0));
  EXPECT_EQ(0, experiment.metrics().action_delay_bucket);
  EXPECT_EQ(0, experiment.user_session_uptime().InSeconds());

  experiment.SetActionDelay(base::Seconds(60));
  EXPECT_EQ(10, experiment.metrics().action_delay_bucket);
  EXPECT_EQ(60, experiment.action_delay().InSeconds());

  experiment.SetActionDelay(base::Seconds(60 * 60));
  EXPECT_EQ(19, experiment.metrics().action_delay_bucket);
  EXPECT_EQ(60 * 60, experiment.action_delay().InSeconds());

  experiment.SetActionDelay(base::Seconds(ExperimentMetrics::kMaxActionDelay));
  EXPECT_EQ(31, experiment.metrics().action_delay_bucket);
  EXPECT_EQ(ExperimentMetrics::kMaxActionDelay,
            experiment.action_delay().InSeconds());

  experiment.SetActionDelay(
      base::Seconds(2 * ExperimentMetrics::kMaxActionDelay));
  EXPECT_EQ(31, experiment.metrics().action_delay_bucket);
  EXPECT_EQ(2 * ExperimentMetrics::kMaxActionDelay,
            experiment.action_delay().InSeconds());
}

TEST(ExperimentTest, TestAllSetters) {
  Experiment experiment;
  experiment.AssignGroup(5);
  experiment.SetState(ExperimentMetrics::kGroupAssigned);
  experiment.SetToastLocation(ExperimentMetrics::kOverTaskbarPin);
  experiment.SetInactiveDays(1621);
  experiment.SetToastCount(1);
  base::Time test_display_time =
      (base::Time::UnixEpoch() +
       base::Seconds(ExperimentMetrics::kExperimentStartSeconds) +
       base::Days(30) + base::Hours(3));
  experiment.SetDisplayTime(test_display_time);
  experiment.SetUserSessionUptime(base::Minutes(3962));
  experiment.SetActionDelay(base::Seconds(32875));

  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.state());
  EXPECT_EQ(5, experiment.group());
  EXPECT_EQ(ExperimentMetrics::kOverTaskbarPin, experiment.toast_location());
  EXPECT_EQ(1621, experiment.inactive_days());
  EXPECT_EQ(1, experiment.toast_count());
  EXPECT_EQ(test_display_time, experiment.first_display_time());
  EXPECT_EQ(test_display_time, experiment.latest_display_time());
  EXPECT_EQ(base::Minutes(3962), experiment.user_session_uptime());
  EXPECT_EQ(base::Seconds(32875), experiment.action_delay());

  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, experiment.metrics().state);
  EXPECT_EQ(5, experiment.metrics().group);
  EXPECT_EQ(ExperimentMetrics::kOverTaskbarPin,
            experiment.metrics().toast_location);
  EXPECT_EQ(1, experiment.metrics().toast_count);
  EXPECT_EQ(30, experiment.metrics().first_toast_offset_days);
  EXPECT_EQ(125, experiment.metrics().last_used_bucket);
  EXPECT_EQ(24, experiment.metrics().action_delay_bucket);
  EXPECT_EQ(49, experiment.metrics().session_length_bucket);
}

}  // namespace installer
