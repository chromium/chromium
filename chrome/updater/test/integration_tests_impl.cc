// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests_impl.h"

#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
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
#include "base/json/json_file_value_serializer.h"
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
#include "base/strings/string_util.h"
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
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/request_matcher.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/file_version_info_win.h"
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
constexpr base::FilePath::CharType kCompanionAppTestExecutableName[] =
    FILE_PATH_LITERAL("enterprise_companion_test");
#elif BUILDFLAG(IS_WIN)
constexpr char kSelfUpdateCRXRun[] = "UpdaterSetup_test.exe";
constexpr char kDoNothingCRXName[] = "updater_qualification_app_exe.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app.exe";
constexpr base::FilePath::CharType kCompanionAppTestExecutableName[] =
    FILE_PATH_LITERAL("enterprise_companion_test.exe");
#elif BUILDFLAG(IS_LINUX)
constexpr char kSelfUpdateCRXRun[] = "updater_test";
constexpr char kDoNothingCRXName[] = "updater_qualification_app.crx";
constexpr char kDoNothingCRXRun[] = "qualification_app";
constexpr base::FilePath::CharType kCompanionAppTestExecutableName[] =
    FILE_PATH_LITERAL("enterprise_companion_test");
#endif

std::string GetHashHex(const base::FilePath& file) {
  base::MemoryMappedFile mmfile;
  EXPECT_TRUE(mmfile.Initialize(file));  // Note: This fails with an empty file.
  return base::HexEncode(crypto::SHA256Hash(mmfile.bytes()));
}

std::string GetUpdateResponseForApp(
    const std::string& app_id,
    const std::string& install_data_index,
    const std::string& codebase,
    const base::Version& version,
    const base::FilePath& update_file,
    const std::string& run_action,
    const std::string& arguments,
    const std::optional<std::string>& file_hash = std::nullopt,
    const std::optional<std::string>& status = std::nullopt) {
  return base::StringPrintf(
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"%s",)"
      R"(%s)"
      R"(      "updatecheck":{)"
      R"(        "status":"ok",)"
      R"(        "urls":{"url":[{"codebase":"%s/"}]},)"
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
      base::ToLowerASCII(app_id).c_str(), status ? status->c_str() : "ok",
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

void RunUpdaterWithSwitches(const base::Version& version,
                            UpdaterScope scope,
                            const std::vector<std::string>& switches,
                            std::optional<int> expected_exit_code) {
  const std::optional<base::FilePath> installed_executable_path =
      GetVersionedInstallDirectory(scope, version)
          ->Append(GetExecutableRelativePath());
  ASSERT_TRUE(installed_executable_path);
  ASSERT_TRUE(base::PathExists(*installed_executable_path));
  base::CommandLine command_line(*installed_executable_path);
  for (const std::string& command_switch : switches) {
    command_line.AppendSwitch(command_switch);
  }
  if (expected_exit_code) {
    int exit_code = -1;
    Run(scope, command_line, &exit_code);
    ASSERT_EQ(exit_code, expected_exit_code.value());
  } else {
    Run(scope, command_line, nullptr);
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
       request::GetAppPriorityMatcher(app_id, priority),
       request::GetUpdaterEnableUpdatesMatcher()},
      GetUpdateResponse(app_id, "", test_server->download_url().spec(),
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
                          const base::Version& to_version,
                          bool do_fault_injection) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath crx_path = test_data_path.Append(FILE_PATH_LITERAL("updater"))
                                .AppendASCII(kDoNothingCRXName);
  ASSERT_TRUE(base::PathExists(crx_path));

  // First request: update check.
  if (do_fault_injection) {
    test_server->ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  }
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
       request::GetAppPriorityMatcher(app_id, priority),
       request::GetUpdaterEnableUpdatesMatcher()},
      GetUpdateResponse(app_id, install_data_index,
                        test_server->download_url().spec(), to_version,
                        crx_path, kDoNothingCRXRun, {}));

  // Second request: update download.
  if (do_fault_injection) {
    test_server->ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  }
  std::string crx_bytes;
  base::ReadFileToString(crx_path, &crx_bytes);
  test_server->ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(), request::GetContentMatcher({""})},
      crx_bytes);

  // Third request: event ping.
  if (do_fault_injection) {
    test_server->ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  }
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
                                   net::HttpStatusCode response_status,
                                   const std::string& response,
                                   std::optional<GURL> target_url = {}) {
  request::MatcherGroup request_matchers = {
      request::GetPathMatcher(base::StringPrintf(
          R"(%s\?request=%s&apptype=Chrome&)"
          R"(agent=%s\+%s&platform=.*&deviceid=%s)",
          test_server->device_management_path().c_str(), request_type.c_str(),
          PRODUCT_FULLNAME_STRING, kUpdaterVersion,
          device_management_storage::GetDefaultDMStorage()
              ->GetDeviceID()
              .c_str())),
      request::GetUpdaterUserAgentMatcher(),
      request::GetHeaderMatcher(
          {{"Authorization",
            base::StringPrintf("%s token=%s", authorization_type.c_str(),
                               authorization_token.c_str())},
           {"Content-Type", "application/x-protobuf"}})};
  if (target_url) {
    request_matchers.push_back(request::GetTargetURLMatcher(*target_url));
  }
  test_server->ExpectOnce(request_matchers, response, response_status);
}

