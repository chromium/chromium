// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/unit_test_util.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/win/scoped_handle.h"
#include "chrome/test/base/process_inspector_win.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#endif

namespace updater::test {

namespace {

// CustomLogPrinter intercepts test part results and prints them using
// Chromium logging, so that assertion failures are tagged with process IDs
// and timestamps.
class CustomLogPrinter : public testing::TestEventListener {
 public:
  // Takes ownership of impl.
  explicit CustomLogPrinter(testing::TestEventListener* impl) : impl_(impl) {}
  ~CustomLogPrinter() override = default;
  CustomLogPrinter(const CustomLogPrinter&) = delete;
  CustomLogPrinter& operator=(const CustomLogPrinter&) = delete;

  // testing::TestEventListener
  void OnTestProgramStart(const testing::UnitTest& unit_test) override {
    impl_->OnTestProgramStart(unit_test);
  }

  void OnTestIterationStart(const testing::UnitTest& unit_test,
                            int iteration) override {
    impl_->OnTestIterationStart(unit_test, iteration);
  }

  void OnEnvironmentsSetUpStart(const testing::UnitTest& unit_test) override {
    impl_->OnEnvironmentsSetUpStart(unit_test);
  }

  void OnEnvironmentsSetUpEnd(const testing::UnitTest& unit_test) override {
    impl_->OnEnvironmentsSetUpEnd(unit_test);
  }

  void OnTestStart(const testing::TestInfo& test_info) override {
    impl_->OnTestStart(test_info);
  }

  // Use Chromium's logging format, so that the process ID and timestamp of the
  // result can be recorded and compared to other lines in the log files.
  void OnTestPartResult(const testing::TestPartResult& result) override {
    if (result.type() == testing::TestPartResult::kSuccess) {
      return;
    }
    logging::LogMessage(result.file_name(), result.line_number(),
                        logging::LOGGING_ERROR)
            .stream()
        << result.message();
  }

  void OnTestEnd(const testing::TestInfo& test_info) override {
    impl_->OnTestEnd(test_info);
  }

  void OnEnvironmentsTearDownStart(
      const testing::UnitTest& unit_test) override {
    impl_->OnEnvironmentsTearDownStart(unit_test);
  }

  void OnEnvironmentsTearDownEnd(const testing::UnitTest& unit_test) override {
    impl_->OnEnvironmentsTearDownEnd(unit_test);
  }

  void OnTestIterationEnd(const testing::UnitTest& unit_test,
                          int iteration) override {
    impl_->OnTestIterationEnd(unit_test, iteration);
  }

  void OnTestProgramEnd(const testing::UnitTest& unit_test) override {
    impl_->OnTestProgramEnd(unit_test);
  }

 private:
  std::unique_ptr<testing::TestEventListener> impl_;
};

// Creates Prefs with the fake updater version set as active.
void SetupFakeUpdaterPrefs(UpdaterScope scope, const base::Version& version) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  ASSERT_TRUE(global_prefs) << "No global prefs.";
  global_prefs->SetActiveVersion(version.GetString());
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  ASSERT_EQ(version.GetString(), global_prefs->GetActiveVersion());
}

// Creates an install folder on the system with the fake updater version.
void SetupFakeUpdaterInstallFolder(UpdaterScope scope,
                                   const base::Version& version,
                                   bool should_create_updater_executable) {
  std::optional<base::FilePath> folder_path =
      GetVersionedInstallDirectory(scope, version);
  ASSERT_TRUE(folder_path);
  const base::FilePath updater_executable_path(
      folder_path->Append(GetExecutableRelativePath()));
  ASSERT_TRUE(base::CreateDirectory(updater_executable_path.DirName()));

  if (should_create_updater_executable) {
    // Create a fake `updater.exe` inside the install folder.
    ASSERT_TRUE(base::CopyFile(folder_path->DirName().AppendASCII("prefs.json"),
                               updater_executable_path));
  }
}

void SetupFakeUpdater(UpdaterScope scope,
                      const base::Version& version,
                      bool should_create_updater_executable) {
  SetupFakeUpdaterPrefs(scope, version);
  SetupFakeUpdaterInstallFolder(scope, version,
                                should_create_updater_executable);
}

}  // namespace

