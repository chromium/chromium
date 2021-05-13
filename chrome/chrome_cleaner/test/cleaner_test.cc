// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.pb.h"
#include "chrome/chrome_cleaner/logging/proto/reporter_logs.pb.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/test/child_process_logger.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/zip_archiver/sandboxed_zip_archiver.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using chrome_cleaner::Engine;
using chrome_cleaner::ExecutionMode;
using chrome_cleaner::PUPData;
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

const wchar_t kCleanerExecutable[] = L"chrome_cleanup_tool.exe";
const wchar_t kScannerExecutable[] = L"software_reporter_tool.exe";

const wchar_t kProtoExtension[] = L"pb";

// Strings which identify log entries which should be allowed because they fail
// the SanitizePath check, but are covered by one of the cases below:
//
// Case 1: When a prefix is added to a valid path, it shows up as unsanitized
//   even though SanitizePath() may have been used.
//
// Case 2: A registry value contains paths which would ideally be sanitized,
//   but the exact value should be reported to prevent ambiguity. Currently this
//   generally means we just ignore the registry fields in the proto.
const std::vector<std::wstring> kAllowedLogStringsForSanitizationCheck = {
    // TryToExpandPath() in disk_util.cc fits Case 1.
    L"] unable to retrieve file information on non-existing file: \'",

    // IsFilePresentLocally in file_path_sanitization.cc fits Case 1.
    L"isfilepresentlocally failed to get attributes: ",

    // The cleaner/reporter process spawned by this test spawns a sandbox with
    // the test-logging-path flag pointing to a temporary directory.
    L"--test-logging-path=",
};

// Parse to |report| the serialized report dumped by |executable_name|. Return
// true on success.
bool ParseSerializedReport(const wchar_t* const executable_name,
                           chrome_cleaner::ChromeCleanerReport* report) {
  DCHECK(executable_name);
  DCHECK(report);
  base::FilePath cleaner_executable(executable_name);
  base::FilePath exe_file_path =
      chrome_cleaner::PreFetchedPaths::GetInstance()->GetExecutablePath();
  base::FilePath proto_dump_file_path(exe_file_path.DirName().Append(
      cleaner_executable.ReplaceExtension(kProtoExtension)));
  std::string dumped_proto_string;
  return base::ReadFileToString(proto_dump_file_path, &dumped_proto_string) &&
         report->ParseFromString(dumped_proto_string);
}

std::wstring StringToWLower(const std::wstring& source) {
  // TODO(joenotcharles): Investigate moving this into string_util.cc and using
  // it instead of ToLowerASCII() through the whole code base. Case insensitive
  // compares will also need to be changed.
  std::wstring copy = source;
  for (wchar_t& character : copy) {
    character = towlower(character);
  }
  return copy;
}

std::vector<std::wstring> GetUnsanitizedPaths() {
  std::vector<std::wstring> unsanitized_path_strings;
  bool success = true;
  for (const auto& entry : chrome_cleaner::PathKeyToSanitizeString()) {
    int id = entry.first;
    base::FilePath unsanitized_path;
    if (!base::PathService::Get(id, &unsanitized_path)) {
      LOG(ERROR) << "Failed to convert id (" << id << ") in PathService";
      success = false;
      continue;
    }
    std::wstring unsanitized_path_string =
        StringToWLower(unsanitized_path.value());
    unsanitized_path_strings.push_back(unsanitized_path_string);
  }
  CHECK(success);
  return unsanitized_path_strings;
}

bool ContainsAnyOf(const std::wstring& main_string,
                   const std::vector<std::wstring>& substrings) {
  return std::any_of(substrings.begin(), substrings.end(),
                     [&main_string](const std::wstring& path) -> bool {
                       return main_string.find(path) != std::wstring::npos;
                     });
}

template <typename RepeatedTypeWithFileInformation>
void CheckFieldForUnsanitizedPaths(
    const RepeatedTypeWithFileInformation& field,
    const std::string& field_name,
    const std::vector<std::wstring>& unsanitized_path_strings) {
  for (const auto& sub_field : field) {
    std::string utf8_path = sub_field.file_information().path();
    EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_path)),
                               unsanitized_path_strings))
        << "Found unsanitized " << field_name << ":\n>>> " << utf8_path;
  }
}

