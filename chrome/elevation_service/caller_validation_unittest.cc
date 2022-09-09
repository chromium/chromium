// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/caller_validation.h"

#include "base/process/launch.h"
#include "base/process/process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elevation_service {

using CallerValidationTest = ::testing::Test;

TEST_F(CallerValidationTest, NoneValidationTest) {
  const auto my_process = base::Process::Current();
  const std::string data =
      GenerateValidationData(ProtectionLevel::NONE, my_process);
  ASSERT_FALSE(data.empty());
  ASSERT_TRUE(ValidateData(my_process, data));
}

TEST_F(CallerValidationTest, PathValidationTest) {
  const auto my_process = base::Process::Current();
  const std::string data =
      GenerateValidationData(ProtectionLevel::PATH_VALIDATION, my_process);
  ASSERT_FALSE(data.empty());
  ASSERT_TRUE(ValidateData(my_process, data));
}

TEST_F(CallerValidationTest, PathValidationTestFail) {
  const auto my_process = base::Process::Current();
  const std::string data =
      GenerateValidationData(ProtectionLevel::PATH_VALIDATION, my_process);
  ASSERT_FALSE(data.empty());

  auto notepad_process =
      base::LaunchProcess(L"notepad.exe", base::LaunchOptions());
  ASSERT_TRUE(notepad_process.IsRunning());

  ASSERT_FALSE(ValidateData(notepad_process, data));
  ASSERT_TRUE(notepad_process.Terminate(0, true));
}

TEST_F(CallerValidationTest, PathValidationTestOtherProcess) {
  std::string data;

  // Start two separate notepad processes to validate that path validation only
  // cares about the process path and not the process itself.
  {
    auto notepad_process =
        base::LaunchProcess(L"notepad.exe", base::LaunchOptions());
    ASSERT_TRUE(notepad_process.IsRunning());

    data = GenerateValidationData(ProtectionLevel::PATH_VALIDATION,
                                  notepad_process);
    ASSERT_TRUE(notepad_process.Terminate(0, true));
  }

  ASSERT_FALSE(data.empty());

  {
    auto notepad_process =
        base::LaunchProcess(L"notepad.exe", base::LaunchOptions());
    ASSERT_TRUE(notepad_process.IsRunning());

    ASSERT_TRUE(ValidateData(notepad_process, data));
    ASSERT_TRUE(notepad_process.Terminate(0, true));
  }
}

TEST_F(CallerValidationTest, NoneValidationTestOtherProcess) {
  const auto my_process = base::Process::Current();
  const std::string data =
      GenerateValidationData(ProtectionLevel::NONE, my_process);
  ASSERT_FALSE(data.empty());

  auto notepad_process =
      base::LaunchProcess(L"notepad.exe", base::LaunchOptions());
  ASSERT_TRUE(notepad_process.IsRunning());

  // None validation should not care if the process is different.
  ASSERT_TRUE(ValidateData(notepad_process, data));
  ASSERT_TRUE(notepad_process.Terminate(0, true));
}

}  // namespace elevation_service