void ExpectDeviceManagementRequestViaCompanionApp(
    ScopedServer* test_server,
    const std::string& request_type,
    const std::string& authorization_type,
    const std::string& authorization_token,
    net::HttpStatusCode response_status,
    const std::string& response,
    std::optional<GURL> target_url = {}) {
  request::MatcherGroup request_matchers = {
      request::GetPathMatcher(base::StringPrintf(
          R"(%s\?.*agent=%sEnterpriseCompanion\+%s&apptype=Chrome)"
          R"(&deviceid=%s.*&platform=.*&request=%s)",
          test_server->device_management_path().c_str(), BROWSER_NAME_STRING,
          kUpdaterVersion,
          device_management_storage::GetDefaultDMStorage()
              ->GetDeviceID()
              .c_str(),
          request_type.c_str())),
      request::GetHeaderMatcher(
          {{"Authorization",
            base::StringPrintf("%s token=%s", authorization_type.c_str(),
                               authorization_token.c_str())},
           {"Content-Type", "application/protobuf"}})};
  if (target_url) {
    request_matchers.push_back(request::GetTargetURLMatcher(*target_url));
  }
  test_server->ExpectOnce(request_matchers, response, response_status);
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
    const int event_type,
    const std::string& custom_app_response,
    const std::string& response_status)
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
      event_type(event_type),
      custom_app_response(custom_app_response),
      response_status(response_status.empty() ? "ok" : response_status) {}
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

void RegisterApp(UpdaterScope scope, const RegistrationRequest& registration) {
  scoped_refptr<UpdateService> update_service = CreateUpdateServiceProxy(scope);
  base::RunLoop loop;
  update_service->RegisterApp(registration,
                              base::BindLambdaForTesting([&loop](int result) {
                                EXPECT_EQ(result, 0);
                                loop.Quit();
                              }));
  loop.Run();
}

void RegisterAppByValue(UpdaterScope scope, const base::Value::Dict& value) {
  RegistrationRequest registration;
  registration.app_id = *value.FindString("app_id");
  registration.brand_code = *value.FindString("brand_code");
  registration.brand_path =
      base::FilePath::FromASCII(*value.FindString("brand_path"));
  registration.ap = *value.FindString("ap");
  registration.ap_path =
      base::FilePath::FromASCII(*value.FindString("ap_path"));
  registration.ap_key = *value.FindString("ap_key");
  registration.version = base::Version(*value.FindString("version"));
  registration.version_path =
      base::FilePath::FromASCII(*value.FindString("version_path"));
  registration.version_key = *value.FindString("version_key");
  registration.existence_checker_path =
      base::FilePath::FromASCII(*value.FindString("existence_checker_path"));
  registration.cohort = *value.FindString("cohort");
  registration.cohort_name = *value.FindString("cohort_name");
  registration.cohort_hint = *value.FindString("cohort_hint");
  return RegisterApp(scope, registration);
}

