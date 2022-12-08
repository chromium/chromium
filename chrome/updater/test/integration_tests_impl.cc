// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests_impl.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/util/util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/win/test/test_executables.h"
#endif

namespace updater::test {
namespace {

constexpr char kSelfUpdateCRXName[] = "updater_selfupdate.crx3";
#if BUILDFLAG(IS_MAC)
constexpr char kSelfUpdateCRXRun[] = PRODUCT_FULLNAME_STRING "_test.app";
constexpr char kDoNothingCRXName[] = "updater_qualification_app_dmg.crx";
constexpr char kDoNothingCRXRun[] = "updater_qualification_app_dmg.dmg";
#elif BUILDFLAG(IS_WIN)
constexpr char kSelfUpdateCRXRun[] = "UpdaterSetup_test.exe";
constexpr char kDoNothingCRXName[] = "updater_qualification_app_exe.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app.exe";
#elif BUILDFLAG(IS_LINUX)
constexpr char kSelfUpdateCRXRun[] = "UpdaterSetup_test";
constexpr char kDoNothingCRXName[] = "updater_qualification_app.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app";
#endif

std::string GetHashHex(const base::FilePath& file) {
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  base::MemoryMappedFile mmfile;
  EXPECT_TRUE(mmfile.Initialize(file));  // Note: This fails with an empty file.
  hasher->Update(mmfile.data(), mmfile.length());
  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  hasher->Finish(actual_hash, sizeof(actual_hash));
  return base::HexEncode(actual_hash, sizeof(actual_hash));
}

std::string GetUpdateResponse(const std::string& app_id,
                              const std::string& install_data_index,
                              const std::string& codebase,
                              const base::Version& version,
                              const base::FilePath& update_file,
                              const std::string& run_action,
                              const std::string& arguments) {
  return base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(%s)"
      R"(      "updatecheck":{)"
      R"(        "status":"ok",)"
      R"(        "urls":{"url":[{"codebase":"%s"}]},)"
      R"(        "manifest":{)"
      R"(          "version":"%s",)"
      R"(          "run":"%s",)"
      R"(          "arguments":"%s",)"
      R"(          "packages":{)"
      R"(            "package":[)"
      R"(              {"name":"%s","hash_sha256":"%s"})"
      R"(            ])"
      R"(          })"
      R"(        })"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str(),
      !install_data_index.empty()
          ? base::StringPrintf(
                R"(     "data":[{ "status":"ok", "name":"install", )"
                R"("index":"%s", "#text":"%s_text" }],)",
                install_data_index.c_str(), install_data_index.c_str())
                .c_str()
          : "",
      codebase.c_str(), version.GetString().c_str(), run_action.c_str(),
      arguments.c_str(), update_file.BaseName().AsUTF8Unsafe().c_str(),
      GetHashHex(update_file).c_str());
}

base::RepeatingCallback<bool(const std::string&)> GetScopePredicate(
    UpdaterScope scope) {
  return base::BindLambdaForTesting([scope](const std::string& request_body) {
    const bool is_match = [&scope, &request_body]() {
      const absl::optional<base::Value> doc =
          base::JSONReader::Read(request_body);
      if (!doc || !doc->is_dict()) {
        return false;
      }
      const base::Value::Dict* object_request =
          doc->GetDict().FindDict("request");
      if (!object_request) {
        return false;
      }
      absl::optional<bool> ismachine = object_request->FindBool("ismachine");
      if (!ismachine.has_value()) {
        return false;
      }
      switch (scope) {
        case UpdaterScope::kSystem:
          return *ismachine;
        case UpdaterScope::kUser:
          return !*ismachine;
      }
    }();
    if (!is_match) {
      ADD_FAILURE() << R"(Request does not match "ismachine": )"
                    << request_body;
    }
    return is_match;
  });
}

void RunUpdaterWithSwitch(const base::Version& version,
                          UpdaterScope scope,
                          const std::string& command,
                          int expected_exit_code) {
  const absl::optional<base::FilePath> installed_executable_path =
      GetVersionedInstallDirectory(scope, version)
          ->Append(GetExecutableRelativePath());
  ASSERT_TRUE(installed_executable_path);
  ASSERT_TRUE(base::PathExists(*installed_executable_path));
  base::CommandLine command_line(*installed_executable_path);
  command_line.AppendSwitch(command);
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, expected_exit_code);
}

}  // namespace

