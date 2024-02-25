// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/upstart.h"

#include "testing/gtest/include/gtest/gtest.h"

// This file contains unit tests for the "upstart" test support helper.

namespace upstart {

// For gtest comparisons.
bool operator==(const JobStatus& a, const JobStatus& b) {
  return std::tie(a.is_valid, a.goal, a.state, a.pid) ==
         std::tie(b.is_valid, b.goal, b.state, b.pid);
}

TEST(UpstartTest, ParseStatus) {
  JobStatus result = internal::ParseStatus("foo", "");
  EXPECT_FALSE(result.is_valid);

  // Normal stopped one.
  result = internal::ParseStatus("boot-splash", " boot-splash stop/waiting\n");
  JobStatus boot_splash{true, Goal::kStop, State::kWaiting, 0};
  EXPECT_EQ(result, boot_splash);

  // Job name doesn't match.
  result = internal::ParseStatus("different-one", "boot-splash stop/waiting");
  EXPECT_FALSE(result.is_valid);

  // Running with a PID.
  result =
      internal::ParseStatus("powerd", "powerd start/running, process 9398\n");
  JobStatus powerd{true, Goal::kStart, State::kRunning, 9398};
  EXPECT_EQ(result, powerd);

  // Multi-line input just gives the first one.
  result = internal::ParseStatus(
      "ureadahead",
      "ureadahead stop/pre-stop, process 227\npre-stop process 5579\n");
  JobStatus ureadahead{true, Goal::kStop, State::kPreStop, 227};
  EXPECT_EQ(result, ureadahead);

  // Instance status.
  result = internal::ParseStatus(
      "ml-service", "ml-service (mojo_service) start/running, process 6820");
  JobStatus mlservice{true, Goal::kStart, State::kRunning, 6820};
  EXPECT_EQ(result, mlservice);

  // Tmpfiles annotation.
  result = internal::ParseStatus("ui",
                                 "ui start/tmpfiles, (tmpfiles) process 19419");
  JobStatus ui{true, Goal::kStart, State::kTmpfiles, 19419};
  EXPECT_EQ(result, ui);
}

TEST(UpstartTest, GetJobStatus) {
  // Random nonexistent job.
  JobStatus result = GetJobStatus("nonexistent-job");
  EXPECT_FALSE(result.is_valid);

  // The "dbus" one should always be running.
  result = GetJobStatus("dbus");
  EXPECT_TRUE(result.is_valid);
  EXPECT_EQ(result.goal, Goal::kStart);
  EXPECT_EQ(result.state, State::kRunning);
  EXPECT_GT(result.pid, 0);
}

TEST(UpstartTest, JobExists) {
  EXPECT_FALSE(JobExists("nonexistent-job"));
  EXPECT_TRUE(JobExists("dbus"));
}

TEST(UpstartTest, WaitForJobStatus) {
  // This is hard to test without making a lot of assumptions or messing with
  // the system. Instead, we just validate some simple cases that don't change
  // anything.
  EXPECT_FALSE(WaitForJobStatus("nonexistent-job", Goal::kStart,
                                State::kRunning, WrongGoalPolicy::kReject));

  // Wait until the current state should always succeed.
  JobStatus dbus = GetJobStatus("dbus");
  EXPECT_TRUE(WaitForJobStatus("dbus", dbus.goal, dbus.state,
                               WrongGoalPolicy::kReject));

  // Wait until the opposite goal should immediately fail.
  Goal opposite_dbus_goal =
      dbus.goal == Goal::kStart ? Goal::kStop : Goal::kStart;
  EXPECT_FALSE(WaitForJobStatus("dbus", opposite_dbus_goal, State::kRunning,
                                WrongGoalPolicy::kReject));

  // Waiting for the opposite goal with "tolerate" option should timeout which
  // we determine by making sure it takes longer than our specified timeout to
  // run.
  base::TimeDelta timeout = base::Milliseconds(100);
  base::TimeTicks begin = base::TimeTicks::Now();
  EXPECT_FALSE(internal::WaitForJobStatusWithTimeout(
      "dbus", opposite_dbus_goal, State::kRunning, WrongGoalPolicy::kTolerate,
      timeout));
  base::TimeTicks end = base::TimeTicks::Now();
  EXPECT_GT(end - begin, timeout);
}

}  // namespace upstart