bool CheckCleanerReportForUnsanitizedPaths(
    const chrome_cleaner::ChromeCleanerReport& report) {
  EXPECT_GT(report.raw_log_line_size(), 0);
  std::vector<std::wstring> unsanitized_path_strings = GetUnsanitizedPaths();
  size_t line_number = 0;
  for (const auto& utf8_line : report.raw_log_line()) {
    ++line_number;
    if (!ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_line)),
                       kAllowedLogStringsForSanitizationCheck)) {
      EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_line)),
                                 unsanitized_path_strings))
          << "Found unsanitized logs line (" << line_number << "):\n>>> "
          << utf8_line;
    }
  }

  for (const auto& unknown_uws_file : report.unknown_uws().files()) {
    std::string utf8_path = unknown_uws_file.file_information().path();
    EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_path)),
                               unsanitized_path_strings))
        << "Found unsanitized unknown uws file:\n>>> " << utf8_path;
  }

  for (const auto& detected_uws : report.detected_uws()) {
    CheckFieldForUnsanitizedPaths(detected_uws.files(), "uws files",
                                  unsanitized_path_strings);
    for (const auto& folder : detected_uws.folders()) {
      std::string utf8_path = folder.folder_information().path();
      EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_path)),
                                 unsanitized_path_strings))
          << "Found unsanitized detected uws folder:\n>>> " << utf8_path;
    }

    // Intentionally skipping registry values since we don't sanitize them on
    // purpose so we are sure what their value is.

    for (const auto& scheduled_task : detected_uws.scheduled_tasks()) {
      std::string utf8_path = scheduled_task.scheduled_task().name();
      EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_path)),
                                 unsanitized_path_strings))
          << "Found unsanitized detected uws scheduled task:\n>>> "
          << utf8_path;
    }
  }

  // TODO(joenotcharles): Switch this over to use report.DebugString() instead
  // of checking individual fields.
  CheckFieldForUnsanitizedPaths(report.system_report().loaded_modules(),
                                "loaded module", unsanitized_path_strings);
  CheckFieldForUnsanitizedPaths(report.system_report().processes(), "process",
                                unsanitized_path_strings);
  CheckFieldForUnsanitizedPaths(report.system_report().services(), "service",
                                unsanitized_path_strings);
  CheckFieldForUnsanitizedPaths(
      report.system_report().layered_service_providers(),
      "layered service provider", unsanitized_path_strings);

  for (const auto& installed_extensions :
       report.system_report().installed_extensions()) {
    for (const auto& extension_file : installed_extensions.extension_files()) {
      std::string utf8_path = extension_file.path();
      EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_path)),
                                 unsanitized_path_strings))
          << "Found unsanitized installed extension file:\n>>> " << utf8_path;
    }
  }

  for (const auto& installed_program :
       report.system_report().installed_programs()) {
    std::string utf8_path = installed_program.folder_information().path();
    EXPECT_FALSE(ContainsAnyOf(StringToWLower(base::UTF8ToWide(utf8_path)),
                               unsanitized_path_strings))
        << "Found unsanitized layered service provider:\n>>> " << utf8_path;
  }

  // Intentionally skipping |system_report().registry_values()| since we don't
  // sanitize them on purpose so we are sure what their value is.

  for (const auto& scheduled_task : report.system_report().scheduled_tasks()) {
    CheckFieldForUnsanitizedPaths(
        scheduled_task.actions(),
        "scheduled task action for " + scheduled_task.name(),
        unsanitized_path_strings);
  }

  return !::testing::Test::HasFailure();
}

enum class TestFeatures {
  kNone,
  kWithoutSandbox,
};

// We can't use testing::Range with an enum so create an array to use with
// testing::ValuesIn.
// clang-format off
constexpr TestFeatures kAllTestFeatures[] = {
    TestFeatures::kNone,
    TestFeatures::kWithoutSandbox,
};
// clang-format on

