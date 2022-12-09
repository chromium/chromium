// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/unittest_util.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/win/windows_version.h"
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
  return base::KillProcesses(executable_name, exit_code, nullptr);
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

absl::optional<base::FilePath> GetOverrideFilePath(UpdaterScope scope) {
  const absl::optional<base::FilePath> data_dir = GetBaseDataDirectory(scope);
  return data_dir
             ? absl::make_optional(data_dir->AppendASCII(kDevOverrideFileName))
             : absl::nullopt;
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

void InitLoggingForUnitTest() {
  base::FilePath file_exe;
  if (!base::PathService::Get(base::FILE_EXE, &file_exe)) {
    return;
  }
  const absl::optional<base::FilePath> log_file_path =
      [](const base::FilePath& file_exe) {
        const base::FilePath dest_dir = GetLogDestinationDir();
        return dest_dir.empty() ? absl::nullopt
                                : absl::make_optional(dest_dir.Append(
                                      file_exe.BaseName().ReplaceExtension(
                                          FILE_PATH_LITERAL("log"))));
      }(file_exe);
  if (log_file_path) {
    logging::LoggingSettings settings;
    settings.log_file_path = (*log_file_path).value().c_str();
    settings.logging_dest = logging::LOG_TO_ALL;
    logging::InitLogging(settings);
    VLOG(0) << "Log initialized for " << file_exe.value() << " -> "
            << settings.log_file_path;
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

  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    VLOG(1) << "Skip changing Windows Defender settings for Win7 and below.";
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
  if (base::win::GetVersion() <= base::win::Version::WIN7) {
    LOG(WARNING) << __func__ << ": skipping procmon logging on Win7.";
    return {};
  }

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

  base::FilePath source_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  const base::FilePath pmc_path(source_path.Append(L"chrome")
                                    .Append(L"updater")
                                    .Append(L"test")
                                    .Append(L"data")
                                    .Append(L"ProcmonConfiguration.pmc"));
  CHECK(base::PathExists(pmc_path));

  base::Time::Exploded start_time;
  base::Time::Now().LocalExplode(&start_time);
  const base::FilePath pml_file(dest_dir.Append(base::StringPrintf(
      L"%02d%02d%02d-%02d%02d%02d.PML", start_time.year, start_time.month,
      start_time.day_of_month, start_time.hour, start_time.minute,
      start_time.second)));

  const std::wstring& cmdline = base::StrCat(
      {kProcmonPath, L" /AcceptEula /LoadConfig \"", pmc_path.value(),
       L"\" /BackingFile \"", pml_file.value(), L"\" /Quiet /externalcapture"});
  base::LaunchOptions options;
  options.start_hidden = true;
  VLOG(1) << __func__ << ": running: " << cmdline;
  const base::Process process = base::LaunchProcess(cmdline, options);

  if (!process.IsValid()) {
    LOG(ERROR) << __func__ << ": failed to run: " << cmdline;
    return {};
  }

  return pml_file;
}

void StopProcmonLogging(const base::FilePath& pml_file) {
  if (!::IsUserAnAdmin() || !base::PathExists(base::FilePath(kProcmonPath)) ||
      !pml_file.MatchesFinalExtension(L".PML")) {
    return;
  }

  for (const std::wstring& cmdline :
       {base::StrCat({kProcmonPath, L" /Terminate"}),
        base::StrCat({kProcmonPath, L" /AcceptEula /OpenLog \"",
                      pml_file.value(), L"\" /SaveAs \"",
                      pml_file.ReplaceExtension(L".CSV").value(), L"\""})}) {
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

#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater::test
