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

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/request_matcher.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/win_constants.h"
#elif BUILDFLAG(IS_LINUX)
#include "chrome/updater/util/linux_util.h"
#include "chrome/updater/util/posix_util.h"
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
constexpr char kSelfUpdateCRXRun[] = "updater_test";
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

std::string GetUpdateResponseForApp(
    const std::string& app_id,
    const std::string& install_data_index,
    const std::string& codebase,
    const base::Version& version,
    const base::FilePath& update_file,
    const std::string& run_action,
    const std::string& arguments,
    const absl::optional<std::string>& file_hash = absl::nullopt) {
  return base::StringPrintf(
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
      R"(    })",
      base::ToLowerASCII(app_id).c_str(),
      !install_data_index.empty()
          ? base::StringPrintf(
                R"(     "data":[{ "status":"ok", "name":"install", )"
                R"("index":"%s", "#text":"%s_text" }],)",
                install_data_index.c_str(), install_data_index.c_str())
                .c_str()
          : "",
      codebase.c_str(), version.GetString().c_str(), run_action.c_str(),
      arguments.c_str(), update_file.BaseName().AsUTF8Unsafe().c_str(),
      file_hash ? file_hash->c_str() : GetHashHex(update_file).c_str());
}

std::string GetUpdateResponse(const std::vector<std::string>& app_responses) {
  return base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(%s)"
      R"(  ])"
      R"(}})",
      base::JoinString(app_responses, ",\n").c_str());
}

std::string GetUpdateResponse(const std::string& app_id,
                              const std::string& install_data_index,
                              const std::string& codebase,
                              const base::Version& version,
                              const base::FilePath& update_file,
                              const std::string& run_action,
                              const std::string& arguments,
                              const std::string& file_hash) {
  return GetUpdateResponse(
      {GetUpdateResponseForApp(app_id, install_data_index, codebase, version,
                               update_file, run_action, arguments, file_hash)
           .c_str()});
}

std::string GetUpdateResponse(const std::string& app_id,
                              const std::string& install_data_index,
                              const std::string& codebase,
                              const base::Version& version,
                              const base::FilePath& update_file,
                              const std::string& run_action,
                              const std::string& arguments) {
  return GetUpdateResponse(app_id, install_data_index, codebase, version,
                           update_file, run_action, arguments,
                           GetHashHex(update_file));
}

void RunUpdaterWithSwitch(const base::Version& version,
                          UpdaterScope scope,
                          const std::string& command,
                          absl::optional<int> expected_exit_code) {
  const absl::optional<base::FilePath> installed_executable_path =
      GetVersionedInstallDirectory(scope, version)
          ->Append(GetExecutableRelativePath());
  ASSERT_TRUE(installed_executable_path);
  ASSERT_TRUE(base::PathExists(*installed_executable_path));
  base::CommandLine command_line(*installed_executable_path);
  command_line.AppendSwitch(command);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  if (expected_exit_code) {
    ASSERT_EQ(exit_code, expected_exit_code.value());
  }
}

void ExpectUpdateCheckSequence(UpdaterScope scope,
                               ScopedServer* test_server,
                               const std::string& app_id,
                               UpdateService::Priority priority,
                               int event_type,
                               const base::Version& from_version,
                               const base::Version& to_version) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {request::GetPathMatcher(test_server->update_path()),
       request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {base::StringPrintf(R"(.*"appid":"%s".*)", app_id.c_str())}),
       request::GetScopeMatcher(scope),
       request::GetAppPriorityMatcher(app_id, priority)},
      GetUpdateResponse(app_id, "", test_server->update_url().spec(),
                        to_version, crx_path, kDoNothingCRXRun, {}));

  // Second request: event ping with an error because the update check response
  // is ignored by the client:
  // {errorCategory::kService, ServiceError::CHECK_FOR_UPDATE_ONLY}
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher({base::StringPrintf(
                               R"(.*"errorcat":4,"errorcode":4,)"
                               R"("eventresult":0,"eventtype":%d,)"
                               R"("nextversion":"%s","previousversion":"%s".*)",
                               event_type, to_version.GetString().c_str(),
                               from_version.GetString().c_str())}),
                           request::GetScopeMatcher(scope)},
                          ")]}'\n");
}

