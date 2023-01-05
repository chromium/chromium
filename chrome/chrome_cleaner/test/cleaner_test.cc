// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_test_util.h"
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
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using chrome_cleaner::Engine;
using chrome_cleaner::ExecutionMode;
using chrome_cleaner::MockChromePromptResponder;
using chrome_cleaner::PromptUserResponse;
using chrome_cleaner::PUPData;
using ::testing::Combine;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
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
  return base::ranges::any_of(
      substrings, [&main_string](const std::wstring& path) {
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

// Base class for tests that use various engines.
class CleanerTestBase : public ::testing::Test {
 public:
  void SetUp() override {
    // Make sure the test UwS has the flags we expect.
    ASSERT_FALSE(PUPData::IsConfirmedUwS(chrome_cleaner::kGoogleTestAUwSID));
    ASSERT_FALSE(PUPData::IsRemovable(chrome_cleaner::kGoogleTestAUwSID));
    ASSERT_TRUE(PUPData::IsConfirmedUwS(chrome_cleaner::kGoogleTestBUwSID));
    ASSERT_TRUE(PUPData::IsRemovable(chrome_cleaner::kGoogleTestBUwSID));

    base::FilePath start_menu_folder;
    CHECK(base::PathService::Get(base::DIR_START_MENU, &start_menu_folder));
    base::FilePath startup_dir = start_menu_folder.Append(L"Startup");

    scan_only_test_uws_ = chrome_cleaner::NormalizePath(
        startup_dir.Append(chrome_cleaner::kTestUwsAFilename));
    removable_test_uws_ = chrome_cleaner::NormalizePath(
        startup_dir.Append(chrome_cleaner::kTestUwsBFilename));

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

  // Launches the process given on |command_line|, and expects it to exit with
  // |expected_exit_code|. The child process is also given access to
  // |handles_to_inherit| if it isn't empty. If |mock_responder| is not null,
  // it will read ChromePrompt requests from the child process in a background
  // thread while the child is running.
  void ExpectExitCode(const base::CommandLine& command_line,
                      int expected_exit_code,
                      base::HandlesToInheritVector handles_to_inherit = {},
                      MockChromePromptResponder* mock_responder = nullptr) {
    chrome_cleaner::ChildProcessLogger logger;
    ASSERT_TRUE(logger.Initialize());

    base::LaunchOptions options;
    options.handles_to_inherit = handles_to_inherit;
    logger.UpdateLaunchOptions(&options);
    base::Process process(base::LaunchProcess(command_line, options));
    if (!process.IsValid())
      logger.DumpLogs();
    ASSERT_TRUE(process.IsValid());

    // Past this point, do not return without waiting on |done_reading_event|,
    // so that it doesn't go out of scope while another thread has a pointer to
    // it.
    base::WaitableEvent done_reading_event;
    if (!mock_responder) {
      // Nothing to read.
      done_reading_event.Signal();
    } else {
      // Unretained is safe because this function will not return until
      // |done_reading_event| is signalled.
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&MockChromePromptResponder::ReadRequests,
                         base::Unretained(mock_responder),
                         base::Unretained(&done_reading_event)));
    }

    int exit_code = -1;
    bool exited_within_timeout =
        process.WaitForExitWithTimeout(base::Minutes(10), &exit_code);
    EXPECT_TRUE(exited_within_timeout);
    EXPECT_EQ(expected_exit_code, exit_code);
    if (!exited_within_timeout || expected_exit_code != exit_code)
      logger.DumpLogs();
    if (!exited_within_timeout)
      process.Terminate(/*exit_code=*/-1, /*wait=*/false);

    // Wait until any last messages the child process wrote are processed.
    done_reading_event.TimedWait(TestTimeouts::action_timeout());
  }

  virtual base::CommandLine BuildCommandLine(
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
    return command_line;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  Engine::Name engine_ = Engine::TEST_ONLY;
  base::FilePath scan_only_test_uws_;
  base::FilePath removable_test_uws_;
  base::FilePath expected_uws_archive_;
  base::File locked_file_;

 private:
  base::ScopedTempDir temp_dir_;
};

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

class CleanerTest : public CleanerTestBase,
                    public ::testing::WithParamInterface<
                        std::tuple<TestFeatures, Engine::Name>> {
 public:
  CleanerTest() { std::tie(test_features_, engine_) = GetParam(); }

  base::CommandLine BuildCommandLine(
      const wchar_t* executable_path,
      ExecutionMode execution_mode = ExecutionMode::kNone) override {
    base::CommandLine command_line =
        CleanerTestBase::BuildCommandLine(executable_path, execution_mode);
#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
    // WithoutSandbox switch is not supported in the official build.
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
  // Fails on Windows7/8/8.1
  // TODO(crbug/1405033): This is temporary while we disable these tests in
  // pre-M110 branches.
  if (base::win::GetVersion() <= base::win::Version::WIN8_1) {
    return;
  }

  CreateRemovableUwS();

  base::CommandLine command_line = BuildCommandLine(kCleanerExecutable);
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

// In Scanning mode, the cleaner communicates with Chrome over a ChromePrompt
// IPC connection, to report the names of found UwS and receive the user's
// permission to start cleaning. These tests validate the behaviour of the
// cleaner depending on the response from Chrome. They only run under the
// test-only engine, because the ESET engine is slower and they don't depend on
// the implementation of the UwS scanner, only on its output.
// Parameters are (enable scanning mode logs, enable cleaning mode logs).
class CleanerScanningModeTest
    : public CleanerTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  using StrictMockChromePromptResponder =
      ::testing::StrictMock<MockChromePromptResponder>;

  void SetUp() override {
    CleanerTestBase::SetUp();

    command_line_ =
        BuildCommandLine(kCleanerExecutable, ExecutionMode::kScanning);
    chrome_cleaner::ChromePromptPipeHandles pipe_handles =
        chrome_cleaner::CreateTestChromePromptMessagePipes(
            chrome_cleaner::ChromePromptServerProcess::kChromeIsServer,
            &command_line_, &handles_to_inherit_);
    ASSERT_TRUE(pipe_handles.IsValid());
    mock_responder_ = std::make_unique<StrictMockChromePromptResponder>(
        std::move(pipe_handles));

    // Start a test server to receive logs uploads.
    test_safe_browsing_server_.RegisterRequestHandler(
        base::BindRepeating(&CleanerScanningModeTest::HandleLogsUploadRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(test_server_handle_ =
                    test_safe_browsing_server_.StartAndReturnHandle());
    command_line_.AppendSwitchASCII(
        chrome_cleaner::kTestLoggingURLSwitch,
        test_safe_browsing_server_.base_url().spec());
    // kNoReportUploadsSwitch was added in the base class by AppendTestSwitches
    // to prevent tests from uploading logs to the real safe browsing server.
    command_line_.RemoveSwitch(chrome_cleaner::kNoReportUploadSwitch);

    // See SwReporterInvocationType at
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h;l=55;drc=4a5ef27e49b17f08b306284ad933d243a1d6b310
    // for a full description of when logs are sent. It boils down to two
    // scenarios:
    // - If the user initiated a scan from the Settings page and chose "report
    //   details to Google", send logs from the cleaner in scanning mode.
    // - Otherwise, do not send logs in scanning mode. (Either because the user
    //   has opted out of logs entirely, or because they were already sent by
    //   the reporter.)
    // In both cases, if removable UwS is found, the response from PromptUser
    // will control whether logs are sent in cleaning mode.
    std::tie(scanning_mode_logs_, cleaning_mode_logs_) = GetParam();
    if (scanning_mode_logs_) {
      // kWithScanningModeLogs is added by Chrome when the user has allowed
      // "report details to Google" in the UI.
      command_line_.AppendSwitch(chrome_cleaner::kWithScanningModeLogsSwitch);
    }
  }

  void TearDown() override {
    // Add an error to all tests if there were any malformed logs requests.
    EXPECT_EQ(logs_upload_errors_, 0);
    CleanerTestBase::TearDown();
  }

  void LaunchCleanerAndExpectExitCode(int expected_exit_code) {
    ExpectExitCode(command_line_, expected_exit_code, handles_to_inherit_,
                   mock_responder_.get());
  }

  void ExpectCloseConnectionRequest() {
    EXPECT_CALL(*mock_responder_, CloseConnectionRequest())
        .WillOnce(
            InvokeWithoutArgs([this] { mock_responder_->StopReading(); }));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleLogsUploadRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    if (!request.has_content) {
      logs_upload_errors_++;
      return http_response;
    }
    chrome_cleaner::ChromeCleanerReport report;
    if (!report.ParseFromString(request.content)) {
      logs_upload_errors_++;
      return http_response;
    }
    if (report.exit_code() == chrome_cleaner::RESULT_CODE_PENDING)
      intermediate_logs_upload_request_count_++;
    else
      logs_upload_request_count_++;
    return http_response;
  }

 protected:
  base::CommandLine command_line_{base::CommandLine::NO_PROGRAM};
  base::HandlesToInheritVector handles_to_inherit_;
  std::unique_ptr<StrictMockChromePromptResponder> mock_responder_;

  bool scanning_mode_logs_ = false;
  bool cleaning_mode_logs_ = false;
  int logs_upload_request_count_ = 0;
  int intermediate_logs_upload_request_count_ = 0;
  int logs_upload_errors_ = 0;
  net::test_server::EmbeddedTestServer test_safe_browsing_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

TEST_P(CleanerScanningModeTest, ReportOnly) {
  // Report-only UwS should not be reported to the user (files to remove should
  // be empty). Chrome will automatically reply with DENIED instead of showing
  // a prompt.
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_responder_, PromptUserRequest(IsEmpty(), IsEmpty()))
        .WillOnce(InvokeWithoutArgs([this] {
          mock_responder_->SendPromptUserResponse(PromptUserResponse::DENIED);
        }));
    ExpectCloseConnectionRequest();
  }
  LaunchCleanerAndExpectExitCode(
      chrome_cleaner::RESULT_CODE_REPORT_ONLY_PUPS_FOUND);

  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));

  // Cleaning mode did not run so the only logs are from scanning mode.
  EXPECT_EQ(logs_upload_request_count_, scanning_mode_logs_ ? 1 : 0);

  // Intermediate logs are only sent before starting a cleanup.
  EXPECT_EQ(intermediate_logs_upload_request_count_, 0);
}

TEST_P(CleanerScanningModeTest, RemovableDenied) {
  CreateRemovableUwS();

  // Removable UwS is reported to the user, who denies the cleanup.
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_responder_,
                PromptUserRequest(
                    UnorderedElementsAre(removable_test_uws_.AsUTF8Unsafe()),
                    IsEmpty()))
        .WillOnce(InvokeWithoutArgs([this] {
          mock_responder_->SendPromptUserResponse(PromptUserResponse::DENIED);
        }));
    ExpectCloseConnectionRequest();
  }
  LaunchCleanerAndExpectExitCode(
      chrome_cleaner::RESULT_CODE_CLEANUP_PROMPT_DENIED);

  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
  EXPECT_TRUE(base::PathExists(removable_test_uws_));

  // Cleaning mode did not run so the only logs are from scanning mode.
  EXPECT_EQ(logs_upload_request_count_, scanning_mode_logs_ ? 1 : 0);

  // Intermediate logs are only sent before starting a cleanup.
  EXPECT_EQ(intermediate_logs_upload_request_count_, 0);
}