std::ostream& operator<<(std::ostream& stream, TestFeatures features) {
  switch (features) {
    case TestFeatures::kNone:
      stream << "None";
      break;
    case TestFeatures::kWithoutSandbox:
      stream << "WithoutSandbox";
      break;
    default:
      stream << "Unknown" << static_cast<int>(features);
      break;
  }
  return stream;
}

class CleanerTest
    : public ::testing::TestWithParam<std::tuple<TestFeatures, Engine::Name>> {
 public:
  void SetUp() override {
    std::tie(test_features_, engine_) = GetParam();

    // Make sure the test UwS has the flags we expect.
    ASSERT_FALSE(PUPData::IsConfirmedUwS(chrome_cleaner::kGoogleTestAUwSID));
    ASSERT_FALSE(PUPData::IsRemovable(chrome_cleaner::kGoogleTestAUwSID));
    ASSERT_TRUE(PUPData::IsConfirmedUwS(chrome_cleaner::kGoogleTestBUwSID));
    ASSERT_TRUE(PUPData::IsRemovable(chrome_cleaner::kGoogleTestBUwSID));

    base::FilePath start_menu_folder;
    CHECK(base::PathService::Get(base::DIR_START_MENU, &start_menu_folder));
    base::FilePath startup_dir = start_menu_folder.Append(L"Startup");

    scan_only_test_uws_ = startup_dir.Append(chrome_cleaner::kTestUwsAFilename);
    removable_test_uws_ = startup_dir.Append(chrome_cleaner::kTestUwsBFilename);

    // Always create scan-only UwS. Only some tests will have removable UwS.
    ASSERT_NE(-1, base::WriteFile(scan_only_test_uws_,
                                  chrome_cleaner::kTestUwsAFileContents,
                                  chrome_cleaner::kTestUwsAFileContentsSize));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InitializeRemovableUwSArchivePath();
  }

  void TearDown() override {
    if (locked_file_.IsValid())
      locked_file_.Close();
    // Remove any leftover UwS.
    base::DeleteFile(scan_only_test_uws_);
    base::DeleteFile(removable_test_uws_);
  }

  void InitializeRemovableUwSArchivePath() {
    const std::string uws_content(chrome_cleaner::kTestUwsBFileContents,
                                  chrome_cleaner::kTestUwsBFileContentsSize);
    std::string uws_hash;
    ASSERT_TRUE(
        chrome_cleaner::ComputeSHA256DigestOfString(uws_content, &uws_hash));

    const std::wstring zip_filename =
        chrome_cleaner::internal::ConstructZipArchiveFileName(
            chrome_cleaner::kTestUwsBFilename, uws_hash,
            /*max_filename_length=*/255);
    expected_uws_archive_ = temp_dir_.GetPath().Append(zip_filename);
  }

  void CreateRemovableUwS() {
    ASSERT_NE(-1, base::WriteFile(removable_test_uws_,
                                  chrome_cleaner::kTestUwsBFileContents,
                                  chrome_cleaner::kTestUwsBFileContentsSize));
  }

  void LockRemovableUwS() {
    // Opening a handle to the file will allow other processes read access, but
    // not deletion.
    locked_file_.Initialize(removable_test_uws_, base::File::FLAG_OPEN |
                                                     base::File::FLAG_READ |
                                                     base::File::FLAG_WRITE);
  }

  void ExpectExitCode(const base::CommandLine& command_line,
                      int expected_exit_code) {
    chrome_cleaner::ChildProcessLogger logger;
    ASSERT_TRUE(logger.Initialize());

    base::LaunchOptions options;
    logger.UpdateLaunchOptions(&options);
    base::Process process(base::LaunchProcess(command_line, options));
    if (!process.IsValid())
      logger.DumpLogs();
    ASSERT_TRUE(process.IsValid());

    int exit_code = -1;
    bool exited_within_timeout = process.WaitForExitWithTimeout(
        base::TimeDelta::FromMinutes(10), &exit_code);
    EXPECT_TRUE(exited_within_timeout);
    EXPECT_EQ(expected_exit_code, exit_code);
    if (!exited_within_timeout || expected_exit_code != exit_code)
      logger.DumpLogs();
    if (!exited_within_timeout)
      process.Terminate(/*exit_code=*/-1, /*wait=*/false);
  }

  base::CommandLine BuildCommandLine(
      const wchar_t* executable_path,
      ExecutionMode execution_mode = ExecutionMode::kNone) {
    base::FilePath path(executable_path);
    base::CommandLine command_line(path);
    chrome_cleaner::AppendTestSwitches(temp_dir_, &command_line);
    command_line.AppendSwitchASCII(
        chrome_cleaner::kEngineSwitch,
        base::NumberToString(static_cast<int>(engine_)));
    if (execution_mode != ExecutionMode::kNone) {
      command_line.AppendSwitchASCII(
          chrome_cleaner::kExecutionModeSwitch,
          base::NumberToString(static_cast<int>(execution_mode)));
    }
    command_line.AppendSwitchPath(chrome_cleaner::kQuarantineDirSwitch,
                                  temp_dir_.GetPath());
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
    if (test_features_ == TestFeatures::kWithoutSandbox) {
      command_line.AppendSwitch(
          chrome_cleaner::kRunWithoutSandboxForTestingSwitch);
    }

    // Scan only the SHELL location, where test UwS are created. This has little
    // impact on the TestOnly engine (which is very fast) but a large impact
    // with the real engine. Official cleaner builds still scan all locations
    // in case there's an issue with the other scan locations.
    command_line.AppendSwitchASCII(
        chrome_cleaner::kScanLocationsSwitch,
        base::NumberToString(chrome_cleaner::UwS::FOUND_IN_SHELL));
#endif  // BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)

    return command_line;
  }

 protected:
  TestFeatures test_features_;
  Engine::Name engine_;
  base::FilePath scan_only_test_uws_;
  base::FilePath removable_test_uws_;
  base::FilePath expected_uws_archive_;
  base::File locked_file_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_P(CleanerTest, Scanner_ScanOnly) {
  base::CommandLine command_line = BuildCommandLine(kScannerExecutable);
  ExpectExitCode(command_line,
                 chrome_cleaner::RESULT_CODE_REPORT_ONLY_PUPS_FOUND);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
}

TEST_P(CleanerTest, Scanner_Removable) {
  CreateRemovableUwS();
  base::CommandLine command_line = BuildCommandLine(kScannerExecutable);

  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_SUCCESS);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
  EXPECT_TRUE(base::PathExists(removable_test_uws_));
  EXPECT_FALSE(base::PathExists(expected_uws_archive_));
}