void ExpectUpdateSequence(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& app_id,
                          const std::string& install_data_index,
                          UpdateService::Priority priority,
                          int event_type,
                          const base::Version& from_version,
                          const base::Version& to_version) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {request::GetPathMatcher(test_server->update_path()),
       request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {base::StringPrintf(R"("appid":"%s")", app_id.c_str()),
            install_data_index.empty()
                ? ""
                : base::StringPrintf(
                      R"("data":\[{"index":"%s","name":"install"}],.*)",
                      install_data_index.c_str())
                      .c_str()}),
       request::GetScopeMatcher(scope),
       request::GetAppPriorityMatcher(app_id, priority)},
      GetUpdateResponse(app_id, install_data_index,
                        test_server->update_url().spec(), to_version, crx_path,
                        kDoNothingCRXRun, {}));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(), request::GetContentMatcher({""})},
      crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher({base::StringPrintf(
                               R"(.*"eventresult":1,"eventtype":%d,)"
                               R"("nextversion":"%s","previousversion":"%s".*)",
                               event_type, to_version.GetString().c_str(),
                               from_version.GetString().c_str())}),
                           request::GetScopeMatcher(scope)},
                          ")]}'\n");
}

void ExpectDeviceManagementRequest(ScopedServer* test_server,
                                   const std::string& request_type,
                                   const std::string& authorization_type,
                                   const std::string& authorization_token,
                                   const std::string& response) {
  test_server->ExpectOnce(
      {request::GetPathMatcher(base::StringPrintf(
           R"(%s\?request=%s&apptype=Chrome&)"
           R"(agent=%s\+%s&platform=.*&deviceid=%s)",
           test_server->device_management_path().c_str(), request_type.c_str(),
           PRODUCT_FULLNAME_STRING, kUpdaterVersion,
           GetDefaultDMStorage()->GetDeviceID().c_str())),
       request::GetUpdaterUserAgentMatcher(),
       request::GetHeaderMatcher(
           "Authorization",
           base::StringPrintf("%s token=%s", authorization_type.c_str(),
                              authorization_token.c_str())),
       request::GetHeaderMatcher("Content-Type", "application/x-protobuf")},
      response);
}

}  // namespace

AppUpdateExpectation::AppUpdateExpectation(
    const std::string& args,
    const std::string& app_id,
    const base::Version& from_version,
    const base::Version& to_version,
    bool is_install,
    bool should_update,
    bool allow_rollback,
    const std::string& target_version_prefix,
    const std::string& target_channel,
    const base::FilePath& crx_relative_path,
    bool always_serve_crx,
    const UpdateService::ErrorCategory error_category,
    const int error_code,
    const int event_type)
    : args(args),
      app_id(app_id),
      from_version(from_version),
      to_version(to_version),
      is_install(is_install),
      should_update(should_update),
      allow_rollback(allow_rollback),
      target_version_prefix(target_version_prefix),
      target_channel(target_channel),
      crx_relative_path(crx_relative_path),
      always_serve_crx(always_serve_crx),
      error_category(error_category),
      error_code(error_code),
      event_type(event_type) {}
AppUpdateExpectation::AppUpdateExpectation(const AppUpdateExpectation&) =
    default;
AppUpdateExpectation::~AppUpdateExpectation() = default;

void ExitTestMode(UpdaterScope scope) {
  DeleteFileAndEmptyParentDirectories(GetOverrideFilePath(scope));
}

int CountDirectoryFiles(const base::FilePath& dir) {
  int res = 0;
  base::FileEnumerator(dir, false, base::FileEnumerator::FILES)
      .ForEach([&res](const base::FilePath& /*name*/) { ++res; });
  return res;
}

void RegisterApp(UpdaterScope scope,
                 const std::string& app_id,
                 const base::Version& version) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  RegistrationRequest registration;
  registration.app_id = app_id;
  registration.version = version;
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

void SetMachineManaged(bool is_managed_device) {
  ASSERT_TRUE(
      ExternalConstantsBuilder().SetMachineManaged(is_managed_device).Modify());
}

void ExpectVersionActive(UpdaterScope scope, const std::string& version) {
  scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
  ASSERT_NE(prefs, nullptr) << "Failed to acquire GlobalPrefs.";
  EXPECT_EQ(prefs->GetActiveVersion(), version);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(version, [scope]() {
    std::wstring version;
    EXPECT_EQ(base::win::RegKey(UpdaterScopeToHKeyRoot(scope), UPDATER_KEY,
                                Wow6432(KEY_READ))
                  .ReadValue(kRegValueVersion, &version),
              ERROR_SUCCESS);
    return base::WideToASCII(version);
  }());
#endif  // IS_WIN
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
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, 0);
}

