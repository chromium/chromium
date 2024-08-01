// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
  const auto data = GenerateValidationData(
      ProtectionLevel::PROTECTION_PATH_VALIDATION, process1);
  ASSERT_TRUE(data.has_value()) << data.error();
  EXPECT_EQ(expected_match, SUCCEEDED(ValidateData(process2, *data)))
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
  const auto data =
      GenerateValidationData(ProtectionLevel::PROTECTION_NONE, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();
  ASSERT_HRESULT_SUCCEEDED(ValidateData(my_process, *data));
}

TEST_F(CallerValidationTest, PathValidationTest) {
  const auto my_process = base::Process::Current();
  const auto data = GenerateValidationData(
      ProtectionLevel::PROTECTION_PATH_VALIDATION, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();
  ASSERT_HRESULT_SUCCEEDED(ValidateData(my_process, *data));
}

TEST_F(CallerValidationTest, PathValidationOldDataTest) {
  // Test old format validation data.
  const std::vector<uint8_t> data = {'P', 'A', 'T', 'H'};
  const auto result = ValidateData(base::Process::Current(), data);
  ASSERT_HRESULT_FAILED(result);
  ASSERT_EQ(result, E_INVALIDARG);
}

TEST_F(CallerValidationTest, DeprecatedPathValidationTest) {
  const auto data =
      GenerateValidationData(ProtectionLevel::PROTECTION_PATH_VALIDATION_OLD,
                             base::Process::Current());

  ASSERT_FALSE(data.has_value());
  EXPECT_EQ(data.error(), Elevator::kErrorUnsupportedProtectionLevel);
}

TEST_F(CallerValidationTest, BackwardsCompatiblePathDataTest) {
  auto data = GenerateValidationData(
      ProtectionLevel::PROTECTION_PATH_VALIDATION, base::Process::Current());
  ASSERT_TRUE(data.has_value());
  ASSERT_EQ((*data)[0], ProtectionLevel::PROTECTION_PATH_VALIDATION);
  // Simulate a client that has previously generated path validation data but
  // with the old validation type (0x01). This is compatible with the new data
  // type (0x02).
  (*data)[0] = ProtectionLevel::PROTECTION_PATH_VALIDATION_OLD;
  const auto result = ValidateData(base::Process::Current(), *data);
  ASSERT_HRESULT_SUCCEEDED(result);
}

TEST_F(CallerValidationTest, PathValidationTestFail) {
  const auto my_process = base::Process::Current();
  const auto data = GenerateValidationData(
      ProtectionLevel::PROTECTION_PATH_VALIDATION, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();

  auto notepad_process =
      base::LaunchProcess(L"calc.exe", base::LaunchOptions());
  ASSERT_TRUE(notepad_process.IsRunning());

  const HRESULT res = ValidateData(notepad_process, *data);
  ASSERT_HRESULT_FAILED(res);
  ASSERT_EQ(res, Elevator::kValidationDidNotPass);
  ASSERT_TRUE(notepad_process.Terminate(0, true));
}

TEST_F(CallerValidationTest, PathValidationTestOtherProcess) {
  base::expected<std::vector<uint8_t>, HRESULT> data;

  // Start two separate notepad processes to validate that path validation only
  // cares about the process path and not the process itself.
  {
    auto notepad_process =
        base::LaunchProcess(L"calc.exe", base::LaunchOptions());
    ASSERT_TRUE(notepad_process.IsRunning());

    data = GenerateValidationData(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                                  notepad_process);
    ASSERT_TRUE(notepad_process.Terminate(0, true));
  }

  ASSERT_TRUE(data.has_value()) << data.error();

  {
    auto notepad_process =
        base::LaunchProcess(L"calc.exe", base::LaunchOptions());
    ASSERT_TRUE(notepad_process.IsRunning());

    ASSERT_HRESULT_SUCCEEDED(ValidateData(notepad_process, *data));
    ASSERT_TRUE(notepad_process.Terminate(0, true));
  }
}

TEST_F(CallerValidationTest, NoneValidationTestOtherProcess) {
  const auto my_process = base::Process::Current();
  const auto data =
      GenerateValidationData(ProtectionLevel::PROTECTION_NONE, my_process);
  ASSERT_TRUE(data.has_value()) << data.error();

  auto notepad_process =
      base::LaunchProcess(L"calc.exe", base::LaunchOptions());
  ASSERT_TRUE(notepad_process.IsRunning());

  // None validation should not care if the process is different.
  ASSERT_HRESULT_SUCCEEDED(ValidateData(notepad_process, *data));
  ASSERT_TRUE(notepad_process.Terminate(0, true));
}

// tempdir
// |__ app1.exe
// |
// |__ Application
// |   |__ app2.exe
// |   |__ app3.exe
// |   |__ Temp
// |   |   |__ app7.exe
// |   |
// |   |__ 1.2.3.4
// |       |__ app10.exe
// |
// |__ Temp
// |   |__ app4.exe
// |
// |__ Blah
// |   |__ app5.exe
// |   |__ app6.exe
// |
// |__ Program Files
// |   |__ app8.exe
// |
// |__ Program Files (x86)
// |   |__ app9.exe

TEST_F(CallerValidationTest, PathValidationFuzzyPathMatch) {
  // Build the paths.
  // the temp dir must not end with the 'scoped_dir' dir or else it will be
  // removed by the trim path function, so place the test binaries into a subdir
  // of the temp dir.
  const auto temp_dir = temp_dir_.GetPath().AppendASCII("testdir");
  const auto app1_path = temp_dir.AppendASCII("app1.exe");
  const auto app2_path =
      temp_dir.AppendASCII("Application").AppendASCII("app2.exe");
  const auto app3_path =
      temp_dir.AppendASCII("Application").AppendASCII("app3.exe");
  const auto app4_path = temp_dir.AppendASCII("Temp").AppendASCII("app4.exe");
  const auto app5_path = temp_dir.AppendASCII("Blah").AppendASCII("app5.exe");
  const auto app6_path = temp_dir.AppendASCII("Blah").AppendASCII("app6.exe");
  const auto app7_path = temp_dir.AppendASCII("Application")
                             .AppendASCII("Temp")
                             .AppendASCII("app7.exe");
  const auto app8_path =
      temp_dir.AppendASCII("Program Files").AppendASCII("app8.exe");
  const auto app9_path =
      temp_dir.AppendASCII("Program Files (x86)").AppendASCII("app9.exe");
  const auto app10_path = temp_dir.AppendASCII("Application")
                              .AppendASCII("1.2.3.4")
                              .AppendASCII("app10.exe");

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
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app8_path, app9_path, /*expected_match=*/true));
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app1_path, app8_path, /*expected_match=*/false));
  // Verify app in version dir normalizes to the parent directory.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app2_path, app10_path, /*expected_match=*/true));
  // Verify app in version dir does not match an unrelated directory.
  ASSERT_NO_FATAL_FAILURE(
      VerifyValidationResult(app5_path, app10_path, /*expected_match=*/false));
}