void EnterTestMode(const GURL& update_url,
                   const GURL& crash_upload_url,
                   const GURL& device_management_url,
                   const GURL& app_logo_url,
                   base::TimeDelta idle_timeout,
                   base::TimeDelta server_keep_alive_time,
                   base::TimeDelta ceca_connection_timeout) {
  ASSERT_TRUE(
      ExternalConstantsBuilder()
          .SetUpdateURL(std::vector<std::string>{update_url.spec()})
          .SetCrashUploadURL(crash_upload_url.spec())
          .SetDeviceManagementURL(device_management_url.spec())
          .SetAppLogoURL(app_logo_url.spec())
          .SetUseCUP(false)
          .SetInitialDelay(base::Milliseconds(100))
          .SetServerKeepAliveTime(server_keep_alive_time)
          .SetCrxVerifierFormat(crx_file::VerifierFormat::CRX3)
          .SetOverinstallTimeout(GetOverinstallTimeoutForEnterTestMode())
          .SetIdleCheckPeriod(idle_timeout)
          .SetCecaConnectionTimeout(ceca_connection_timeout)
          .Modify());
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
  EXPECT_EQ(version, [scope] {
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

void Install(UpdaterScope scope, const base::Value::List& switches) {
  const base::FilePath path = GetSetupExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitchASCII(kInstallSwitch, "usagestats=1");
  for (const base::Value& cmd_line_switch : switches) {
    command_line.AppendSwitch(cmd_line_switch.GetString());
  }
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, 0);
}

void InstallUpdaterAndApp(UpdaterScope scope,
                          const std::string& app_id,
                          const bool is_silent_install,
                          const std::string& tag,
                          const std::string& child_window_text_to_find,
                          const bool always_launch_cmd,
                          const bool verify_app_logo_loaded,
                          const bool expect_success,
                          const bool wait_for_the_installer) {
  const base::FilePath path = GetSetupExecutablePath();
  ASSERT_FALSE(path.empty());
  base::CommandLine command_line(path);
  command_line.AppendSwitchASCII(kInstallSwitch, tag);
  if (!app_id.empty()) {
    command_line.AppendSwitchASCII(kAppIdSwitch, app_id);
  }
  if (is_silent_install) {
    ASSERT_TRUE(child_window_text_to_find.empty());
    command_line.AppendSwitch(kSilentSwitch);
  }
  if (always_launch_cmd) {
    command_line.AppendSwitch(kAlwaysLaunchCmdSwitch);
  }

  if (child_window_text_to_find.empty()) {
    int exit_code = -1;
    Run(scope, command_line, wait_for_the_installer ? &exit_code : nullptr);
    if (wait_for_the_installer) {
      ASSERT_EQ(expect_success, exit_code == 0);
    }
  } else {
#if BUILDFLAG(IS_WIN)
    ASSERT_TRUE(wait_for_the_installer);
    Run(scope, command_line, nullptr);

    std::u16string bundle_name;
    if (!tag.empty()) {
      tagging::TagArgs tag_args;
      ASSERT_EQ(tagging::ErrorCode::kSuccess,
                tagging::Parse(tag, {}, tag_args));
      bundle_name = base::UTF8ToUTF16(tag_args.bundle_name);
    }
    CloseInstallCompleteDialog(bundle_name,
                               base::ASCIIToWide(child_window_text_to_find),
                               verify_app_logo_loaded);
#else
    NOTREACHED_IN_MIGRATION();
#endif
  }
}

void PrintFile(const base::FilePath& file) {
  std::string contents;
  if (!base::ReadFileToString(file, &contents)) {
    return;
  }
  VLOG(0) << "Contents of " << file << " for " << GetTestName();
  const std::string demarcation(72, '=');
  VLOG(0) << demarcation;
  VLOG(0) << contents;
  VLOG(0) << "End contents of " << file << " for " << GetTestName() << ".";
  VLOG(0) << demarcation;
}

std::vector<base::FilePath> GetUpdaterLogFilesInTmp() {
  base::FilePath temp_dir;

#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(
      base::PathService::Get(IsSystemInstall(GetUpdaterScopeForTesting())
                                 ? static_cast<int>(base::DIR_SYSTEM_TEMP)
                                 : static_cast<int>(base::DIR_TEMP),
                             &temp_dir));
#endif
  if (temp_dir.empty()) {
    return {};
  }

  std::vector<base::FilePath> files;
  base::FileEnumerator(temp_dir, false, base::FileEnumerator::FILES,
                       FILE_PATH_LITERAL("updater*.log"))
      .ForEach([&](const base::FilePath& item) { files.push_back(item); });
  return files;
}

void PrintLog(UpdaterScope scope) {
  PrintFile([&] {
    std::optional<base::FilePath> path = GetInstallDirectory(scope);
    if (path && base::PathExists(path->AppendASCII("updater.log"))) {
      return path->AppendASCII("updater.log");
    } else if (const std::vector<base::FilePath> files =
                   GetUpdaterLogFilesInTmp();
               !files.empty()) {
      return files[0];
    } else {
      VLOG(0) << "updater.log file not found for " << GetTestName();
      return base::FilePath();
    }
  }());
}

// Copies the updater log file present in `src_dir` or `%TMP%` to a
// test-specific directory name in Swarming/Isolate. Avoids overwriting the
// destination log file if other instances of it exist in the destination
// directory. Swarming retries each failed test. It is useful to capture a few
// logs from previous failures instead of the log of the last run only. Logs
// labeled with different (optional) infixes are numbered independently. The
// infix is applied only to the output log file name; the retrieved log is
// always `updater.log`.
void CopyLog(const base::FilePath& src_dir, const std::string& infix) {
  base::FilePath log_path = src_dir.AppendASCII("updater.log");
  if (!base::PathExists(log_path)) {
    if (const std::vector<base::FilePath> files = GetUpdaterLogFilesInTmp();
        !files.empty()) {
      log_path = files[0];
    }
  }

  const std::string real_infix =
      infix.empty() ? "" : base::StrCat({".", infix});

  base::FilePath dest_dir = GetLogDestinationDir();
  if (!dest_dir.empty() && base::PathExists(dest_dir) &&
      base::PathExists(log_path)) {
    dest_dir = dest_dir.AppendASCII(GetTestName());
    EXPECT_TRUE(base::CreateDirectory(dest_dir));
    const base::FilePath dest_file_path = [dest_dir, real_infix] {
      base::FilePath path =
          dest_dir.AppendASCII(base::StrCat({"updater", real_infix, ".log"}));
      for (int i = 1; i < 10 && base::PathExists(path); ++i) {
        path = dest_dir.AppendASCII(
            base::StringPrintf("updater%s.%d.log", real_infix.c_str(), i));
      }
      return path;
    }();
    VLOG(0) << "Copying updater.log file. From: " << log_path
            << ". To: " << dest_file_path;
    EXPECT_TRUE(base::CopyFile(log_path, dest_file_path));
  }
}

void ExpectNoCrashes(UpdaterScope scope) {
  if (scope == UpdaterScope::kSystem) {
    ExpectNoCrashes(UpdaterScope::kUser);
  }
  std::optional<base::FilePath> database_path(GetCrashDatabasePath(scope));
  if (!database_path || !base::PathExists(*database_path)) {
    return;
  }

  base::FilePath dest_dir = GetLogDestinationDir();
  if (dest_dir.empty()) {
    VLOG(2) << "No log destination folder, skip copying possible crash dumps.";
    return;
  }
  dest_dir =
      dest_dir.AppendASCII(GetTestName())
          .AppendASCII(scope == UpdaterScope::kSystem ? "system" : "user");
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
    if (app.allow_rollback) {
      app_requests.push_back(R"("rollback_allowed":true,)");
    }
    if (!app.target_version_prefix.empty()) {
      app_requests.push_back(base::StringPrintf(
          R"("targetversionprefix":"%s")", app.target_version_prefix.c_str()));
    }
    if (!app.target_channel.empty()) {
      app_requests.push_back(base::StringPrintf(R"("release_channel":"%s",)",
                                                app.target_channel.c_str()));
    }
    if (!app.custom_app_response.empty()) {
      app_responses.push_back(app.custom_app_response);
      continue;
    }
    const base::FilePath crx_path = exe_path.Append(app.crx_relative_path);
    const base::FilePath base_name = crx_path.BaseName().RemoveExtension();
    const base::FilePath run_action =
        base_name.Extension().empty() ? base_name.AddExtension(kExeExtension)
                                      : base_name;
    app_responses.push_back(GetUpdateResponseForApp(
        app.app_id, "", test_server->download_url().spec(), app.to_version,
        crx_path, run_action.MaybeAsASCII().c_str(), app.args, std::nullopt,
        app.response_status));
  }
  test_server->ExpectOnce({request::GetPathMatcher(test_server->update_path()),
                           request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher(attributes),
                           request::GetContentMatcher(app_requests),
                           request::GetScopeMatcher(scope),
                           request::GetUpdaterEnableUpdatesMatcher()},
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
    } else if (app.custom_app_response.empty() && app.response_status == "ok") {
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
  RunUpdaterWithSwitches(base::Version(kUpdaterVersion), scope, {kWakeSwitch},
                         expected_exit_code);
}

void RunWakeAll(UpdaterScope scope) {
  RunUpdaterWithSwitches(base::Version(kUpdaterVersion), scope,
                         {kWakeAllSwitch}, kErrorOk);
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
  RunUpdaterWithSwitches(active_version, scope, {kWakeSwitch},
                         expected_exit_code);
}

void RunCrashMe(UpdaterScope scope) {
  RunUpdaterWithSwitches(base::Version(kUpdaterVersion), scope,
                         {kCrashMeSwitch, kMonitorSelfSwitch}, std::nullopt);
}

void RunServer(UpdaterScope scope, int expected_exit_code, bool internal) {
  const std::optional<base::FilePath> installed_executable_path =
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
  if (const std::optional<int> _state_member =                         \
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

  if (const std::optional<int> expected_result =
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
  std::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
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

  std::optional<base::FilePath> exe_path =
      GetUpdaterExecutablePath(scope, active_version);
  ASSERT_TRUE(exe_path.has_value())
      << "No path for active updater. Version: " << active_version;
  DeleteFile(*exe_path);
#if BUILDFLAG(IS_LINUX)
  // On Linux, a qualified service makes a full copy of itself, so we have to
  // delete the copy that systemd uses too.
  std::optional<base::FilePath> launcher_path =
      GetUpdateServiceLauncherPath(GetUpdaterScopeForTesting());
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
  base::MakeRefCounted<PersistedData>(scope, global_prefs->GetPrefService(),
                                      nullptr)
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
  std::optional<base::FilePath> log = GetLogFilePath(scope);
  ASSERT_TRUE(log);
  std::string data = "This test string is used to fill up log space.\n";
  for (int i = 0; i < 1024 * 1024 * 3; i += data.length()) {
    ASSERT_TRUE(base::AppendToFile(*log, data));
  }
}

void ExpectLogRotated(UpdaterScope scope) {
  std::optional<base::FilePath> log = GetLogFilePath(scope);
  ASSERT_TRUE(log);
  EXPECT_TRUE(base::PathExists(log->AddExtension(FILE_PATH_LITERAL(".old"))));
  int64_t size = 0;
  ASSERT_TRUE(base::GetFileSize(*log, &size));
  EXPECT_TRUE(size < 1024 * 1024);
}

void ExpectRegistered(UpdaterScope scope, const std::string& app_id) {
  ASSERT_TRUE(base::Contains(
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefs(scope)->GetPrefService(), nullptr)
          ->GetAppIds(),
      base::ToLowerASCII(app_id)));
}

void ExpectNotRegistered(UpdaterScope scope, const std::string& app_id) {
  ASSERT_FALSE(base::Contains(
      base::MakeRefCounted<PersistedData>(
          scope, CreateGlobalPrefs(scope)->GetPrefService(), nullptr)
          ->GetAppIds(),
      base::ToLowerASCII(app_id)));
}

void ExpectAppTag(UpdaterScope scope,
                  const std::string& app_id,
                  const std::string& tag) {
  EXPECT_EQ(tag, base::MakeRefCounted<PersistedData>(
                     scope, CreateGlobalPrefs(scope)->GetPrefService(), nullptr)
                     ->GetAP(app_id));
}

void SetAppTag(UpdaterScope scope,
               const std::string& app_id,
               const std::string& tag) {
  scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope);
  base::MakeRefCounted<PersistedData>(scope, global_prefs->GetPrefService(),
                                      nullptr)
      ->SetAP(app_id, tag);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
}

void Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
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

void ExpectCliResult(base::CommandLine command_line,
                     bool elevate,
                     std::optional<std::string> want_stdout,
                     std::optional<int> want_exit_code) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
  if (elevate) {
    command_line = MakeElevated(command_line);
  }
  VLOG(0) << "Run command (with expectations): "
          << command_line.GetCommandLineString();
  std::string output;
  int exit_code = EXIT_FAILURE;
  bool run_succeeded =
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
  VPLOG_IF(0, !run_succeeded);
  ASSERT_TRUE(run_succeeded);

  if (want_exit_code) {
    ASSERT_EQ(*want_exit_code, exit_code) << "stdout:\n" << output;
  }
  if (want_stdout) {
    ASSERT_EQ(*want_stdout, output) << "exit code:" << exit_code;
  }
}