void InstallUpdaterAndApp(UpdaterScope scope,
                          const std::string& app_id,
                          const bool is_silent_install,
                          const std::string& tag,
                          const std::string& child_window_text_to_find) {
  const base::FilePath path = GetSetupExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitch(kInstallSwitch);
  command_line.AppendSwitchASCII(kTagSwitch, tag);
  command_line.AppendSwitchASCII(kAppIdSwitch, app_id);
  if (is_silent_install) {
    ASSERT_TRUE(child_window_text_to_find.empty());
    command_line.AppendSwitch(kSilentSwitch);
  }

  if (child_window_text_to_find.empty()) {
    int exit_code = -1;
    Run(scope, command_line, &exit_code);
    ASSERT_EQ(exit_code, 0);
  } else {
#if BUILDFLAG(IS_WIN)
    Run(scope, command_line, nullptr);
    CloseInstallCompleteDialog(base::ASCIIToWide(child_window_text_to_find));
#else
    NOTREACHED();
#endif
  }
}

void PrintLog(UpdaterScope scope) {
  std::string contents;
  absl::optional<base::FilePath> path = GetInstallDirectory(scope);
  EXPECT_TRUE(path);
  if (path &&
      base::ReadFileToString(path->AppendASCII("updater.log"), &contents)) {
    VLOG(0) << "Contents of updater.log for " << GetTestName() << " in "
            << path.value() << ":";
    const std::string demarcation(72, '=');
    VLOG(0) << demarcation;
    VLOG(0) << contents;
    VLOG(0) << "End contents of updater.log for " << GetTestName() << ".";
    VLOG(0) << demarcation;
  } else {
    VLOG(0) << "No updater.log at " << path.value() << " for " << GetTestName();
  }
}

// Copies the updater log file present in `src_dir` to a test-specific directory
// name in Swarming/Isolate. Avoids overwriting the destination log file if
// other instances of it exist in the destination directory. Swarming retries
// each failed test. It is useful to capture a few logs from previous failures
// instead of the log of the last run only.
void CopyLog(const base::FilePath& src_dir) {
  base::FilePath dest_dir = GetLogDestinationDir();
  const base::FilePath log_path = src_dir.AppendASCII("updater.log");
  if (!dest_dir.empty() && base::PathExists(dest_dir) &&
      base::PathExists(log_path)) {
    dest_dir = dest_dir.AppendASCII(GetTestName());
    EXPECT_TRUE(base::CreateDirectory(dest_dir));
    const base::FilePath dest_file_path = [dest_dir]() {
      base::FilePath path = dest_dir.AppendASCII("updater.log");
      for (int i = 1; i < 10 && base::PathExists(path); ++i) {
        path = dest_dir.AppendASCII(base::StringPrintf("updater.%d.log", i));
      }
      return path;
    }();
    VLOG(0) << "Copying updater.log file. From: " << log_path
            << ". To: " << dest_file_path;
    EXPECT_TRUE(base::CopyFile(log_path, dest_file_path));
  }
}

void ExpectNoCrashes(UpdaterScope scope) {
  absl::optional<base::FilePath> database_path(GetCrashDatabasePath(scope));
  if (!database_path || !base::PathExists(*database_path)) {
    return;
  }

  base::FilePath dest_dir = GetLogDestinationDir();
  if (dest_dir.empty()) {
    VLOG(2) << "No log destination folder, skip copying possible crash dumps.";
    return;
  }
  dest_dir = dest_dir.AppendASCII(GetTestName());
  EXPECT_TRUE(base::CreateDirectory(dest_dir));

  int count = 0;
  base::FileEnumerator(*database_path, true, base::FileEnumerator::FILES,
                       FILE_PATH_LITERAL("*.dmp"),
                       base::FileEnumerator::FolderSearchPolicy::ALL)
      .ForEach([&count, &dest_dir](const base::FilePath& name) {
        VLOG(0) << __func__ << "Copying " << name << " to: " << dest_dir;
        EXPECT_TRUE(base::CopyFile(name, dest_dir.Append(name.BaseName())));

        ++count;
      });

  EXPECT_EQ(count, 0) << ": " << count << " crashes found";
}