TEST_P(CleanerScanningModeTest, RemovableAccepted) {
  CreateRemovableUwS();

  // Removable UwS is reported to the user, who accepts the cleanup.
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_responder_,
                PromptUserRequest(
                    UnorderedElementsAre(removable_test_uws_.AsUTF8Unsafe()),
                    IsEmpty()))
        .WillOnce(InvokeWithoutArgs([this] {
          mock_responder_->SendPromptUserResponse(
              cleaning_mode_logs_ ? PromptUserResponse::ACCEPTED_WITH_LOGS
                                  : PromptUserResponse::ACCEPTED_WITHOUT_LOGS);
        }));
    ExpectCloseConnectionRequest();
  }
  LaunchCleanerAndExpectExitCode(chrome_cleaner::RESULT_CODE_SUCCESS);

  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
  EXPECT_FALSE(base::PathExists(removable_test_uws_));

  // The final log should not be sent until cleaning is finished, so the
  // cleaning mode logs setting controls uploads.
  EXPECT_EQ(logs_upload_request_count_, cleaning_mode_logs_ ? 1 : 0);

  // Intermediate logs are sent before the cleanup in each mode that has logs
  // enabled.
  int expected_intermediate_logs = 0;
  if (scanning_mode_logs_)
    expected_intermediate_logs++;
  if (cleaning_mode_logs_)
    expected_intermediate_logs++;
  EXPECT_EQ(intermediate_logs_upload_request_count_,
            expected_intermediate_logs);
}