TEST_P(CleanerTest, Cleaner_ScanOnly) {
  base::CommandLine command_line =
      BuildCommandLine(kCleanerExecutable, ExecutionMode::kCleanup);
  ExpectExitCode(command_line,
                 chrome_cleaner::RESULT_CODE_REPORT_ONLY_PUPS_FOUND);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
}

TEST_P(CleanerTest, Cleaner_Removable) {
  CreateRemovableUwS();
  base::CommandLine command_line =
      BuildCommandLine(kCleanerExecutable, ExecutionMode::kCleanup);

  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));

  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_SUCCESS);
  EXPECT_FALSE(base::PathExists(removable_test_uws_));
  EXPECT_TRUE(base::PathExists(expected_uws_archive_));
}

TEST_P(CleanerTest, Cleaner_LockedFiles) {
  CreateRemovableUwS();
  LockRemovableUwS();
  base::CommandLine command_line =
      BuildCommandLine(kCleanerExecutable, ExecutionMode::kCleanup);
  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_PENDING_REBOOT);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
  EXPECT_TRUE(base::PathExists(removable_test_uws_));
  EXPECT_TRUE(base::PathExists(expected_uws_archive_));
}

TEST_P(CleanerTest, PostReboot_ScanOnly) {
  base::CommandLine command_line =
      BuildCommandLine(kCleanerExecutable, ExecutionMode::kCleanup);
  command_line.AppendSwitch(chrome_cleaner::kPostRebootSwitch);
  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_POST_REBOOT_SUCCESS);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
}