void ExpectAppsUpdateSequence(UpdaterScope scope,
                              ScopedServer* test_server,
                              const base::Value::Dict& request_attributes,
                              const std::vector<AppUpdateExpectation>& apps) {
#if BUILDFLAG(IS_WIN)
  const base::FilePath::StringType kExeExtension = FILE_PATH_LITERAL(".exe");
#else
  const base::FilePath::StringType kExeExtension = FILE_PATH_LITERAL(".zip");
#endif  // BUILDFLAG(IS_WIN)

  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));

  // First request: update check.
  std::vector<std::string> attributes;
  for (const auto [key, value] : request_attributes) {
    attributes.push_back(base::StringPrintf(R"("%s":"%s")", key.c_str(),
                                            value.GetString().c_str()));
  }
  std::vector<std::string> app_requests;
  std::vector<std::string> app_responses;
  for (const AppUpdateExpectation& app : apps) {
    app_requests.push_back(
        base::StringPrintf(R"("appid":"%s")", app.app_id.c_str()));
    if (!app.target_channel.empty()) {
      app_requests.push_back(base::StringPrintf(R"("release_channel":"%s",)",
                                                app.target_channel.c_str()));
    }
    const base::FilePath crx_path = exe_path.Append(app.crx_relative_path);
    const base::FilePath base_name = crx_path.BaseName().RemoveExtension();
    const base::FilePath run_action =
        base_name.Extension().empty() ? base_name.AddExtension(kExeExtension)
                                      : base_name;
    app_responses.push_back(GetUpdateResponseForApp(
        app.app_id, "", test_server->update_url().spec(), app.to_version,
        crx_path, run_action.MaybeAsASCII().c_str(), app.args));
  }
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher(attributes),
                           request::GetContentMatcher(app_requests),
                           request::GetScopeMatcher(scope)},
                          GetUpdateResponse(app_responses));

  for (const AppUpdateExpectation& app : apps) {
    if (app.should_update || app.always_serve_crx) {
      // Download requests for apps that install/update
      const base::FilePath crx_path = exe_path.Append(app.crx_relative_path);
      ASSERT_TRUE(base::PathExists(crx_path));
      std::string crx_bytes;
      base::ReadFileToString(crx_path, &crx_bytes);
      test_server->ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                               request::GetContentMatcher({""})},
                              crx_bytes);
    }

    if (app.should_update) {
      // Followed by event ping.
      test_server->ExpectOnce(
          {request::GetPathMatcher(test_server->update_path()),
           request::GetUpdaterUserAgentMatcher(),
           request::GetContentMatcher({base::StringPrintf(
               R"(.*"appid":"%s",.*)"
               R"("eventresult":1,"eventtype":%d,)"
               R"("nextversion":"%s","previousversion":"%s".*)"
               R"("version":"%s".*)",
               app.app_id.c_str(), app.is_install ? 2 : 3,
               app.to_version.GetString().c_str(),
               app.from_version.GetString().c_str(),
               app.to_version.GetString().c_str())})},
          ")]}'\n");
    } else {
      // Event ping for apps that doesn't update.
      test_server->ExpectOnce(
          {request::GetPathMatcher(test_server->update_path()),
           request::GetUpdaterUserAgentMatcher(),
           request::GetContentMatcher({base::StringPrintf(
               R"(.*"appid":"%s",.*)"
               R"(.*"errorcat":%d,"errorcode":%d,)"
               R"("eventresult":0,"eventtype":%d,)"
               R"("nextversion":"%s","previousversion":"%s".*)"
               R"("version":"%s".*)",
               app.app_id.c_str(), static_cast<int>(app.error_category),
               app.error_code, app.event_type,
               app.to_version.GetString().c_str(),
               app.from_version.GetString().c_str(),
               app.from_version.GetString().c_str())})},
          ")]}'\n");
    }
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
    scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
    ASSERT_NE(prefs, nullptr) << "Failed to acquire GlobalPrefs.";
    active_version = base::Version(prefs->GetActiveVersion());
  }
  ASSERT_TRUE(active_version.IsValid());

  // Invoke the wake client of that version.
  RunUpdaterWithSwitch(active_version, scope, kWakeSwitch, expected_exit_code);
}

void RunCrashMe(UpdaterScope scope) {
  RunUpdaterWithSwitch(base::Version(kUpdaterVersion), scope, kCrashMeSwitch,
                       absl::nullopt);
}

void RunServer(UpdaterScope scope, int expected_exit_code, bool internal) {
  const absl::optional<base::FilePath> installed_executable_path =
      GetVersionedInstallDirectory(scope, base::Version(kUpdaterVersion))
          ->Append(GetExecutableRelativePath());
  ASSERT_TRUE(installed_executable_path);
  ASSERT_TRUE(base::PathExists(*installed_executable_path));
  base::CommandLine command_line(*installed_executable_path);
  command_line.AppendSwitch(kServerSwitch);
  command_line.AppendSwitchASCII(
      kServerServiceSwitch, internal ? kServerUpdateServiceInternalSwitchValue
                                     : kServerUpdateServiceSwitchValue);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, expected_exit_code);
}