TEST_P(CleanerScanningModeTest, RemovableAcceptedElevationDenied) {
  CreateRemovableUwS();

  // Removable UwS is reported to the user, who accepts the cleanup but then
  // refuses the Windows UAC elevation prompt.
  command_line_.AppendSwitch(chrome_cleaner::kDenyElevationForTestingSwitch);
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_responder_,
                PromptUserRequest(
                    UnorderedElementsAre(removable_test_uws_.AsUTF8Unsafe()),
                    IsEmpty()))
        .WillOnce(InvokeWithoutArgs([this] {
          mock_responder_->SendPromptUserResponse(
              cleaning_mode_logs_ ? PromptUserResponse::ACCEPTED_WITH_LOGS
                                  : PromptUserResponse::ACCEPTED_WITHOUT_LOGS);
        }));
    ExpectCloseConnectionRequest();
  }
  LaunchCleanerAndExpectExitCode(
      chrome_cleaner::RESULT_CODE_ELEVATION_PROMPT_DECLINED);

  EXPECT_TRUE(base::PathExists(scan_only_test_uws_));
  EXPECT_TRUE(base::PathExists(removable_test_uws_));

  // Cleaning mode did not actually run so the scanning mode logs should be
  // sent.
  EXPECT_EQ(logs_upload_request_count_, scanning_mode_logs_ ? 1 : 0);

  // Intermediate logs are sent from scanning mode before the cleanup was
  // attempted.
  EXPECT_EQ(intermediate_logs_upload_request_count_,
            scanning_mode_logs_ ? 1 : 0);
}

INSTANTIATE_TEST_SUITE_P(AllLogsScenarios,
                         CleanerScanningModeTest,
                         Combine(Values(true, false), Values(true, false)),
                         chrome_cleaner::GetParamNameForTest());

}  // namespace