void SetupRealUpdaterLowerVersion(UpdaterScope scope) {
  base::CommandLine command_line(GetRealUpdaterLowerVersionPath());
  command_line.AppendSwitch(kInstallSwitch);
  int exit_code = -1;
  Run(scope, command_line, &exit_code);
  ASSERT_EQ(exit_code, 0);
}

void ExpectPing(UpdaterScope scope,
                ScopedServer* test_server,
                int event_type,
                std::optional<GURL> target_url) {
  ASSERT_TRUE(test_server) << "TEST ISSUE - nil `test_server` in ExpectPing";
  request::MatcherGroup request_matchers = {
      request::GetPathMatcher(test_server->update_path()),
      request::GetUpdaterUserAgentMatcher(),
      request::GetContentMatcher(
          {base::StringPrintf(R"(.*"eventtype":%d,.*)", event_type)}),
      request::GetScopeMatcher(scope)};

  if (target_url) {
    request_matchers.push_back(request::GetTargetURLMatcher(*target_url));
  }
  test_server->ExpectOnce(request_matchers, ")]}'\n");
}

void ExpectAppCommandPing(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& appid,
                          const std::string& appcommandid,
                          int errorcode,
                          int eventresult,
                          int event_type,
                          const base::Version& version) {
  test_server->ExpectOnce(
      {
          request::GetPathMatcher(test_server->update_path()),
          request::GetUpdaterUserAgentMatcher(),
          request::GetContentMatcher({base::StringPrintf(
              R"(.*"appid":"%s","enabled":true,"event":\[{"appcommandid":"%s",)"
              R"("errorcode":%d,"eventresult":%d,"eventtype":%d,)"
              R"("previousversion":"%s"}\])",
              appid.c_str(), appcommandid.c_str(), errorcode, eventresult,
              event_type, version.GetString().c_str())}),
          request::GetScopeMatcher(scope),
      },
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
          kUpdaterAppId, "", test_server->download_url().spec(),
          base::Version(kUpdaterVersion), crx_path, kSelfUpdateCRXRun,
          base::StrCat(
              {"--update", IsSystemInstall(scope) ? " --system" : ""})));

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
                          const base::Version& to_version,
                          bool do_fault_injection) {
  ExpectUpdateSequence(scope, test_server, app_id, install_data_index, priority,
                       /*event_type=*/3, from_version, to_version,
                       do_fault_injection);
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
          app_id, install_data_index, test_server->download_url().spec(),
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
                           const base::Version& to_version,
                           bool do_fault_injection) {
  ExpectUpdateSequence(scope, test_server, app_id, install_data_index, priority,
                       /*event_type=*/2, from_version, to_version,
                       do_fault_injection);
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
  auto loop_closure = [&] {
    LOG(ERROR) << __func__ << ": n: " << n << ", " << base::Time::Now();
    if (--n) {
      return false;
    }
    loop.Quit();
    return true;
  };

  // Creates a task runner, and runs the service instance on it.
  using LoopClosure = decltype(loop_closure);
  auto stress_runner = [scope, loop_closure] {
    // `task_runner` is always bound on the main sequence.
    struct Local {
      static void GetVersion(
          UpdaterScope scope,
          scoped_refptr<base::SequencedTaskRunner> task_runner,
          LoopClosure loop_closure) {
        base::ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting([scope, task_runner, loop_closure] {
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
            base::BindLambdaForTesting([scope, task_runner, loop_closure] {
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
      updater_scope, CreateGlobalPrefs(updater_scope)->GetPrefService(),
      nullptr)
      ->SetLastChecked(time);
}

void ExpectLastChecked(UpdaterScope updater_scope) {
  EXPECT_FALSE(base::MakeRefCounted<PersistedData>(
                   updater_scope,
                   CreateGlobalPrefs(updater_scope)->GetPrefService(), nullptr)
                   ->GetLastChecked()
                   .is_null());
}

void ExpectLastStarted(UpdaterScope updater_scope) {
  EXPECT_FALSE(base::MakeRefCounted<PersistedData>(
                   updater_scope,
                   CreateGlobalPrefs(updater_scope)->GetPrefService(), nullptr)
                   ->GetLastStarted()
                   .is_null());
}

std::set<base::FilePath::StringType> GetTestProcessNames() {
#if BUILDFLAG(IS_MAC)
  return {GetExecutableRelativePath().BaseName().value(),
          GetSetupExecutablePath().BaseName().value()};
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

std::set<base::FilePath::StringType> GetCompanionAppProcessNames() {
  return {base::FilePath::FromASCII(kCompanionAppExecutableName).value(),
          kCompanionAppTestExecutableName};
}

#if BUILDFLAG(IS_WIN)
VersionProcessFilter::VersionProcessFilter()
    : this_version_(base::Version(kUpdaterVersion)), older_version_([] {
        const std::unique_ptr<FileVersionInfoWin> version_info =
            FileVersionInfoWin::CreateFileVersionInfoWin(
                GetRealUpdaterLowerVersionPath());
        CHECK(version_info);
        const base::Version version(
            base::UTF16ToUTF8(version_info->file_version()));
        CHECK(version.IsValid());
        return version;
      }()) {}

bool VersionProcessFilter::Includes(const base::ProcessEntry& entry) const {
  const base::Process process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                            false, entry.th32ProcessID));
  if (!process.IsValid()) {
    return false;
  }

  DWORD path_len = MAX_PATH;
  std::wstring path(path_len, '\0');
  if (!::QueryFullProcessImageName(process.Handle(), 0, path.data(),
                                   &path_len)) {
    return false;
  }

  const std::unique_ptr<FileVersionInfoWin> version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(base::FilePath(path));
  if (!version_info) {
    return false;
  }
  const base::Version version(base::UTF16ToUTF8(version_info->file_version()));
  return version.IsValid() &&
         (version == this_version_ || version == older_version_);
}
#endif  // BUILDFLAG(IS_WIN)

void CleanProcesses() {
  base::ProcessFilter* filter = nullptr;
#if BUILDFLAG(IS_WIN)
  VersionProcessFilter version_filter;
  filter = &version_filter;
#endif

  for (const base::FilePath::StringType& process_name : GetTestProcessNames()) {
    EXPECT_TRUE(KillProcesses(process_name, -1, filter)) << process_name;
    EXPECT_TRUE(WaitForProcessesToExit(process_name,
                                       TestTimeouts::action_timeout(), filter))
        << process_name;
    EXPECT_FALSE(IsProcessRunning(process_name, filter)) << process_name;
  }
}

void ExpectCleanProcesses() {
  base::ProcessFilter* filter = nullptr;
#if BUILDFLAG(IS_WIN)
  VersionProcessFilter version_filter;
  filter = &version_filter;
#endif

  for (const base::FilePath::StringType& process_name : GetTestProcessNames()) {
    EXPECT_FALSE(IsProcessRunning(process_name, filter))
        << PrintProcesses(process_name, filter);
  }
}

// Standalone installers are supported for Windows only.
#if !BUILDFLAG(IS_WIN)
void RunOfflineInstall(UpdaterScope scope,
                       bool is_legacy_install,
                       bool is_silent_install) {
  NOTREACHED_IN_MIGRATION();
}

void RunOfflineInstallOsNotSupported(UpdaterScope scope,
                                     bool is_legacy_install,
                                     bool is_silent_install) {
  NOTREACHED_IN_MIGRATION();
}
#endif  // !BUILDFLAG(IS_WIN)

void DMPushEnrollmentToken(const std::string& enrollment_token) {
  scoped_refptr<device_management_storage::DMStorage> storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_NE(storage, nullptr);
  EXPECT_TRUE(storage->StoreEnrollmentToken(enrollment_token));
  EXPECT_TRUE(storage->DeleteDMToken());
}

void DMDeregisterDevice(UpdaterScope scope) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    return;
  }
  EXPECT_TRUE(
      device_management_storage::GetDefaultDMStorage()->InvalidateDMToken());
}

void DMCleanup(UpdaterScope scope) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    return;
  }
  scoped_refptr<device_management_storage::DMStorage> storage =
      device_management_storage::GetDefaultDMStorage();
  EXPECT_TRUE(storage->DeleteEnrollmentToken());
  EXPECT_TRUE(storage->DeleteDMToken());
  EXPECT_TRUE(base::DeletePathRecursively(storage->policy_cache_folder()));

#if BUILDFLAG(IS_WIN)
  RegDeleteKey(HKEY_LOCAL_MACHINE, kRegKeyCompanyLegacyCloudManagement);
  RegDeleteKey(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement);
  RegDeleteKey(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY);
  RegDeleteKey(HKEY_LOCAL_MACHINE, COMPANY_POLICIES_KEY);
#endif
}

void InstallEnterpriseCompanionApp() {
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  int exit_code = -1;
  base::CommandLine command(exe_path.Append(kCompanionAppTestExecutableName));
  command.AppendSwitch("install");
  base::Process process = base::LaunchProcess(command, {});
  EXPECT_TRUE(process.IsValid());
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
}

void InstallBrokenEnterpriseCompanionApp() {
  std::optional<base::FilePath> install_dir =
      enterprise_companion::GetInstallDirectory();
  ASSERT_TRUE(install_dir);
  ASSERT_TRUE(base::CreateDirectory(*install_dir));
  ASSERT_TRUE(
      base::WriteFile(install_dir->AppendASCII(kCompanionAppExecutableName),
                      "broken enterprise companion app"));
  VLOG(1) << "Broken enterprise companion app installed.";
}

void UninstallBrokenEnterpriseCompanionApp() {
  std::optional<base::FilePath> install_dir =
      enterprise_companion::GetInstallDirectory();
  ASSERT_TRUE(install_dir);
  for (const base::FilePath::StringType& process_name :
       GetCompanionAppProcessNames()) {
    KillProcesses(process_name, -1);
    WaitForProcessesToExit(process_name, TestTimeouts::action_timeout());
    EXPECT_FALSE(IsProcessRunning(process_name)) << process_name;
  }
  ASSERT_TRUE(base::DeletePathRecursively(*install_dir));
  VLOG(1) << "Enterprise companion app manually uninstalled.";
}

void InstallEnterpriseCompanionAppOverrides(
    const base::Value::Dict& external_overrides) {
  std::optional<base::FilePath> json_path =
      enterprise_companion::GetOverridesFilePath();
  EXPECT_TRUE(json_path);
  EXPECT_TRUE(base::CreateDirectory(json_path->DirName()));
  JSONFileValueSerializer json_serializer(*json_path);
#if BUILDFLAG(IS_WIN)
  // Allow admin to access companion app's Mojo service named pipe.
  EXPECT_TRUE(json_serializer.Serialize(external_overrides.Clone().Set(
      enterprise_companion::kNamedPipeSecurityDescriptorKey,
      "D:(A;;GA;;;BA)")));
#else
  EXPECT_TRUE(json_serializer.Serialize(external_overrides));
#endif
  VLOG(1) << "Enterprise companion app overrides installed.";
}

void ExpectEnterpriseCompanionAppNotInstalled() {
  std::optional<base::FilePath> install_dir =
      enterprise_companion::GetInstallDirectory();
  if (!install_dir) {
    VLOG(1) << "Cannot find enterprise companion app installation directory, "
            << "assume it does not exist.";
    return;
  }
  EXPECT_FALSE(
      base::PathExists(install_dir->Append(kCompanionAppTestExecutableName)));
}

void UninstallEnterpriseCompanionApp() {
  std::optional<base::FilePath> install_dir =
      enterprise_companion::GetInstallDirectory();
  if (!install_dir) {
    VLOG(1) << "Cannot find enterprise companion app installation directory, "
            << "assume it does not exist.";
    return;
  }

  base::CommandLine command_line(
      install_dir->AppendASCII(kCompanionAppExecutableName));
  command_line.AppendSwitch(kUninstallCompanionAppSwitch);
  base::Process uninstall_process = base::LaunchProcess(command_line, {});
  if (uninstall_process.IsValid() && WaitForProcess(uninstall_process) == 0) {
    VLOG(1) << "Enterprise companion app is removed.";
    return;
  }

  // Forcefully remove the installation in case a broken one exists.
  ASSERT_NO_FATAL_FAILURE(UninstallBrokenEnterpriseCompanionApp());
}

void ExpectDeviceManagementRegistrationRequest(
    ScopedServer* test_server,
    const std::string& enrollment_token,
    const std::string& dm_token) {
  ExpectDeviceManagementRequest(
      test_server, "register_policy_agent", "GoogleEnrollmentToken",
      enrollment_token, net::HTTP_OK, [&dm_token] {
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
        OmahaSettingsClientProto& omaha_settings,
    bool first_request,
    bool rotate_public_key,
    std::optional<GURL> target_url) {
  ExpectDeviceManagementRequest(
      test_server, "policy", "GoogleDMToken", dm_token, net::HTTP_OK,
      [&dm_token, &omaha_settings, first_request, rotate_public_key] {
        std::unique_ptr<::enterprise_management::DeviceManagementResponse>
            dm_response = GetDMResponseForOmahaPolicy(
                first_request, rotate_public_key,
                DMPolicyBuilderForTesting::SigningOption::kSignNormally,
                dm_token,
                device_management_storage::GetDefaultDMStorage()->GetDeviceID(),
                omaha_settings);
        return dm_response->SerializeAsString();
      }(),
      target_url);
}

void ExpectDeviceManagementPolicyFetchWithNewPublicKeyRequest(
    ScopedServer* test_server,
    const std::string& dm_token,
    const ::wireless_android_enterprise_devicemanagement::
        OmahaSettingsClientProto& omaha_settings) {
  ExpectDeviceManagementRequest(
      test_server, "policy", "GoogleDMToken", dm_token, net::HTTP_OK,
      [&dm_token, &omaha_settings] {
        std::unique_ptr<::enterprise_management::DeviceManagementResponse>
            dm_response =
                DMPolicyBuilderForTesting::CreateInstanceWithOptions(
                    /*first_request=*/false, /*rotate_to_new_key=*/true,
                    DMPolicyBuilderForTesting::SigningOption::kSignNormally,
                    dm_token,
                    device_management_storage::GetDefaultDMStorage()
                        ->GetDeviceID())
                    ->BuildDMResponseForPolicies(
                        {{"a-mock-policy-type-without-new-public-key",
                          omaha_settings.SerializeAsString()},
                         {"google/machine-level-omaha",
                          omaha_settings.SerializeAsString()},
                         {"yet-another-policy-type-without-new-public-key",
                          omaha_settings.SerializeAsString()}});
        return dm_response->SerializeAsString();
      }());
}

void ExpectDeviceManagementTokenDeletionRequest(ScopedServer* test_server,
                                                const std::string& dm_token,
                                                bool invalidate_token) {
  ::enterprise_management::DeviceManagementErrorDetail error_detail =
      invalidate_token ? ::enterprise_management::
                             CBCM_DELETION_POLICY_PREFERENCE_INVALIDATE_TOKEN
                       : ::enterprise_management::
                             CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN;
  ExpectDeviceManagementRequest(
      test_server, "policy", "GoogleDMToken", dm_token, net::HTTP_GONE,
      [&dm_token, error_detail] {
        std::unique_ptr<::enterprise_management::DeviceManagementResponse>
            dm_response =
                DMPolicyBuilderForTesting::CreateInstanceWithOptions(
                    /*first_request=*/false, /*rotate_to_new_key=*/false,
                    DMPolicyBuilderForTesting::SigningOption::kSignNormally,
                    dm_token,
                    device_management_storage::GetDefaultDMStorage()
                        ->GetDeviceID())
                    ->BuildDMResponseWithError(error_detail);
        return dm_response->SerializeAsString();
      }());
}

void ExpectDeviceManagementPolicyValidationRequest(
    ScopedServer* test_server,
    const std::string& dm_token) {
  ExpectDeviceManagementRequest(test_server, "policy_validation_report",
                                "GoogleDMToken", dm_token, net::HTTP_OK, "");
}

void ExpectDeviceManagementRegistrationRequestViaCompanionApp(
    ScopedServer* test_server,
    const std::string& enrollment_token,
    const std::string& dm_token) {
  ExpectDeviceManagementRequestViaCompanionApp(
      test_server, "register_policy_agent", "GoogleEnrollmentToken",
      enrollment_token, net::HTTP_OK, [&dm_token] {
        enterprise_management::DeviceManagementResponse dm_response;
        dm_response.mutable_register_response()->set_device_management_token(
            dm_token);
        return dm_response.SerializeAsString();
      }());
}

void ExpectDeviceManagementPolicyFetchRequestViaCompanionApp(
    ScopedServer* test_server,
    const std::string& dm_token,
    const ::wireless_android_enterprise_devicemanagement::
        OmahaSettingsClientProto& omaha_settings,
    bool first_request,
    bool rotate_public_key,
    std::optional<GURL> target_url) {
  ExpectDeviceManagementRequestViaCompanionApp(
      test_server, "policy", "GoogleDMToken", dm_token, net::HTTP_OK,
      [&dm_token, &omaha_settings, first_request, rotate_public_key] {
        std::unique_ptr<::enterprise_management::DeviceManagementResponse>
            dm_response = GetDMResponseForOmahaPolicy(
                first_request, rotate_public_key,
                DMPolicyBuilderForTesting::SigningOption::kSignNormally,
                dm_token,
                device_management_storage::GetDefaultDMStorage()->GetDeviceID(),
                omaha_settings);
        return dm_response->SerializeAsString();
      }(),
      target_url);
}

void ExpectProxyPacScriptRequest(ScopedServer* test_server) {
  test_server->ExpectOnce(
      {request::GetPathMatcher(test_server->proxy_pac_path()),
       request::GetHeaderMatcher(
           {{"User-Agent", "WinHttp-Autoproxy-Service.*"}})},
      base::StringPrintf(
          "function FindProxyForURL(url, host) { return \"PROXY %s\"; }",
          test_server->host_port_pair().c_str()));
}

}  // namespace updater::test