void CheckForUpdate(UpdaterScope scope, const std::string& app_id) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  base::RunLoop loop;
  update_service->CheckForUpdate(
      app_id, UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, base::DoNothing(),
      base::BindLambdaForTesting(
          [&loop](UpdateService::Result result_unused) { loop.Quit(); }));
  loop.Run();
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

void InstallAppViaService(UpdaterScope scope,
                          const std::string& appid,
                          const base::Value::Dict& expected_final_values) {
  RegistrationRequest registration;
  registration.app_id = appid;
  registration.version = base::Version({0, 0, 0, 0});
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  UpdateService::UpdateState final_update_state;
  UpdateService::Result final_result;
  base::RunLoop loop;
  update_service->Install(
      registration, /*client_install_data=*/"", /*install_data_index=*/"",
      UpdateService::Priority::kForeground,
      base::BindLambdaForTesting(
          [&](const UpdateService::UpdateState& update_state) {
            final_update_state = update_state;
          }),
      base::BindLambdaForTesting([&](UpdateService::Result result) {
        final_result = result;
        loop.Quit();
      }));
  loop.Run();

  const base::Value::Dict* expected_update_state =
      expected_final_values.FindDict("expected_update_state");
  if (expected_update_state) {
#define CHECK_STATE_MEMBER_STRING(p)                 \
  if (const std::string* _state_member =             \
          expected_update_state->FindString(#p);     \
      _state_member) {                               \
    EXPECT_EQ(final_update_state.p, *_state_member); \
  }
#define CHECK_STATE_MEMBER_INT(p)                                      \
  if (const absl::optional<int> _state_member =                        \
          expected_update_state->FindInt(#p);                          \
      _state_member) {                                                 \
    EXPECT_EQ(static_cast<int>(final_update_state.p), *_state_member); \
  }
#define CHECK_STATE_MEMBER_VERSION(p)                            \
  if (const std::string* _state_member =                         \
          expected_update_state->FindString(#p);                 \
      _state_member) {                                           \
    EXPECT_EQ(final_update_state.p.GetString(), *_state_member); \
  }

    CHECK_STATE_MEMBER_STRING(app_id);
    CHECK_STATE_MEMBER_INT(state);
    CHECK_STATE_MEMBER_VERSION(next_version);
    CHECK_STATE_MEMBER_INT(downloaded_bytes);
    CHECK_STATE_MEMBER_INT(total_bytes);
    CHECK_STATE_MEMBER_INT(install_progress);
    CHECK_STATE_MEMBER_INT(error_category);
    CHECK_STATE_MEMBER_INT(error_code);
    CHECK_STATE_MEMBER_INT(extra_code1);
    CHECK_STATE_MEMBER_STRING(installer_text);
    CHECK_STATE_MEMBER_STRING(installer_cmd_line);

#undef CHECK_STATE_MEMBER_VERSION
#undef CHECK_STATE_MEMBER_INT
#undef CHECK_STATE_MEMBER_STRING
  }

  if (const absl::optional<int> expected_result =
          expected_final_values.FindInt("expected_result");
      expected_result) {
    EXPECT_EQ(static_cast<int>(final_result), *expected_result);
  }
}

void GetAppStates(UpdaterScope updater_scope,
                  const base::Value::Dict& expected_app_states) {
  scoped_refptr<UpdateService> update_service =
      CreateUpdateServiceProxy(updater_scope);

  base::RunLoop loop;
  update_service->GetAppStates(base::BindLambdaForTesting(
      [&expected_app_states,
       &loop](const std::vector<updater::UpdateService::AppState>& states) {
        for (const auto [expected_app_id, expected_state] :
             expected_app_states) {
          const auto& it = base::ranges::find_if(
              states, [&expected_app_id](const auto& state) {
                return base::EqualsCaseInsensitiveASCII(state.app_id,
                                                        expected_app_id);
              });
          ASSERT_TRUE(it != std::end(states));
          const base::Value::Dict* expected = expected_state.GetIfDict();
          ASSERT_TRUE(expected);
          EXPECT_EQ(it->app_id, *expected->FindString("app_id"));
          EXPECT_EQ(it->version.GetString(), *expected->FindString("version"));
          EXPECT_EQ(it->ap, *expected->FindString("ap"));
          EXPECT_EQ(it->brand_code, *expected->FindString("brand_code"));
#if BUILDFLAG(IS_WIN)
          EXPECT_EQ(base::WideToASCII(it->brand_path.value()),
                    *expected->FindString("brand_path"));
          EXPECT_EQ(base::WideToASCII(it->ecp.value()),
                    *expected->FindString("ecp"));
#else
          EXPECT_EQ(it->brand_path.value(),
                    *expected->FindString("brand_path"));
          EXPECT_EQ(it->ecp.value(), *expected->FindString("ecp"));
#endif  // BUILDFLAG(IS_WIN)
        }
        loop.Quit();
      }));

  loop.Run();
}

void DeleteUpdaterDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  ASSERT_TRUE(install_dir);
  ASSERT_TRUE(base::DeletePathRecursively(*install_dir));
}

void DeleteActiveUpdaterExecutable(UpdaterScope scope) {
  base::Version active_version;
  {
    scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
    ASSERT_TRUE(global_prefs) << "No global prefs.";
    active_version = base::Version(global_prefs->GetActiveVersion());
    ASSERT_TRUE(active_version.IsValid()) << "No active updater.";
  }

  absl::optional<base::FilePath> exe_path =
      GetUpdaterExecutablePath(scope, active_version);
  ASSERT_TRUE(exe_path.has_value())
      << "No path for active updater. Version: " << active_version;
  DeleteFile(*exe_path);
#if BUILDFLAG(IS_LINUX)
  // On Linux, a qualified service makes a full copy of itself, so we have to
  // delete the copy that systemd uses too.
  absl::optional<base::FilePath> launcher_path =
      GetUpdateServiceLauncherPath(GetTestScope());
  ASSERT_TRUE(launcher_path.has_value()) << "No launcher path.";
  DeleteFile(*launcher_path);
#endif  // BUILDFLAG(IS_LINUX)

  // The broken updater should still be active. Tests using this method will
  // probably not test the scenario they expect to test if it's not.
  ExpectVersionActive(scope, active_version.GetString());
}

void DeleteFile(UpdaterScope /*scope*/, const base::FilePath& path) {
  ASSERT_TRUE(base::DeleteFile(path)) << "Can't delete " << path;
}

void SetupFakeUpdaterLowerVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, base::Version("100.0.0.0"),
                          /*major_version_offset=*/0,
                          /*should_create_updater_executable=*/false);
}