void ExitTestMode(UpdaterScope scope) {
  DeleteFileAndEmptyParentDirectories(GetOverrideFilePath(scope));
}

int CountDirectoryFiles(const base::FilePath& dir) {
  base::FileEnumerator it(dir, false, base::FileEnumerator::FILES);
  int res = 0;
  for (base::FilePath name = it.Next(); !name.empty(); name = it.Next()) {
    ++res;
  }
  return res;
}

void RegisterApp(UpdaterScope scope, const std::string& app_id) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = base::Version("0.1");
  base::RunLoop loop;
  update_service->RegisterApp(registration,
                              base::BindLambdaForTesting([&loop](int result) {
                                EXPECT_EQ(result, 0);
                                loop.Quit();
                              }));
  loop.Run();
}

void SetGroupPolicies(const base::Value::Dict& values) {
  ASSERT_TRUE(ExternalConstantsBuilder().SetGroupPolicies(values).Modify());
}

void ExpectVersionActive(UpdaterScope scope, const std::string& version) {
  scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
  ASSERT_NE(prefs, nullptr) << "Failed to acquire GlobalPrefs.";
  EXPECT_EQ(prefs->GetActiveVersion(), version);
}

void ExpectVersionNotActive(UpdaterScope scope, const std::string& version) {
  scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
  ASSERT_NE(prefs, nullptr) << "Failed to acquire GlobalPrefs.";
  EXPECT_NE(prefs->GetActiveVersion(), version);
}

void Install(UpdaterScope scope) {
  const base::FilePath path = GetSetupExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  command_line.AppendSwitchASCII(kTagSwitch, "usagestats=1");
  int exit_code = -1;
  ASSERT_TRUE(Run(scope, command_line, &exit_code));
  EXPECT_EQ(exit_code, 0);
}

void PrintLog(UpdaterScope scope) {
  std::string contents;
  absl::optional<base::FilePath> path = GetDataDirPath(scope);
  EXPECT_TRUE(path);
  VLOG(0) << "Contents of updater.log for " << GetTestName() << " in "
          << path.value() << ":";
  if (path &&
      base::ReadFileToString(path->AppendASCII("updater.log"), &contents)) {
    const std::string demarcation(72, '=');
    VLOG(0) << demarcation;
    VLOG(0) << contents;
    VLOG(0) << "End contents of updater.log.";
    VLOG(0) << demarcation;
  } else {
    VLOG(0) << "Failed to read updater.log file.";
  }
}

// Copies the updater log file present in `src_dir` to a test-specific directory
// name in Swarming/Isolate. Avoids overwriting the destination log file if
// other instances of it exist in the destination directory. Swarming retries
// each failed test. It is useful to capture a few logs from previous failures
// instead of the log of the last run only.
void CopyLog(const base::FilePath& src_dir) {
  // TODO(crbug.com/1159189): copy other test artifacts.
  base::FilePath dest_dir = GetLogDestinationDir();
  if (!dest_dir.empty() && base::PathExists(dest_dir) &&
      base::PathExists(src_dir)) {
    dest_dir = dest_dir.AppendASCII(GetTestName());
    EXPECT_TRUE(base::CreateDirectory(dest_dir));
    const base::FilePath dest_file_path = [dest_dir]() {
      base::FilePath path = dest_dir.AppendASCII("updater.log");
      for (int i = 1; i < 10 && base::PathExists(path); ++i) {
        path = dest_dir.AppendASCII(base::StringPrintf("updater.%d.log", i));
      }
      return path;
    }();
    const base::FilePath log_path = src_dir.AppendASCII("updater.log");
    VLOG(0) << "Copying updater.log file. From: " << log_path
            << ". To: " << dest_file_path;
    EXPECT_TRUE(base::CopyFile(log_path, dest_file_path));
  }
}

void RunWake(UpdaterScope scope, int expected_exit_code) {
  RunUpdaterWithSwitch(base::Version(kUpdaterVersion), scope, kWakeSwitch,
                       expected_exit_code);
}

void RunWakeAll(UpdaterScope scope) {
  RunUpdaterWithSwitch(base::Version(kUpdaterVersion), scope, kWakeAllSwitch,
                       kErrorOk);
}

