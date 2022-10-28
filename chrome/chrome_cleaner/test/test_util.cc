// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_util.h"

#include <stdint.h>
#include <windows.h>

#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/crash/crash_client.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/scoped_logging.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_remover_allowlist.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "chrome/chrome_cleaner/os/post_reboot_registration.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/rebooter.h"
#include "chrome/chrome_cleaner/os/scoped_disable_wow64_redirection.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

bool IsSandboxedProcess() {
  static const bool is_sandboxed_process =
      (sandbox::SandboxFactory::GetTargetServices() != nullptr);
  return is_sandboxed_process;
}

// base::TestSuite's Initialize method initializes logging differently than we
// do. This subclass ensures logging is properly initialized using ScopedLogging
// after base::TestSuite::Initialize has run.
class ChromeCleanerTestSuite : public base::TestSuite {
 public:
  // Inherit constructors.
  using base::TestSuite::TestSuite;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
    scoped_logging = std::make_unique<chrome_cleaner::ScopedLogging>(
        IsSandboxedProcess() ? chrome_cleaner::kSandboxLogFileSuffix : L"");
  }

 private:
  std::unique_ptr<chrome_cleaner::ScopedLogging> scoped_logging;
};

}  // namespace

bool SetupTestConfigs() {
  return SetupTestConfigsWithCatalogs({&TestUwSCatalog::GetInstance()});
}

bool SetupTestConfigsWithCatalogs(const PUPData::UwSCatalogs& catalogs) {
  if (!InitializeOSUtils())
    return false;

  PUPData::InitializePUPData(catalogs);

  PreFetchedPaths::GetInstance()->DisableForTesting();
  base::PathService::DisableCache();

  FileRemoverAllowlist::GetInstance()->DisableCache();

  return true;
}

int RunChromeCleanerTestSuite(int argc,
                              char** argv,
                              const PUPData::UwSCatalogs& catalogs) {
  ChromeCleanerTestSuite test_suite(argc, argv);

  if (!chrome_cleaner::SetupTestConfigsWithCatalogs(catalogs))
    return 1;

  // Make sure tests will not end up in an infinite reboot loop.
  if (chrome_cleaner::Rebooter::IsPostReboot())
    return 0;

#if BUILDFLAG(IS_INTERNAL_CHROME_CLEANER_BUILD)
  // The tests will run with the internal engine, which takes longer.
  // IS_INTERNAL_CHROME_CLEANER_BUILD is only set on the Chrome Cleaner
  // builders, not the chromium builders, so this will not slow down the
  // general commit queue.
  constexpr base::TimeDelta kInternalTimeout = base::Minutes(10);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTestLauncherTimeout,
      base::NumberToString(kInternalTimeout.InMilliseconds()));
#endif

  // ScopedCOMInitializer keeps COM initialized in a specific scope. We don't
  // want to initialize it for sandboxed processes, so manage its lifetime with
  // a unique_ptr, which will call ScopedCOMInitializer's destructor when it
  // goes out of scope below.
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer;

  if (!IsSandboxedProcess()) {
    scoped_com_initializer = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    bool success = chrome_cleaner::InitializeCOMSecurity();
    DCHECK(success) << "InitializeCOMSecurity() failed.";

    success = chrome_cleaner::TaskScheduler::Initialize();
    DCHECK(success) << "TaskScheduler::Initialize() failed.";

    // Crash reporting must be initialized only once, so it cannot be
    // initialized by individual tests or fixtures. Also, since crashpad does
    // not actually enable uploading of crash reports in non-official builds
    // (unless forced to by the --enable-crash-reporting flag) we don't need to
    // disable crash reporting.
    chrome_cleaner::CrashClient::GetInstance()->InitializeCrashReporting(
        chrome_cleaner::CrashClient::Mode::CLEANER,
        chrome_cleaner::SandboxType::kNonSandboxed);
  }

  // Some tests spawn sandbox targets using job objects. Windows 7 doesn't
  // support nested job objects, so don't use them in the test suite. Otherwise
  // all sandbox tests will fail as they try to create a second job object.
  bool use_job_objects = base::win::GetVersion() >= base::win::Version::WIN8;

  // Some tests will fail if two tests try to launch test_process.exe
  // simultaneously, so run the tests serially. This will still shard them and
  // distribute the shards to different swarming bots, but tests will run
  // serially on each bot.
  const int result = base::LaunchUnitTestsWithOptions(
      argc, argv,
      /*parallel_jobs=*/1U,        // Like LaunchUnitTestsSerially
      /*default_batch_limit=*/10,  // Like LaunchUnitTestsSerially
      use_job_objects, base::DoNothing(),
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  if (!IsSandboxedProcess())
    chrome_cleaner::TaskScheduler::Terminate();

  return result;
}

ScopedIsPostReboot::ScopedIsPostReboot() {
  // This switch will be removed by scoped_command_line_'s destructor when it
  // goes out of scope.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(kPostRebootSwitch);
}