void SetupFakeUpdaterHigherVersion(UpdaterScope scope) {
  SetupFakeUpdaterVersion(scope, base::Version(kUpdaterVersion),
                          /*major_version_offset=*/1,
                          /*should_create_updater_executable=*/false);
}

void SetExistenceCheckerPath(UpdaterScope scope,
                             const std::string& app_id,
                             const base::FilePath& path) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  base::MakeRefCounted<PersistedData>(scope, global_prefs->GetPrefService())
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
  for (int i = 0; i < 1024 * 1024 * 3; i += data.length()) {
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
  ASSERT_TRUE(
      base::Contains(base::MakeRefCounted<PersistedData>(
                         scope, CreateGlobalPrefs(scope)->GetPrefService())
                         ->GetAppIds(),
                     app_id));
}

void ExpectNotRegistered(UpdaterScope scope, const std::string& app_id) {
  ASSERT_FALSE(
      base::Contains(base::MakeRefCounted<PersistedData>(
                         scope, CreateGlobalPrefs(scope)->GetPrefService())
                         ->GetAppIds(),
                     app_id));
}

void ExpectAppTag(UpdaterScope scope,
                  const std::string& app_id,
                  const std::string& tag) {
  EXPECT_EQ(tag, base::MakeRefCounted<PersistedData>(
                     scope, CreateGlobalPrefs(scope)->GetPrefService())
                     ->GetAP(app_id));
}

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version) {
  const base::Version app_version =
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefs(scope)->GetPrefService())
          ->GetProductVersion(app_id);
  EXPECT_TRUE(app_version.IsValid());
  EXPECT_EQ(version, app_version);
}

void Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code) {
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
  VPLOG_IF(0, !process.IsValid());
  ASSERT_TRUE(process.IsValid());

  if (!exit_code) {
    return;
  }

  // macOS requires a larger timeout value for --install.
  bool succeeded = process.WaitForExitWithTimeout(
      2 * TestTimeouts::action_max_timeout(), exit_code);
  VPLOG_IF(0, !succeeded);
  ASSERT_TRUE(succeeded);
}

void ExpectUninstallPing(UpdaterScope scope, ScopedServer* test_server) {
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher({R"(.*"eventtype":4.*)"}),
                           request::GetScopeMatcher(scope)},
                          ")]}'\n");
}