void RunWakeActive(UpdaterScope scope, int expected_exit_code) {
  // Find the active version.
  base::Version active_version;
  {
    scoped_refptr<UpdateService> service = CreateUpdateServiceProxy(scope);
    base::RunLoop loop;
    service->GetVersion(base::BindLambdaForTesting(
        [&loop, &active_version](const base::Version& version) {
          active_version = version;
          loop.Quit();
        }));
    loop.Run();
  }
  ASSERT_TRUE(active_version.IsValid());

  // Invoke the wake client of that version.
  RunUpdaterWithSwitch(active_version, scope, kWakeSwitch, expected_exit_code);
}

void Update(UpdaterScope scope,
            const std::string& app_id,
            const std::string& install_data_index) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  base::RunLoop loop;
  update_service->Update(
      app_id, install_data_index, UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, base::DoNothing(),
      base::BindLambdaForTesting(
          [&loop](UpdateService::Result result_unused) { loop.Quit(); }));
  loop.Run();
}

void UpdateAll(UpdaterScope scope) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  base::RunLoop loop;
  update_service->UpdateAll(
      base::DoNothing(),
      base::BindLambdaForTesting(
          [&loop](UpdateService::Result result_unused) { loop.Quit(); }));
  loop.Run();
}

void DeleteUpdaterDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> install_dir = GetBaseInstallDirectory(scope);
  ASSERT_TRUE(install_dir);
  ASSERT_TRUE(base::DeletePathRecursively(*install_dir));
}

void SetupFakeUpdaterPrefs(UpdaterScope scope, const base::Version& version) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  ASSERT_TRUE(global_prefs) << "No global prefs.";
  global_prefs->SetActiveVersion(version.GetString());
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());

  ASSERT_EQ(version.GetString(), global_prefs->GetActiveVersion());
}

void SetupFakeUpdaterInstallFolder(UpdaterScope scope,
                                   const base::Version& version) {
  const absl::optional<base::FilePath> folder_path =
      GetFakeUpdaterInstallFolderPath(scope, version);
  ASSERT_TRUE(folder_path);
  ASSERT_TRUE(base::CreateDirectory(*folder_path));
}

void SetupFakeUpdater(UpdaterScope scope, const base::Version& version) {
  SetupFakeUpdaterPrefs(scope, version);
  SetupFakeUpdaterInstallFolder(scope, version);
}

void SetupFakeUpdaterVersion(UpdaterScope scope, int offset) {
  ASSERT_NE(offset, 0);
  std::vector<uint32_t> components =
      base::Version(kUpdaterVersion).components();
  base::CheckedNumeric<uint32_t> new_version = components[0];
  new_version += offset;
  ASSERT_TRUE(new_version.AssignIfValid(&components[0]));
  SetupFakeUpdater(scope, base::Version(std::move(components)));
}

void SetupFakeUpdaterLowerVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, -1);
}

void SetupFakeUpdaterHigherVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, 1);
}

void SetExistenceCheckerPath(UpdaterScope scope,
                             const std::string& app_id,
                             const base::FilePath& path) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService())
      ->SetExistenceCheckerPath(app_id, path);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
}

void SetServerStarts(UpdaterScope scope, int value) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  for (int i = 0; i <= value; ++i) {
    global_prefs->CountServerStarts();
  }
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
}

void FillLog(UpdaterScope scope) {
  absl::optional<base::FilePath> log = GetLogFilePath(scope);
  ASSERT_TRUE(log);
  std::string data = "This test string is used to fill up log space.\n";
  for (int i = 0; i < 1024 * 1024 * 6; i += data.length()) {
    ASSERT_TRUE(base::AppendToFile(*log, data));
  }
}

void ExpectLogRotated(UpdaterScope scope) {
  absl::optional<base::FilePath> log = GetLogFilePath(scope);
  ASSERT_TRUE(log);
  EXPECT_TRUE(base::PathExists(log->AddExtension(FILE_PATH_LITERAL(".old"))));
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(*log, &size));
  EXPECT_TRUE(size < 1024 * 1024);
}

void ExpectRegistered(UpdaterScope scope, const std::string& app_id) {
  ASSERT_TRUE(base::Contains(base::MakeRefCounted<PersistedData>(
                                 CreateGlobalPrefs(scope)->GetPrefService())
                                 ->GetAppIds(),
                             app_id));
}

void ExpectNotRegistered(UpdaterScope scope, const std::string& app_id) {
  ASSERT_FALSE(base::Contains(base::MakeRefCounted<PersistedData>(
                                  CreateGlobalPrefs(scope)->GetPrefService())
                                  ->GetAppIds(),
                              app_id));
}

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version) {
  const base::Version app_version =
      base::MakeRefCounted<PersistedData>(
          CreateGlobalPrefs(scope)->GetPrefService())
          ->GetProductVersion(app_id);
  EXPECT_TRUE(app_version.IsValid() && version == app_version);
}