bool RunOnceCommandLineContains(const std::wstring& product_shortname,
                                const wchar_t* sub_string) {
  DCHECK(sub_string);
  PostRebootRegistration post_reboot(product_shortname);
  std::wstring run_once_value = post_reboot.RunOnceOnRestartRegisteredValue();
  return WStringContainsCaseInsensitive(run_once_value, sub_string);
}

bool RunOnceOverrideCommandLineContains(const std::string& cleanup_id,
                                        const wchar_t* sub_string) {
  DCHECK(sub_string);

  std::wstring reg_value;
  base::win::RegKey run_once_key(
      HKEY_CURRENT_USER,
      PostRebootRegistration::GetPostRebootSwitchKeyPath().c_str(), KEY_READ);
  if (run_once_key.Valid()) {
    // There is no need to check the return value, since ReadValue will leave
    // |reg_value| empty on error.
    run_once_key.ReadValue(base::UTF8ToWide(cleanup_id).c_str(), &reg_value);
  }

  return WStringContainsCaseInsensitive(reg_value, sub_string);
}

bool RegisterTestTask(TaskScheduler* task_scheduler,
                      TaskScheduler::TaskInfo* task_info) {
  const wchar_t name[] = L"Chrome Cleaner Test task";
  const wchar_t description[] =
      L"Chrome Cleaner Test Task Used Just For Testing";

  const base::FilePath exe_path =
      PreFetchedPaths::GetInstance()->GetExecutablePath();

  TaskScheduler::TaskExecAction action{
      exe_path, base::FilePath(), L"argument",
  };

  base::CommandLine command_line(action.application_path);
  command_line.AppendArgNative(action.arguments);
  if (!task_scheduler->RegisterTask(
          name, description, command_line,
          chrome_cleaner::TaskScheduler::TRIGGER_TYPE_HOURLY, false)) {
    LOG(ERROR) << "Failed to register test task";
    return false;
  }

  task_info->name = name;
  task_info->description = description;
  task_info->exec_actions.push_back(action);
  return true;
}

void AppendTestSwitches(const base::ScopedTempDir& temp_dir,
                        base::CommandLine* command_line) {
  command_line->AppendSwitch(kNoRecoveryComponentSwitch);
  command_line->AppendSwitch(kNoCrashUploadSwitch);
  command_line->AppendSwitch(kNoReportUploadSwitch);
  command_line->AppendSwitch(kNoSelfDeleteSwitch);
  // Some tests spawn an executable which spawns a sandboxed process. All of
  // these logs should go to a temporary directory so we can ensure they are
  // deleted after the test.
  command_line->AppendSwitchPath(kTestLoggingPathSwitch, temp_dir.GetPath());
}

void ExpectDiskFootprint(const PUPData::PUP& pup,
                         const base::FilePath& expected_path) {
  for (const auto& path : pup.expanded_disk_footprints.file_paths()) {
    if (PathEqual(expected_path, path))
      return;
  }
  ADD_FAILURE() << "Expected file: " << expected_path.value();
}

// Expect the scheduled task footprint to be found in |pup|.
void ExpectScheduledTaskFootprint(const PUPData::PUP& pup,
                                  const wchar_t* task_name) {
  DCHECK(task_name);
  for (const auto& footprint : pup.expanded_scheduled_tasks) {
    if (footprint == task_name)
      return;
  }
  ADD_FAILURE() << "Expected schedule task not found: '" << task_name << "'.";
}

bool StringContainsCaseInsensitive(const std::string& value,
                                   const std::string& substring) {
  return base::ranges::search(
             value, substring,
             base::CaseInsensitiveCompareASCII<std::string::value_type>()) !=
         value.end();
}

LoggingOverride::LoggingOverride() {
  DCHECK(!active_logging_messages_);
  active_logging_messages_ = &logging_messages_;
  logging::SetLogMessageHandler(&LogMessageHandler);
}

LoggingOverride::~LoggingOverride() {
  logging::SetLogMessageHandler(nullptr);
  logging_messages_.clear();
  DCHECK_EQ(active_logging_messages_, &logging_messages_);
  active_logging_messages_ = nullptr;
}

bool LoggingOverride::LoggingMessagesContain(const std::string& sub_string) {
  for (const auto& line : logging_messages_) {
    if (StringContainsCaseInsensitive(line, sub_string))
      return true;
  }
  return false;
}

bool LoggingOverride::LoggingMessagesContain(const std::string& sub_string1,
                                             const std::string& sub_string2) {
  for (const auto& line : logging_messages_) {
    if (StringContainsCaseInsensitive(line, sub_string1) &&
        StringContainsCaseInsensitive(line, sub_string2))
      return true;
  }
  return false;
}

std::vector<std::string>* LoggingOverride::active_logging_messages_ = nullptr;

bool IsSubsetOf(const FilePathSet& set1,
                const FilePathSet& set2,
                const std::string& unexpected_path_log_message) {
  bool is_subset = true;
  for (const base::FilePath& file_path : set1.file_paths()) {
    if (!set2.Contains(file_path)) {
      LOG(ERROR) << unexpected_path_log_message << ": '" << file_path.value()
                 << "'";
      is_subset = false;
    }
  }
  return is_subset;
}