void ExpectSelfUpdateSequence(UpdaterScope scope, ScopedServer* test_server) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_data_path));
  base::FilePath crx_path = test_data_path.AppendASCII(kSelfUpdateCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {request::GetPathMatcher(test_server->update_path()),
       request::GetContentMatcher(
           {base::StringPrintf(R"(.*"appid":"%s".*)", kUpdaterAppId)}),
       request::GetScopeMatcher(scope)},
      GetUpdateResponse(
          kUpdaterAppId, "", test_server->update_url().spec(),
          base::Version(kUpdaterVersion), crx_path, kSelfUpdateCRXRun,
          base::StrCat({"--update", IsSystemInstall(scope) ? " --system" : "",
                        " --", kEnableLoggingSwitch, " --",
                        kLoggingModuleSwitch, "=",
                        kLoggingModuleSwitchValue})));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce({request::GetContentMatcher({""})}, crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetContentMatcher({base::StringPrintf(
                               R"(.*"eventresult":1,"eventtype":3,)"
                               R"("nextversion":"%s",.*)",
                               kUpdaterVersion)}),
                           request::GetScopeMatcher(scope)},
                          ")]}'\n");
}

void ExpectUpdateCheckRequest(UpdaterScope scope, ScopedServer* test_server) {
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher({R"("updatecheck":{})"}),
                           request::GetScopeMatcher(scope)},
                          GetUpdateResponse({}));
}

void ExpectUpdateCheckSequence(UpdaterScope scope,
                               ScopedServer* test_server,
                               const std::string& app_id,
                               UpdateService::Priority priority,
                               const base::Version& from_version,
                               const base::Version& to_version) {
  ExpectUpdateCheckSequence(scope, test_server, app_id, priority,
                            /*event_type=*/3, from_version, to_version);
}

void ExpectUpdateSequence(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& app_id,
                          const std::string& install_data_index,
                          UpdateService::Priority priority,
                          const base::Version& from_version,
                          const base::Version& to_version) {
  ExpectUpdateSequence(scope, test_server, app_id, install_data_index, priority,
                       /*event_type=*/3, from_version, to_version);
}

void ExpectUpdateSequenceBadHash(UpdaterScope scope,
                                 ScopedServer* test_server,
                                 const std::string& app_id,
                                 const std::string& install_data_index,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  test_server->ExpectOnce(
      {request::GetPathMatcher(test_server->update_path()),
       request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {base::StringPrintf(R"("appid":"%s")", app_id.c_str()),
            install_data_index.empty()
                ? ""
                : base::StringPrintf(
                      R"("data":\[{"index":"%s","name":"install"}],.*)",
                      install_data_index.c_str())
                      .c_str()}),
       request::GetScopeMatcher(scope),
       request::GetAppPriorityMatcher(app_id, priority)},
      GetUpdateResponse(
          app_id, install_data_index, test_server->update_url().spec(),
          to_version, crx_path, kDoNothingCRXRun, {},
          "badbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbad1"));

  // Second request: update download.
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(), request::GetContentMatcher({""})},
      crx_bytes);

  // Third request: event ping.
  test_server->ExpectOnce(
      {request::GetPathMatcher(test_server->update_path()),
       request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher({base::StringPrintf(
           R"(.*"errorcat":1,"errorcode":12,"eventresult":0,"eventtype":3,)"
           R"("nextversion":"%s","previousversion":"%s".*)",
           to_version.GetString().c_str(), from_version.GetString().c_str())}),
       request::GetScopeMatcher(scope)},
      ")]}'\n");
}

void ExpectInstallSequence(UpdaterScope scope,
                           ScopedServer* test_server,
                           const std::string& app_id,
                           const std::string& install_data_index,
                           UpdateService::Priority priority,
                           const base::Version& from_version,
                           const base::Version& to_version) {
  ExpectUpdateSequence(scope, test_server, app_id, install_data_index, priority,
                       /*event_type=*/2, from_version, to_version);
}

// Runs multiple cycles of instantiating the update service, calling
// `GetVersion()`, then releasing the service interface.
void StressUpdateService(UpdaterScope scope) {
  base::RunLoop loop;

  // Number of times to run the cycle of instantiating the service.
  int n = 10;

  // Delay in milliseconds between successive cycles.
#if BUILDFLAG(IS_LINUX)
  // Looping too tightly causes socket connections to be dropped on Linux.
  const int kDelayBetweenLoopsMS = 10;
#else
  const int kDelayBetweenLoopsMS = 0;
#endif

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
        base::ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
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
  Run(scope, command, &exit_code);
  ASSERT_EQ(exit_code, kErrorOk);
}