const char kChromeAppId[] = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";

bool IsProcessRunning(const base::FilePath::StringType& executable_name,
                      const base::ProcessFilter* filter) {
  return base::GetProcessCount(executable_name, filter) != 0;
}

bool WaitForProcessesToExit(const base::FilePath::StringType& executable_name,
                            base::TimeDelta wait,
                            const base::ProcessFilter* filter) {
  return base::WaitForProcessesToExit(executable_name, wait, filter);
}

bool KillProcesses(const base::FilePath::StringType& executable_name,
                   int exit_code,
                   const base::ProcessFilter* filter) {
  bool result = true;
  for (const base::ProcessEntry& entry :
       base::NamedProcessIterator(executable_name, filter).Snapshot()) {
    base::Process process = base::Process::Open(entry.pid());
    if (!process.IsValid()) {
      PLOG(ERROR) << "Process invalid for PID: " << executable_name << ": "
                  << entry.pid();
      result = false;
      continue;
    }

    const bool process_terminated = process.Terminate(exit_code, true);

#if BUILDFLAG(IS_WIN)
    PLOG_IF(ERROR, !process_terminated &&
                       !::TerminateProcess(process.Handle(),
                                           static_cast<UINT>(exit_code)))
        << "::TerminateProcess failed: " << executable_name << ": "
        << entry.pid();
#endif  // BUILDFLAG(IS_WIN)

    result &= process_terminated;
  }
  return result;
}

scoped_refptr<PolicyService> CreateTestPolicyService() {
  std::vector<scoped_refptr<PolicyManagerInterface>> managers{
      GetDefaultValuesPolicyManager()};
  return base::MakeRefCounted<PolicyService>(std::move(managers),
                                             /*usage_stats_enabled=*/true);
}

std::string GetTestName() {
  const ::testing::TestInfo* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return test_info ? base::StrCat(
                         {test_info->test_suite_name(), ".", test_info->name()})
                   : "?.?";
}

bool DeleteFileAndEmptyParentDirectories(
    const std::optional<base::FilePath>& file_path) {
  struct Local {
    // Deletes recursively `dir` and its parents up, if dir is empty
    // and until one non-empty parent directory is found.
    static bool DeleteDirsIfEmpty(const base::FilePath& dir) {
      if (!base::DirectoryExists(dir) || !base::IsDirectoryEmpty(dir)) {
        return true;
      }
      if (!base::DeleteFile(dir)) {
        return false;
      }
      return DeleteDirsIfEmpty(dir.DirName());
    }
  };

  if (!file_path || !base::DeleteFile(*file_path)) {
    return false;
  }
  return Local::DeleteDirsIfEmpty(file_path->DirName());
}

base::FilePath GetLogDestinationDir() {
  const char* var = std::getenv("ISOLATED_OUTDIR");
  return var ? base::FilePath::FromUTF8Unsafe(var) : base::FilePath();
}

void InitLoggingForUnitTest(const base::FilePath& log_base_path) {
  const std::optional<base::FilePath> log_file_path = [&log_base_path] {
    const base::FilePath dest_dir = GetLogDestinationDir();
    return dest_dir.empty()
               ? std::nullopt
               : std::make_optional(dest_dir.Append(log_base_path));
  }();
  if (log_file_path) {
    logging::LoggingSettings settings;
    settings.log_file_path = (*log_file_path).value().c_str();
    settings.logging_dest = logging::LOG_TO_ALL;
    logging::InitLogging(settings);
    base::FilePath file_exe;
    const bool succeeded = base::PathService::Get(base::FILE_EXE, &file_exe);
    VLOG_IF(0, succeeded) << "Log initialized for " << file_exe.value()
                          << " -> " << settings.log_file_path;
  }
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new CustomLogPrinter(
      listeners.Release(listeners.default_result_printer())));
}