void ExpectEqualFilePathSets(const FilePathSet& matched_files,
                             const FilePathSet& expected_files) {
  EXPECT_TRUE(IsSubsetOf(matched_files, expected_files,
                         "Unexpected file in matched footprints"));
  EXPECT_TRUE(
      IsSubsetOf(expected_files, matched_files, "Missing expected footprint"));
}

base::FilePath GetWow64RedirectedSystemPath() {
  std::vector<wchar_t> buffer;
  size_t size_needed = MAX_PATH;
  while (size_needed > buffer.size()) {
    buffer.resize(size_needed);
    size_needed = ::GetSystemWow64DirectoryW(buffer.data(), buffer.size());
    if (size_needed == 0) {
      PLOG(ERROR) << "Could not get system Wow64 directory";
      return base::FilePath();
    }
  }
  buffer.push_back(L'\0');
  return base::FilePath(buffer.data());
}

base::FilePath GetSampleDLLPath() {
  // The sample DLL should be next to the executable because it is generated at
  // build time.
  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.Append(L"empty_dll.dll");
}

base::FilePath GetSignedSampleDLLPath() {
  // The signed sample DLL should be next to the executable because it's copied
  // there at build time.
  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.Append(L"signed_empty_dll.dll");
}

ScopedTempDirNoWow64::ScopedTempDirNoWow64() = default;

ScopedTempDirNoWow64::~ScopedTempDirNoWow64() {
  // Since the temp dir was created with Wow64 disabled, it must be deleted
  // with Wow64 disabled.
  ScopedDisableWow64Redirection disable_wow64_redirection;
  std::ignore = Delete();

  // The parent's destructor will call Delete again, without disabling Wow64,
  // which could delete a directory with the same name in SysWOW64. So make
  // sure the path is cleared even if the Delete call above failed.
  Take();
}

bool ScopedTempDirNoWow64::CreateUniqueSystem32TempDir() {
  ScopedDisableWow64Redirection disable_wow64_redirection;
  base::FilePath system_path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &system_path)) {
    PLOG(ERROR) << "Unable to get system32 path";
    return false;
  }
  // Note that on 32-bit Windows this check will always succeed because
  // GetWow64RedirectedSystemPath returns an empty string. This is correct
  // because Wow64 redirection isn't supported on 32-bit Windows, so the system
  // path is guaranteed not to be redirected.
  DCHECK(system_path != GetWow64RedirectedSystemPath());
  return CreateUniqueTempDirUnderPath(system_path);
}

bool ScopedTempDirNoWow64::CreateEmptyFileInUniqueSystem32TempDir(
    const std::wstring& file_name) {
  if (!CreateUniqueSystem32TempDir())
    return false;
  ScopedDisableWow64Redirection disable_wow64_redirection;
  return CreateEmptyFile(GetPath().Append(file_name));
}

bool CheckTestPrivileges() {
  // Check for administrator privileges, unless running in the sandbox.
  const bool is_sandboxed_process =
      (sandbox::SandboxFactory::GetTargetServices() != nullptr);
  if (!is_sandboxed_process && !chrome_cleaner::HasAdminRights()) {
    LOG(ERROR) << "Some Chrome Cleanup tests need administrator privileges.";
    return false;
  }

  // All programs run under the msys git shell have debug privileges (!), which
  // breaks the assumptions of some of the tests. So drop that privilege unless
  // actually running under a debugger.
  if (!::IsDebuggerPresent() &&
      !chrome_cleaner::ReleaseDebugRightsPrivileges()) {
    PLOG(ERROR) << "Failed to release debug privileges";
    return false;
  }

  return true;
}

bool ResetAclForUcrtbase() {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::BasePathKey::DIR_EXE, &exe_path)) {
    LOG(ERROR) << "Failed to get directory path.";
    return false;
  }
  base::FilePath abs_path = base::MakeAbsoluteFilePath(exe_path);
#ifdef NDEBUG
  base::FilePath ucrt_path = abs_path.Append(L"ucrtbase.dll");
#else
  base::FilePath ucrt_path = abs_path.Append(L"ucrtbased.dll");
#endif
  base::CommandLine cmd({L"icacls"});
  cmd.AppendArgPath(ucrt_path);
  cmd.AppendArg("/reset");
  cmd.AppendArg("/t");
  base::Process process =
      base::LaunchProcess(cmd, base::LaunchOptionsForTest());
  int exit_code = 0;
  if (!process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                      &exit_code)) {
    LOG(ERROR) << "Failed to reset acl for file " << ucrt_path.value();
    return false;
  }
  if (exit_code) {
    LOG(ERROR) << "Failed to reset acl for file " << ucrt_path.value()
               << " with exit code " << exit_code;
  }
  return !exit_code;
}

}  // namespace chrome_cleaner
