// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/unittest_util.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/strings/string_number_conversions_win.h"
#include "base/win/scoped_handle.h"
#include "chrome/test/base/process_inspector_win.h"
#include "chrome/updater/util/win_util.h"
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
    if (result.type() == testing::TestPartResult::kSuccess)
      return;
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

}  // namespace

const char kChromeAppId[] = "{8A69D345-D564-463C-AFF1-A69D9E530F96}";

bool IsProcessRunning(const base::FilePath::StringType& executable_name) {
  return base::GetProcessCount(executable_name, nullptr) != 0;
}

bool WaitForProcessesToExit(const base::FilePath::StringType& executable_name,
                            base::TimeDelta wait) {
  return base::WaitForProcessesToExit(executable_name, wait, nullptr);
}

bool KillProcesses(const base::FilePath::StringType& executable_name,
                   int exit_code) {
  bool result = true;
  for (const base::ProcessEntry& entry :
       base::NamedProcessIterator(executable_name, nullptr).Snapshot()) {
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
  PolicyService::PolicyManagerVector managers;
  managers.push_back(GetDefaultValuesPolicyManager());
  return base::MakeRefCounted<PolicyService>(std::move(managers));
}

std::string GetTestName() {
  const ::testing::TestInfo* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return test_info ? base::StrCat(
                         {test_info->test_suite_name(), ".", test_info->name()})
                   : "?.?";
}

bool DeleteFileAndEmptyParentDirectories(
    const absl::optional<base::FilePath>& file_path) {
  struct Local {
    // Deletes recursively `dir` and its parents up, if dir is empty
    // and until one non-empty parent directory is found.
    static bool DeleteDirsIfEmpty(const base::FilePath& dir) {
      if (!base::DirectoryExists(dir) || !base::IsDirectoryEmpty(dir))
        return true;
      if (!base::DeleteFile(dir))
        return false;
      return DeleteDirsIfEmpty(dir.DirName());
    }
  };

  if (!file_path || !base::DeleteFile(*file_path))
    return false;
  return Local::DeleteDirsIfEmpty(file_path->DirName());
}

base::FilePath GetLogDestinationDir() {
  const char* var = std::getenv("ISOLATED_OUTDIR");
  return var ? base::FilePath::FromUTF8Unsafe(var) : base::FilePath();
}

void InitLoggingForUnitTest(const base::FilePath& log_base_path) {
  const absl::optional<base::FilePath> log_file_path = [&log_base_path]() {
    const base::FilePath dest_dir = GetLogDestinationDir();
    return dest_dir.empty()
               ? absl::nullopt
               : absl::make_optional(dest_dir.Append(log_base_path));
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
  if (!command_line->HasSwitch(kTestLauncherExcludePathsFromWindowDefender))
    return;

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

  base::Time::Exploded start_time;
  base::Time::Now().LocalExplode(&start_time);
  const base::FilePath pml_file(dest_dir.Append(base::StringPrintf(
      L"%02d%02d%02d-%02d%02d%02d.PML", start_time.year, start_time.month,
      start_time.day_of_month, start_time.hour, start_time.minute,
      start_time.second)));

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

  // TODO(crbug/1396077): Make a copy of the PML file in case the original gets
  // deleted.
  if (!base::CopyFile(pml_file, pml_file.ReplaceExtension(L".PML.BAK")))
    LOG(ERROR) << __func__ << ": failed to backup pml file";
}

const base::ProcessIterator::ProcessEntries FindProcesses(
    const base::FilePath::StringType& executable_name) {
  return base::NamedProcessIterator(executable_name, nullptr).Snapshot();
}

base::FilePath::StringType PrintProcesses(
    const base::FilePath::StringType& executable_name) {
  base::FilePath::StringType message(L"Found processes:\n");
  base::FilePath::StringType demarcation(72, L'=');
  demarcation += L'\n';
  message += demarcation;

  for (const base::ProcessEntry& entry : FindProcesses(executable_name)) {
    message += base::StrCat(
        {entry.exe_file(), L", pid=", base::NumberToWString(entry.pid()),
         L", creation time=",
         [](base::ProcessId pid) {
           const base::Process process = base::Process::Open(pid);
           return process.IsValid() ? base::ASCIIToWide(base::TimeFormatHTTP(
                                          process.CreationTime()))
                                    : L"n/a";
         }(entry.pid()),
         L", cmdline=",
         [](base::ProcessId pid) {
           std::unique_ptr<ProcessInspector> process_inspector =
               ProcessInspector::Create(base::Process::OpenWithAccess(
                   pid, PROCESS_ALL_ACCESS | PROCESS_VM_READ));
           return process_inspector ? process_inspector->command_line()
                                    : L"n/a";
         }(entry.pid()),
         L"\n"});
  }

  return message + demarcation;
}

EventHolder CreateWaitableEventForTest() {
  NamedObjectAttributes attr = GetNamedObjectAttributes(
      base::NumberToWString(::GetCurrentProcessId()).c_str(), GetTestScope());
  return {base::WaitableEvent(base::win::ScopedHandle(
              ::CreateEvent(&attr.sa, FALSE, FALSE, attr.name.c_str()))),
          attr.name};
}

#endif  // BUILDFLAG(IS_WIN)

bool WaitFor(base::RepeatingCallback<bool()> predicate,
             base::RepeatingClosure still_waiting) {
  constexpr base::TimeDelta kOutputInterval = base::Seconds(10);
  auto notify_next = base::TimeTicks::Now() + kOutputInterval;
  const auto deadline = base::TimeTicks::Now() + TestTimeouts::action_timeout();
  while (base::TimeTicks::Now() < deadline) {
    if (predicate.Run()) {
      return true;
    }
    if (notify_next < base::TimeTicks::Now()) {
      still_waiting.Run();
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

}  // namespace updater::test
