// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/elevation_service/caller_validation.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/win/scoped_process_information.h"
#include "base/win/startup_information.h"
#include "chrome/elevation_service/elevator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elevation_service {

namespace {

// Starts a suspended process that's located at `path`.
base::Process StartSuspendedFakeProcess(const base::FilePath& path) {
  PROCESS_INFORMATION temp_process_info = {};
  if (!base::PathExists(path)) {
    base::CreateDirectory(path.DirName());
    // Doesn't matter what the executable is, as long as it's an executable.
    base::CopyFile(base::PathService::CheckedGet(base::FILE_EXE), path);
  }

  base::win::StartupInformation startup_info;
  std::wstring writable_cmd_line = path.value();
  if (::CreateProcess(nullptr, writable_cmd_line.data(), nullptr, nullptr,
                      /*bInheritHandles=*/FALSE, CREATE_SUSPENDED, nullptr,
                      nullptr, startup_info.startup_info(),
                      &temp_process_info)) {
    base::win::ScopedProcessInformation process_info(temp_process_info);
    return base::Process(process_info.TakeProcessHandle());
  }
  return base::Process();
}

void VerifyValidationResult(const base::FilePath& path1,
                            const base::FilePath& path2,
                            bool expected_match) {
  auto process1 = StartSuspendedFakeProcess(path1);
  ASSERT_TRUE(process1.IsRunning());
  auto process2 = StartSuspendedFakeProcess(path2);
  ASSERT_TRUE(process2.IsRunning());
  const auto data =
      GenerateValidationData(ProtectionLevel::PATH_VALIDATION, process1);
  ASSERT_TRUE(data.has_value()) << data.error();
  EXPECT_EQ(expected_match, ValidateData(process2, *data))
      << path1 << " vs. " << path2;
  process1.Terminate(0, /*wait=*/true);
  process2.Terminate(0, /*wait=*/true);
}

}  // namespace

class CallerValidationTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(CallerValidationTest, NoneValidationTest) {
  const auto my_process = base::Process::Current();
  const auto data = GenerateValidationData(ProtectionLevel::NONE, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();
  ASSERT_TRUE(ValidateData(my_process, *data));
}

TEST_F(CallerValidationTest, PathValidationTest) {
  const auto my_process = base::Process::Current();
  const auto data =
      GenerateValidationData(ProtectionLevel::PATH_VALIDATION, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();
  ASSERT_TRUE(ValidateData(my_process, *data));
}

TEST_F(CallerValidationTest, PathValidationTestFail) {
  const auto my_process = base::Process::Current();
  const auto data =
      GenerateValidationData(ProtectionLevel::PATH_VALIDATION, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();

  auto notepad_process =
      base::LaunchProcess(L"calc.exe", base::LaunchOptions());
  ASSERT_TRUE(notepad_process.IsRunning());

  ASSERT_FALSE(ValidateData(notepad_process, *data));
  ASSERT_TRUE(notepad_process.Terminate(0, true));
}

TEST_F(CallerValidationTest, PathValidationTestOtherProcess) {
  base::expected<std::string, HRESULT> data;

  // Start two separate notepad processes to validate that path validation only
  // cares about the process path and not the process itself.
  {
    auto notepad_process =
        base::LaunchProcess(L"calc.exe", base::LaunchOptions());
    ASSERT_TRUE(notepad_process.IsRunning());

    data = GenerateValidationData(ProtectionLevel::PATH_VALIDATION,
                                  notepad_process);
    ASSERT_TRUE(notepad_process.Terminate(0, true));
  }

  ASSERT_TRUE(data.has_value()) << data.error();

  {
    auto notepad_process =
        base::LaunchProcess(L"calc.exe", base::LaunchOptions());
    ASSERT_TRUE(notepad_process.IsRunning());

    ASSERT_TRUE(ValidateData(notepad_process, *data));
    ASSERT_TRUE(notepad_process.Terminate(0, true));
  }
}

TEST_F(CallerValidationTest, NoneValidationTestOtherProcess) {
  const auto my_process = base::Process::Current();
  const auto data = GenerateValidationData(ProtectionLevel::NONE, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();

  auto notepad_process =
      base::LaunchProcess(L"calc.exe", base::LaunchOptions());
  ASSERT_TRUE(notepad_process.IsRunning());

  // None validation should not care if the process is different.
  ASSERT_TRUE(ValidateData(notepad_process, *data));
  ASSERT_TRUE(notepad_process.Terminate(0, true));
}

// tempdir
// |__ app1.exe
// |
// |__ Application
// |   |__ app2.exe
// |   |__ app3.exe
// |   |__ Temp
// |       |__ app7.exe
// |
// |__ Temp
// |   |__ app4.exe
// |
// |__ Blah
// |   |__ app5.exe
// |   |__ app6.exe
TEST_F(CallerValidationTest, PathValidationFuzzyPathMatch) {
  // Build the paths.
  const auto app1_path = temp_dir_.GetPath().AppendASCII("app1.exe");
  const auto app2_path =
      temp_dir_.GetPath().AppendASCII("Application").AppendASCII("app2.exe");
  const auto app3_path =
      temp_dir_.GetPath().AppendASCII("Application").AppendASCII("app3.exe");
  const auto app4_path =
      temp_dir_.GetPath().AppendASCII("Temp").AppendASCII("app4.exe");
  const auto app5_path =
      temp_dir_.GetPath().AppendASCII("Blah").AppendASCII("app5.exe");
  const auto app6_path =
      temp_dir_.GetPath().AppendASCII("Blah").AppendASCII("app6.exe");
  const auto app7_path = temp_dir_.GetPath()
                             .AppendASCII("Application")
                             .AppendASCII("Temp")
                             .AppendASCII("app7.exe");

  // Should ignore 'Temp' and 'Application' for matches.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app1_path, app2_path, /*expected_match=*/true));
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app1_path, app3_path, /*expected_match=*/true));
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app1_path, app4_path, /*expected_match=*/true));
  // Invalid subdir 'Blah'.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app1_path, app5_path, /*expected_match=*/false));
  // Case for rename of chrome.exe to new_chrome.exe during install.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app2_path, app3_path, /*expected_match=*/true));
  // 'Temp' and 'Application' should both normalize to each other.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app2_path, app4_path, /*expected_match=*/true));
  // Invalid subdir 'Blah'.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app2_path, app5_path, /*expected_match=*/false));
  // Case for temp path of chrome exe during install.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app4_path, app2_path, /*expected_match=*/true));
  // Verify app in unusual directory still validates correctly.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app5_path, app6_path, /*expected_match=*/true));
  // Verify Temp/Application should only be removed once and not multiple times.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app7_path, app3_path, /*expected_match=*/false));
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app7_path, app1_path, /*expected_match=*/false));
}

// To run this locally, copy the elevation_service_unittests binary to a
// network drive (e.g. X:) and run it using:
// X:\elevation_service_unittests.exe
// --gtest_filter=CallerValidationTest.PathValidationNetwork
// --gtest_also_run_disabled_tests.
TEST_F(CallerValidationTest, DISABLED_PathValidationNetwork) {
  const auto data = GenerateValidationData(ProtectionLevel::PATH_VALIDATION,
                                           base::Process::Current());
  EXPECT_FALSE(data.has_value());
  EXPECT_EQ(data.error(), Elevator::kErrorUnsupportedFilePath);
}

}  // namespace elevation_service