// To run this locally, copy the elevation_service_unittests binary to a
// network drive (e.g. X:) and run it using:
// X:\elevation_service_unittests.exe
// --gtest_filter=CallerValidationTest.PathValidationNetwork
// --gtest_also_run_disabled_tests.
TEST_F(CallerValidationTest, DISABLED_PathValidationNetwork) {
  const auto data = GenerateValidationData(
      ProtectionLevel::PROTECTION_PATH_VALIDATION, base::Process::Current());
  EXPECT_FALSE(data.has_value());
  EXPECT_EQ(data.error(), Elevator::kErrorUnsupportedFilePath);
}

TEST_F(CallerValidationTest, TrimProcessPath) {
  struct TestData {
    base::FilePath::StringPieceType input;
    base::FilePath::StringPieceType expected;
  } cases[] = {
      {L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome"},
      {L"C:\\Program Files\\Google\\Chrome\\Temp\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome"},
      {L"C:\\Program Files (x86)\\Google\\Chrome\\Temp\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome"},
      {L"C:\\Program Files (x86)\\Google\\Chrome\\Blah\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome\\Blah"},
      {L"C:\\Dir\\app.exe", L"C:\\Dir"},
      {L"C:\\Dir\\", L"C:\\Dir"},
      {L"C:\\Dir", L"C:\\Dir"},
      {L"C:\\Program Files "
       L"(x86)\\Google\\Chrome\\Temp\\scoped_dir11452_73964817\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome"},
      {L"C:\\Program Files "
       L"(x86)\\Google\\Chrome\\scoped_dir11452_73964817\\Temp\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome\\scoped_dir11452_73964817"},
      {L"C:\\Program Files (x86)\\Google\\scoped_dir1\\Chrome\\chrome.exe",
       L"C:\\Program Files\\Google\\scoped_dir1\\Chrome"},
      {L"C:\\Temp\\Program Files "
       L"(x86)\\Google\\scoped_dir1\\Chrome\\chrome.exe",
       L"C:\\Temp\\Program Files\\Google\\scoped_dir1\\Chrome"},
      {L"C:\\scoped_dir1234\\Program Files "
       L"(x86)\\Google\\scoped_dir1234\\Chrome\\chrome.exe",
       L"C:\\scoped_dir1234\\Program Files\\Google\\scoped_dir1234\\Chrome"},
      {L"C:\\Program Files\\Google\\Chrome\\Application\\1.2.3.4\\chrome.exe",
       L"C:\\Program Files\\Google\\Chrome"},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    base::FilePath input(cases[i].input);
    auto output = MaybeTrimProcessPathForTesting(input);
    EXPECT_EQ(output.value(), cases[i].expected);
  }
}

}  // namespace elevation_service