#if BUILDFLAG(IS_WIN)
namespace {
const wchar_t kProcmonPath[] = L"C:\\tools\\Procmon.exe";
}  // namespace

void MaybeExcludePathsFromWindowsDefender() {
  constexpr char kTestLauncherExcludePathsFromWindowDefender[] =
      "exclude-paths-from-win-defender";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTestLauncherExcludePathsFromWindowDefender)) {
    return;
  }

  if (!IsServiceRunning(L"WinDefend")) {
    VLOG(1) << "WinDefend is not running, no need to add exclusion paths.";
    return;
  }

  base::FilePath program_files;
  base::FilePath program_files_x86;
  base::FilePath local_app_data;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files) ||
      !base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_x86) ||
      !base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data)) {
    return;
  }

  const auto quote_path_value = [](const base::FilePath& path) {
    return base::StrCat({L"'", path.value(), L"'"});
  };
  const std::wstring cmdline =
      base::StrCat({L"PowerShell.exe Add-MpPreference -ExclusionPath ",
                    base::JoinString({quote_path_value(program_files),
                                      quote_path_value(program_files_x86),
                                      quote_path_value(local_app_data)},
                                     L", ")});

  base::LaunchOptions options;
  options.start_hidden = true;
  options.wait = true;
  VLOG(1) << "Running: " << cmdline;
  base::Process process = base::LaunchProcess(cmdline, options);
  LOG_IF(ERROR, !process.IsValid())
      << "Failed to disable Windows Defender: " << cmdline;
}

base::FilePath StartProcmonLogging() {
  if (!::IsUserAnAdmin()) {
    LOG(WARNING) << __func__
                 << ": user is not an admin, skipping procmon logging";
    return {};
  }

  if (!base::PathExists(base::FilePath(kProcmonPath))) {
    LOG(WARNING) << __func__
                 << ": procmon missing, skipping logging: " << kProcmonPath;
    return {};
  }

  base::FilePath dest_dir = GetLogDestinationDir();
  if (dest_dir.empty() || !base::PathExists(dest_dir)) {
    LOG(ERROR) << __func__ << ": failed to get log destination dir";
    return {};
  }

  dest_dir = dest_dir.AppendASCII(GetTestName());
  if (!base::CreateDirectory(dest_dir)) {
    LOG(ERROR) << __func__
               << ": failed to create log destination dir: " << dest_dir;
    return {};
  }

  const base::FilePath pmc_path(GetTestFilePath("ProcmonConfiguration.pmc"));
  CHECK(base::PathExists(pmc_path));

  const base::FilePath pml_file(
      dest_dir.Append(base::ASCIIToWide(base::UnlocalizedTimeFormatWithPattern(
          base::Time::Now(), "yyMMdd-HHmmss.'PML'"))));

  const std::wstring& cmdline = base::StrCat(
      {kProcmonPath, L" /AcceptEula /LoadConfig ",
       base::CommandLine::QuoteForCommandLineToArgvW(pmc_path.value()),
       L" /BackingFile ",
       base::CommandLine::QuoteForCommandLineToArgvW(pml_file.value()),
       L" /Quiet /externalcapture"});
  base::LaunchOptions options;
  options.start_hidden = true;
  VLOG(1) << __func__ << ": running: " << cmdline;
  const base::Process process = base::LaunchProcess(cmdline, options);

  if (!process.IsValid()) {
    LOG(ERROR) << __func__ << ": failed to run: " << cmdline;
    return {};
  }

  // Gives time for the procmon process to start logging. Without a sleep,
  // `procmon` is unable to fully initialize the logging, and subsequently when
  // `procmon /Terminate` is called to terminate the logging `procmon`, it
  // causes the PML log file to corrupt.
  base::PlatformThread::Sleep(base::Seconds(3));

  return pml_file;
}

