// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/shutdown_watchdog.h"

#include <windows.h>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

int ReturnAnInt(int i) {
  return i;
}

// Test that Alarm() exits the process with the exit code returned from the
// callback function provided to Watchdog's constructor.
TEST(ShutdownWatchdogTestDeathTest, Alarm) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_EXIT(
      {
        chrome_cleaner::ShutdownWatchdog watchdog(
            base::Milliseconds(1), base::BindOnce(ReturnAnInt, 47));
        watchdog.Arm();
        // We won't actually sleep this long since the watchdog will terminate
        // the process much sooner.
        ::Sleep(1000);
      },
      ::testing::ExitedWithCode(47),
      "Shutdown watchdog triggered, exiting with code 47");

  EXPECT_EXIT(
      {
        chrome_cleaner::ShutdownWatchdog watchdog(
            base::Milliseconds(1), base::BindOnce(ReturnAnInt, 30));
        watchdog.Arm();
        // We won't actually sleep this long since the watchdog will terminate
        // the process much sooner.
        ::Sleep(1000);
      },
      ::testing::ExitedWithCode(30),
      "Shutdown watchdog triggered, exiting with code 30");
}