bool Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
  command_line.AppendSwitch(kEnableLoggingSwitch);
  command_line.AppendSwitchASCII(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue);
  if (IsSystemInstall(scope)) {
    command_line.AppendSwitch(kSystemSwitch);
    command_line = MakeElevated(command_line);
  }
  VLOG(0) << " Run command: " << command_line.GetCommandLineString();
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid()) {
    return false;
  }

  // macOS requires a larger timeout value for --install.
  return process.WaitForExitWithTimeout(2 * TestTimeouts::action_max_timeout(),
                                        exit_code);
}

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

bool RequestMatcherRegex(const std::string& request_body_regex,
                         const std::string& request_body) {
  if (!re2::RE2::PartialMatch(request_body, request_body_regex)) {
    VLOG(0) << "Request match failed.";
    ADD_FAILURE() << "Request with body: " << request_body
                  << " did not match expected regex " << request_body_regex;
    return false;
  }
  return true;
}

void ExpectSelfUpdateSequence(UpdaterScope scope, ScopedServer* test_server) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_data_path));
  base::FilePath crx_path = test_data_path.AppendASCII(kSelfUpdateCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"appid":"%s".*)", kUpdaterAppId)),
       GetScopePredicate(scope)},
      GetUpdateResponse(
          kUpdaterAppId, "", test_server->base_url().spec(),
          base::Version(kUpdaterVersion), crx_path, kSelfUpdateCRXRun,
          base::StrCat({"--update", IsSystemInstall(scope) ? " --system" : "",
                        " --", kEnableLoggingSwitch, " --",
                        kLoggingModuleSwitch, "=",
                        kLoggingModuleSwitchValue})));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce({base::BindRepeating(RequestMatcherRegex, "")},
                          crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"eventresult":1,"eventtype":3,)"
                              R"("nextversion":"%s",.*)",
                              kUpdaterVersion)),
       GetScopePredicate(scope)},
      ")]}'\n");
}

void ExpectUpdateSequence(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& app_id,
                          const std::string& install_data_index,
                          const base::Version& from_version,
                          const base::Version& to_version) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"appid":"%s".*)", app_id.c_str())),
       base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(
               R"(.*%s)",
               !install_data_index.empty()
                   ? base::StringPrintf(
                         R"("data":\[{"index":"%s","name":"install"}],.*)",
                         install_data_index.c_str())
                         .c_str()
                   : "")),
       GetScopePredicate(scope)},
      GetUpdateResponse(app_id, install_data_index,
                        test_server->base_url().spec(), to_version, crx_path,
                        kDoNothingCRXRun, {}));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce({base::BindRepeating(RequestMatcherRegex, "")},
                          crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce(
      {base::BindRepeating(
           RequestMatcherRegex,
           base::StringPrintf(R"(.*"eventresult":1,"eventtype":3,)"
                              R"("nextversion":"%s","previousversion":"%s".*)",
                              to_version.GetString().c_str(),
                              from_version.GetString().c_str())),
       GetScopePredicate(scope)},
      ")]}'\n");
}

// Runs multiple cycles of instantiating the update service, calling
// `GetVersion()`, then releasing the service interface.
void StressUpdateService(UpdaterScope scope) {
  base::RunLoop loop;

  // Number of times to run the cycle of instantiating the service.
  int n = 10;

  // Delay in milliseconds between successive cycles.
  const int kDelayBetweenLoopsMS = 0;

  // Runs on the main sequence.
  auto loop_closure = [&]() {
    LOG(ERROR) << __func__ << ": n: " << n;
    if (--n) {
      return false;
    }
    loop.Quit();
    return true;
  };

  // Creates a task runner, and runs the service instance on it.
  using LoopClosure = decltype(loop_closure);
  auto stress_runner = [scope, loop_closure]() {
    // `task_runner` is always bound on the main sequence.
    struct Local {
      static void GetVersion(
          UpdaterScope scope,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          LoopClosure loop_closure) {
        auto service_task_runner =
            base::ThreadPool::CreateSingleThreadTaskRunner(
                {}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
        service_task_runner->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting([scope, task_runner, loop_closure]() {
              auto update_service = CreateUpdateServiceProxy(scope);
              update_service->GetVersion(
                  base::BindOnce(GetVersionCallback, scope, update_service,
                                 task_runner, loop_closure));
            }),
            base::Milliseconds(kDelayBetweenLoopsMS));
      }

      static void GetVersionCallback(
          UpdaterScope scope,
          scoped_refptr<UpdateService> /*update_service*/,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          LoopClosure loop_closure,
          const base::Version& version) {
        EXPECT_EQ(version, base::Version(kUpdaterVersion));
        task_runner->PostTask(
            FROM_HERE,
            base::BindLambdaForTesting([scope, task_runner, loop_closure]() {
              if (loop_closure()) {
                return;
              }
              GetVersion(scope, task_runner, loop_closure);
            }));
      }
    };

    Local::GetVersion(scope, base::SequencedTaskRunner::GetCurrentDefault(),
                      loop_closure);
  };

  stress_runner();
  loop.Run();
}