void StopProcmonLogging(const base::FilePath& pml_file) {
  if (!::IsUserAnAdmin() || !base::PathExists(base::FilePath(kProcmonPath)) ||
      !pml_file.MatchesFinalExtension(L".PML")) {
    return;
  }

  for (const std::wstring& cmdline :
       {base::StrCat({kProcmonPath, L" /Terminate"})}) {
    base::LaunchOptions options;
    options.start_hidden = true;
    options.wait = true;
    VLOG(1) << __func__ << ": running: " << cmdline;
    const base::Process process = base::LaunchProcess(cmdline, options);
    LOG_IF(ERROR, !process.IsValid())
        << __func__ << ": failed to run: " << cmdline;
  }

  // Make a copy of the PML file in case the original gets deleted.
  if (!base::CopyFile(pml_file, pml_file.ReplaceExtension(L".PML.BAK"))) {
    LOG(ERROR) << __func__ << ": failed to backup pml file";
  }
}

EventHolder CreateWaitableEventForTest() {
  NamedObjectAttributes attr = GetNamedObjectAttributes(
      base::NumberToWString(::GetCurrentProcessId()).c_str(),
      GetUpdaterScopeForTesting());
  return {base::WaitableEvent(base::win::ScopedHandle(
              ::CreateEvent(&attr.sa, FALSE, FALSE, attr.name.c_str()))),
          attr.name};
}
#endif  // BUILDFLAG(IS_WIN)

const base::ProcessIterator::ProcessEntries FindProcesses(
    const base::FilePath::StringType& executable_name,
    const base::ProcessFilter* filter) {
  return base::NamedProcessIterator(executable_name, filter).Snapshot();
}

std::string PrintProcesses(const base::FilePath::StringType& executable_name,
                           const base::ProcessFilter* filter) {
  const std::string demarcation(72, '=');
  std::stringstream message;
  message << "Found processes:" << std::endl << demarcation << std::endl;
  for (const base::ProcessEntry& entry :
       FindProcesses(executable_name, filter)) {
    message << entry.exe_file() << ", pid=" << entry.pid()
            << ", creation time=" << [](base::ProcessId pid) {
                 const base::Process process = base::Process::Open(pid);
                 return process.IsValid()
                            ? base::TimeFormatHTTP(process.CreationTime())
                            : "n/a";
               }(entry.pid());
#if BUILDFLAG(IS_WIN)
    message << ", cmdline=" << [](base::ProcessId pid) {
      std::unique_ptr<ProcessInspector> process_inspector =
          ProcessInspector::Create(base::Process::OpenWithAccess(
              pid, PROCESS_ALL_ACCESS | PROCESS_VM_READ));
      return process_inspector ? process_inspector->command_line() : L"n/a";
    }(entry.pid());
#endif
    message << std::endl;
  }
  message << demarcation << std::endl;
  return message.str();
}