TEST_P(CleanerTest, PostReboot_Removable) {
  CreateRemovableUwS();
  base::CommandLine command_line = BuildCommandLine(kCleanerExecutable);
  command_line.AppendSwitch(chrome_cleaner::kPostRebootSwitch);
  command_line.AppendSwitchASCII(
      chrome_cleaner::kExecutionModeSwitch,
      base::NumberToString(static_cast<int>(ExecutionMode::kCleanup)));
  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_POST_REBOOT_SUCCESS);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));

  // The engine should have removed the file.
  EXPECT_FALSE(base::PathExists(removable_test_uws_));
  EXPECT_TRUE(base::PathExists(expected_uws_archive_));
}

TEST_P(CleanerTest, PostReboot_LockedFiles) {
  CreateRemovableUwS();
  LockRemovableUwS();
  base::CommandLine command_line = BuildCommandLine(kCleanerExecutable);
  command_line.AppendSwitch(chrome_cleaner::kPostRebootSwitch);
  command_line.AppendSwitchASCII(
      chrome_cleaner::kExecutionModeSwitch,
      base::NumberToString(static_cast<int>(ExecutionMode::kCleanup)));

  ExpectExitCode(command_line,
                 chrome_cleaner::RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED);
  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
  EXPECT_TRUE(base::PathExists(removable_test_uws_));
  EXPECT_TRUE(base::PathExists(expected_uws_archive_));
}

TEST_P(CleanerTest, NoPotentialFalsePositivesOnCleanMachine) {
  base::CommandLine command_line = BuildCommandLine(kCleanerExecutable);
  command_line.AppendSwitchASCII(
      chrome_cleaner::kExecutionModeSwitch,
      base::NumberToString(static_cast<int>(ExecutionMode::kCleanup)));

  // Delete the scan only uws to make the machine clean.
  base::DeleteFile(scan_only_test_uws_);

  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_NO_PUPS_FOUND);
}

TEST_P(CleanerTest, NoUnsanitizedPaths) {
  CreateRemovableUwS();

  base::CommandLine command_line = BuildCommandLine(kCleanerExecutable);
  LOG(ERROR) << command_line.GetCommandLineString();
  command_line.AppendSwitch(chrome_cleaner::kDumpRawLogsSwitch);
  command_line.AppendSwitchASCII(
      chrome_cleaner::kExecutionModeSwitch,
      base::NumberToString(static_cast<int>(ExecutionMode::kCleanup)));
  ExpectExitCode(command_line, chrome_cleaner::RESULT_CODE_SUCCESS);

  chrome_cleaner::ChromeCleanerReport chrome_cleaner_report;
  EXPECT_TRUE(
      ParseSerializedReport(kCleanerExecutable, &chrome_cleaner_report));

  // Ensure the report doesn't have any unsanitized paths.
  EXPECT_TRUE(CheckCleanerReportForUnsanitizedPaths(chrome_cleaner_report));
}

// Test all features with the TestOnly engine, which is quick.
INSTANTIATE_TEST_SUITE_P(AllFeatures,
                         CleanerTest,
                         Combine(ValuesIn(kAllTestFeatures),
                                 Values(Engine::TEST_ONLY)),
                         chrome_cleaner::GetParamNameForTest());

#if BUILDFLAG(IS_INTERNAL_CHROME_CLEANER_BUILD)
// The full scan with the ESET engine takes too long to test more than once so
// don't enable any test features. In fact, don't test it in debug builds since
// they are slower.
#ifdef NDEBUG
INSTANTIATE_TEST_SUITE_P(EsetFeatures,
                         CleanerTest,
                         Combine(Values(TestFeatures::kNone),
                                 Values(Engine::ESET)),
                         chrome_cleaner::GetParamNameForTest());
#endif  // NDEBUG
#endif  // BUILDFLAG(IS_INTERNAL_CHROME_CLEANER_BUILD)

}  // namespace