void CallServiceUpdate(UpdaterScope updater_scope,
                       const std::string& app_id,
                       const std::string& install_data_index,
                       bool same_version_update_allowed) {
  UpdateService::PolicySameVersionUpdate policy_same_version_update =
      same_version_update_allowed
          ? UpdateService::PolicySameVersionUpdate::kAllowed
          : UpdateService::PolicySameVersionUpdate::kNotAllowed;

  scoped_refptr<UpdateService> service_proxy =
      CreateUpdateServiceProxy(updater_scope);

  base::RunLoop loop;
  service_proxy->Update(
      app_id, install_data_index, UpdateService::Priority::kForeground,
      policy_same_version_update,
      base::BindLambdaForTesting([](const UpdateService::UpdateState&) {}),
      base::BindLambdaForTesting([&](UpdateService::Result result) {
        EXPECT_EQ(result, UpdateService::Result::kSuccess);
        loop.Quit();
      }));

  loop.Run();
}

void RunRecoveryComponent(UpdaterScope scope,
                          const std::string& app_id,
                          const base::Version& version) {
  base::CommandLine command(GetSetupExecutablePath());
  command.AppendSwitchASCII(kBrowserVersionSwitch, version.GetString());
  command.AppendSwitchASCII(kAppGuidSwitch, app_id);
  int exit_code = -1;
  EXPECT_TRUE(Run(scope, command, &exit_code));
  EXPECT_EQ(exit_code, kErrorOk);
}

void ExpectLastChecked(UpdaterScope updater_scope) {
  EXPECT_FALSE(base::MakeRefCounted<PersistedData>(
                   CreateGlobalPrefs(updater_scope)->GetPrefService())
                   ->GetLastChecked()
                   .is_null());
}

void ExpectLastStarted(UpdaterScope updater_scope) {
  EXPECT_FALSE(base::MakeRefCounted<PersistedData>(
                   CreateGlobalPrefs(updater_scope)->GetPrefService())
                   ->GetLastStarted()
                   .is_null());
}

std::set<base::FilePath::StringType> GetTestProcessNames() {
#if BUILDFLAG(IS_MAC)
  return {
      GetExecutableRelativePath().BaseName().value(),
      GetSetupExecutablePath().BaseName().value(),
  };
#elif BUILDFLAG(IS_WIN)
  return {
      GetExecutableRelativePath().BaseName().value(),
      GetSetupExecutablePath().BaseName().value(),
      kTestProcessExecutableName,
      []() {
        const base::FilePath test_executable =
            base::FilePath::FromASCII(kExecutableName).BaseName();
        return base::StrCat({test_executable.RemoveExtension().value(),
                             base::ASCIIToWide(kExecutableSuffix),
                             test_executable.Extension()});
      }(),
  };
#else
  return {GetExecutableRelativePath().BaseName().value()};
#endif
}

void CleanProcesses() {
  for (const base::FilePath::StringType& process_name : GetTestProcessNames()) {
    EXPECT_TRUE(KillProcesses(process_name, -1)) << process_name;
    EXPECT_TRUE(
        WaitForProcessesToExit(process_name, TestTimeouts::action_timeout()))
        << process_name;
    EXPECT_FALSE(IsProcessRunning(process_name)) << process_name;
  }
}

void ExpectCleanProcesses() {
  for (const base::FilePath::StringType& process_name : GetTestProcessNames()) {
    EXPECT_FALSE(IsProcessRunning(process_name)) << process_name;
  }
}

}  // namespace updater::test