bool WaitFor(base::FunctionRef<bool()> predicate,
             base::FunctionRef<void()> still_waiting) {
  constexpr base::TimeDelta kOutputInterval = base::Seconds(10);
  auto notify_next = base::TimeTicks::Now() + kOutputInterval;
  const auto deadline = base::TimeTicks::Now() + TestTimeouts::action_timeout();
  while (base::TimeTicks::Now() < deadline) {
    if (predicate()) {
      return true;
    }
    if (notify_next < base::TimeTicks::Now()) {
      still_waiting();
      notify_next += kOutputInterval;
    }
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
  return false;
}

base::FilePath GetTestFilePath(const char* file_name) {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.AppendASCII("chrome")
      .AppendASCII("updater")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII(file_name);
}

void SetupFakeUpdaterVersion(UpdaterScope scope,
                             const base::Version& base_version,
                             int major_version_offset,
                             bool should_create_updater_executable) {
  std::vector<uint32_t> components = base_version.components();
  base::CheckedNumeric<uint32_t> new_version = components[0];
  new_version += major_version_offset;
  ASSERT_TRUE(new_version.AssignIfValid(&components[0]));
  SetupFakeUpdater(scope, base::Version(std::move(components)),
                   should_create_updater_executable);
}

void SetupMockUpdater(const base::FilePath& mock_updater_path) {
  const base::FilePath updater_dir(mock_updater_path.DirName());

#if BUILDFLAG(IS_WIN)
  // A valid executable is needed for Windows.
  const base::FilePath test_executable(
      GetTestProcessCommandLine(GetUpdaterScopeForTesting(),
                                test::GetTestName())
          .GetProgram());
#else
  // Create an empty temporary file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath test_executable(
      temp_dir.GetPath().Append(mock_updater_path.BaseName()));
  base::File file(test_executable,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
#endif

  for (const base::FilePath& dir :
       {updater_dir, updater_dir.Append(FILE_PATH_LITERAL("1.2.3.4")),
        updater_dir.Append(FILE_PATH_LITERAL("Download")),
        updater_dir.Append(FILE_PATH_LITERAL("Install"))}) {
    ASSERT_TRUE(base::CreateDirectory(dir));

    for (const base::FilePath& executable_name :
         {mock_updater_path.BaseName(),
          base::FilePath(FILE_PATH_LITERAL("mock.executable"))}) {
      ASSERT_TRUE(base::CopyFile(test_executable, dir.Append(executable_name)));
    }
  }
}

void ExpectOnlyMockUpdater(const base::FilePath& mock_updater_path) {
  ASSERT_TRUE(base::PathExists(mock_updater_path));
  int count_mock_updater_path = 0;

  base::FileEnumerator(
      mock_updater_path.DirName(), false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES)
      .ForEach([&mock_updater_path,
                &count_mock_updater_path](const base::FilePath& item) {
        if (item == mock_updater_path) {
          ++count_mock_updater_path;
        } else {
          ADD_FAILURE() << "Unexpected file/directory found: " << item;
        }
      });

  EXPECT_EQ(count_mock_updater_path, 1);
}

void ExpectTagArgsEqual(const updater::tagging::TagArgs& actual,
                        const updater::tagging::TagArgs& expected) {
  EXPECT_EQ(actual.bundle_name, expected.bundle_name);
  EXPECT_EQ(actual.installation_id, expected.installation_id);
  EXPECT_EQ(actual.brand_code, expected.brand_code);
  EXPECT_EQ(actual.client_id, expected.client_id);
  EXPECT_EQ(actual.experiment_labels, expected.experiment_labels);
  EXPECT_EQ(actual.referral_id, expected.referral_id);
  EXPECT_EQ(actual.language, expected.language);
  EXPECT_EQ(actual.browser_type, expected.browser_type);
  EXPECT_EQ(actual.usage_stats_enable, expected.usage_stats_enable);
  EXPECT_EQ(actual.enrollment_token, expected.enrollment_token);

  EXPECT_EQ(actual.apps.size(), expected.apps.size());
  for (size_t i = 0; i < expected.apps.size(); ++i) {
    const updater::tagging::AppArgs& app_actual = actual.apps[i];
    const updater::tagging::AppArgs& app_expected = expected.apps[i];

    EXPECT_EQ(app_actual.app_id, app_expected.app_id);
    EXPECT_EQ(app_actual.app_name, app_expected.app_name);
    EXPECT_EQ(app_actual.needs_admin, app_expected.needs_admin);
    EXPECT_EQ(app_actual.ap, app_expected.ap);
    EXPECT_EQ(app_actual.encoded_installer_data,
              app_expected.encoded_installer_data);
    EXPECT_EQ(app_actual.install_data_index, app_expected.install_data_index);
    EXPECT_EQ(app_actual.experiment_labels, app_expected.experiment_labels);
  }

  EXPECT_EQ(actual.runtime_mode, expected.runtime_mode);
}

int WaitForProcess(base::Process& process) {
  int exit_code = -1;
  bool process_exited = false;
  base::RunLoop wait_for_process_exit_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE, base::BindLambdaForTesting([&] {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_blocking;
            process_exited = base::WaitForMultiprocessTestChildExit(
                process, TestTimeouts::action_timeout(), &exit_code);
          }),
          wait_for_process_exit_loop.QuitClosure());
  wait_for_process_exit_loop.Run();
  EXPECT_TRUE(process_exited);
  return exit_code;
}

}  // namespace updater::test