void SetLastChecked(UpdaterScope updater_scope, const base::Time& time) {
  base::MakeRefCounted<PersistedData>(
      updater_scope, CreateGlobalPrefs(updater_scope)->GetPrefService())
      ->SetLastChecked(time);
}

void ExpectLastChecked(UpdaterScope updater_scope) {
  EXPECT_FALSE(
      base::MakeRefCounted<PersistedData>(
          updater_scope, CreateGlobalPrefs(updater_scope)->GetPrefService())
          ->GetLastChecked()
          .is_null());
}

void ExpectLastStarted(UpdaterScope updater_scope) {
  EXPECT_FALSE(
      base::MakeRefCounted<PersistedData>(
          updater_scope, CreateGlobalPrefs(updater_scope)->GetPrefService())
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
      [] {
        const base::FilePath test_executable =
            base::FilePath::FromASCII(kExecutableName).BaseName();
        return base::StrCat({test_executable.RemoveExtension().value(),
                             base::ASCIIToWide(kExecutableSuffix),
                             test_executable.Extension()});
      }(),
  };
#else
  return {GetExecutableRelativePath().BaseName().value(), kLauncherName};
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
    EXPECT_FALSE(IsProcessRunning(process_name))
        << PrintProcesses(process_name);
  }
}

#if !BUILDFLAG(IS_WIN)
void RunOfflineInstall(UpdaterScope scope,
                       bool is_legacy_install,
                       bool is_silent_install) {
  // TODO(crbug.com/1281688).
  NOTREACHED();
}

void RunOfflineInstallOsNotSupported(UpdaterScope scope,
                                     bool is_legacy_install,
                                     bool is_silent_install) {
  // TODO(crbug.com/1281688).
  NOTREACHED();
}
#endif  // IS_WIN

void DMPushEnrollmentToken(const std::string& enrollment_token) {
  scoped_refptr<DMStorage> storage = GetDefaultDMStorage();
  ASSERT_NE(storage, nullptr);
  EXPECT_TRUE(storage->StoreEnrollmentToken(enrollment_token));
  EXPECT_TRUE(storage->DeleteDMToken());
}

void DMDeregisterDevice(UpdaterScope scope) {
  if (!IsSystemInstall(GetTestScope())) {
    return;
  }
  EXPECT_TRUE(GetDefaultDMStorage()->InvalidateDMToken());
}

void DMCleanup(UpdaterScope scope) {
  if (!IsSystemInstall(GetTestScope())) {
    return;
  }
  scoped_refptr<DMStorage> storage = GetDefaultDMStorage();
  EXPECT_TRUE(storage->DeleteEnrollmentToken());
  EXPECT_TRUE(storage->DeleteDMToken());
  EXPECT_TRUE(base::DeletePathRecursively(storage->policy_cache_folder()));

#if BUILDFLAG(IS_WIN)
  RegDeleteKey(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement);
  RegDeleteKey(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY);
#endif
}

void ExpectDeviceManagementRegistrationRequest(
    ScopedServer* test_server,
    const std::string& enrollment_token,
    const std::string& dm_token) {
  ExpectDeviceManagementRequest(
      test_server, "register_policy_agent", "GoogleEnrollmentToken",
      enrollment_token, [&dm_token]() {
        enterprise_management::DeviceManagementResponse dm_response;
        dm_response.mutable_register_response()->set_device_management_token(
            dm_token);
        return dm_response.SerializeAsString();
      }());
}

void ExpectDeviceManagementPolicyFetchRequest(
    ScopedServer* test_server,
    const std::string& dm_token,
    const ::wireless_android_enterprise_devicemanagement::
        OmahaSettingsClientProto& omaha_settings) {
  ExpectDeviceManagementRequest(
      test_server, "policy", "GoogleDMToken", dm_token,
      [&dm_token, &omaha_settings]() {
        std::unique_ptr<::enterprise_management::DeviceManagementResponse>
            dm_response = GetDMResponseForOmahaPolicy(
                /*first_request=*/true, /*rotate_to_new_key=*/false,
                DMPolicyBuilderForTesting::SigningOption::kSignNormally,
                dm_token, GetDefaultDMStorage()->GetDeviceID(), omaha_settings);
        return dm_response->SerializeAsString();
      }());
}

void ExpectDeviceManagementPolicyValidationRequest(
    ScopedServer* test_server,
    const std::string& dm_token) {
  ExpectDeviceManagementRequest(test_server, "policy_validation_report",
                                "GoogleDMToken", dm_token, "");
}

}  // namespace updater::test
