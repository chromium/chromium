// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/registry_monitor.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class RegistryMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_EQ(test_key_.Create(HKEY_CURRENT_USER,
                               L"Software\\Chromium\\RegistryMonitorTest",
                               KEY_SET_VALUE),
              ERROR_SUCCESS);
  }

  // Returns the test registry key to monitor. |access| may be used to override
  // the default access rights for the key.
  base::win::RegKey GetKeyToMonitor(REGSAM access = KEY_NOTIFY) {
    base::win::RegKey key;
    EXPECT_EQ(key.Open(test_key_.Handle(), L"", access), ERROR_SUCCESS);
    return key;
  }

  registry_util::RegistryOverrideManager registry_override_;
  base::win::RegKey test_key_;
  base::test::TaskEnvironment task_environment_;
};

// Tests that the callback is invoked when the key is invalid.
TEST_F(RegistryMonitorTest, ErrorOnInvalidKey) {
  RegistryMonitor monitor{base::win::RegKey()};
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(true)).WillOnce([&run_loop]() { run_loop.Quit(); });
  monitor.Start(callback.Get());
  run_loop.Run();
}

// Tests that the callback is invoked when the key doesn't have NOTIFY access.
TEST_F(RegistryMonitorTest, ErrorOnWrongPerms) {
  RegistryMonitor monitor(GetKeyToMonitor(KEY_QUERY_VALUE));
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(true)).WillOnce([&run_loop]() { run_loop.Quit(); });
  monitor.Start(callback.Get());
  run_loop.Run();
}

// Tests that the callback is not invoked when nothing happens.
TEST_F(RegistryMonitorTest, NoNoise) {
  RegistryMonitor monitor(GetKeyToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;
  monitor.Start(callback.Get());
  task_environment_.RunUntilIdle();
}

// Tests that the callback is invoked when the key is modified.
TEST_F(RegistryMonitorTest, OneChange) {
  RegistryMonitor monitor(GetKeyToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  monitor.Start(callback.Get());
  test_key_.WriteValue(L"some value", L"hi, mom");
  run_loop.Run();
}

// Tests that the callback is invoked multiple times for multiple modifications.
TEST_F(RegistryMonitorTest, MultipleChanges) {
  RegistryMonitor monitor(GetKeyToMonitor());
  ::testing::StrictMock<base::MockRepeatingCallback<void(bool)>> callback;
  ::testing::InSequence sequence;
  base::RunLoop run_loop;
  base::RunLoop run_loop2;
  EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  EXPECT_CALL(callback, Run(false)).WillOnce([&run_loop2]() {
    run_loop2.Quit();
  });
  monitor.Start(callback.Get());
  test_key_.WriteValue(L"some value", L"hi, mom");
  run_loop.Run();
  test_key_.WriteValue(L"some value", L"hi, dad");
  run_loop2.Run();
}
