// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_policy_builder_for_testing.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/request_matcher.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/unit_test_util.h"
#include "chrome/updater/util/util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include <unistd.h>

#include "base/environment.h"
#include "base/strings/strcat.h"
#include "chrome/updater/util/posix_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/win_constants.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater::test {
namespace {

namespace enterprise_management =
    ::wireless_android_enterprise_devicemanagement;
using enterprise_management::ApplicationSettings;
using enterprise_management::OmahaSettingsClientProto;

#if BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

void ExpectNoUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id) {
  test_server->ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                           request::GetContentMatcher({base::StringPrintf(
                               R"(.*"appid":"%s".*)", app_id.c_str())})},
                          base::StringPrintf(")]}'\n"
                                             R"({"response":{)"
                                             R"(  "protocol":"3.1",)"
                                             R"(  "app":[)"
                                             R"(    {)"
                                             R"(      "appid":"%s",)"
                                             R"(      "status":"ok",)"
                                             R"(      "updatecheck":{)"
                                             R"(        "status":"noupdate")"
                                             R"(      })"
                                             R"(    })"
                                             R"(  ])"
                                             R"(}})",
                                             app_id.c_str()));
}

#endif  // BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace

class IntegrationTest : public ::testing::Test {
 public:
  IntegrationTest() : test_commands_(CreateIntegrationTestCommands()) {}
  ~IntegrationTest() override = default;

 protected:
  void SetUp() override {
    VLOG(2) << __func__ << " entered.";

    ASSERT_NO_FATAL_FAILURE(CleanProcesses());
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(Clean());
    ASSERT_NO_FATAL_FAILURE(ExpectClean());
    ASSERT_NO_FATAL_FAILURE(EnterTestMode(
        GURL("http://localhost:1234"), GURL("http://localhost:1235"),
        GURL("http://localhost:1236"), base::Minutes(5)));
    ASSERT_NO_FATAL_FAILURE(SetMachineManaged(false));
#if BUILDFLAG(IS_LINUX)
    // On LUCI the XDG_RUNTIME_DIR and DBUS_SESSION_BUS_ADDRESS environment
    // variables may not be set. These are required for systemctl to connect to
    // its bus in user mode.
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    const std::string xdg_runtime_dir =
        base::StrCat({"/run/user/", base::NumberToString(getuid())});
    if (!env->HasVar("XDG_RUNTIME_DIR")) {
      ASSERT_TRUE(env->SetVar("XDG_RUNTIME_DIR", xdg_runtime_dir));
    }
    if (!env->HasVar("DBUS_SESSION_BUS_ADDRESS")) {
      ASSERT_TRUE(
          env->SetVar("DBUS_SESSION_BUS_ADDRESS",
                      base::StrCat({"unix:path=", xdg_runtime_dir, "/bus"})));
    }
#endif

    // Mark the device as de-registered. This stops sending DM requests
    // that mess up the request expectations in the mock server.
    ASSERT_NO_FATAL_FAILURE(DMDeregisterDevice());

    VLOG(2) << __func__ << "completed.";
  }

  void TearDown() override {
    VLOG(2) << __func__ << " entered.";

    ExitTestMode();
    if (!HasFailure()) {
      ExpectClean();
    }
    ExpectNoCrashes();

    PrintLog();
    CopyLog();

    DMCleanup();

    // Updater process must not be running for `Clean()` to succeed.
    ASSERT_TRUE(WaitForUpdaterExit());
    Clean();

    VLOG(2) << __func__ << "completed.";
  }

  void ExpectNoCrashes() { test_commands_->ExpectNoCrashes(); }

  void CopyLog() { test_commands_->CopyLog(); }

  void PrintLog() { test_commands_->PrintLog(); }

  void Install() { test_commands_->Install(); }

  void InstallUpdaterAndApp(const std::string& app_id,
                            const bool is_silent_install,
                            const std::string& tag,
                            const std::string& child_window_text_to_find = {}) {
    test_commands_->InstallUpdaterAndApp(app_id, is_silent_install, tag,
                                         child_window_text_to_find);
  }

  void ExpectInstalled() { test_commands_->ExpectInstalled(); }

  void Uninstall() {
    ASSERT_TRUE(WaitForUpdaterExit());
    ExpectNoCrashes();
    PrintLog();
    CopyLog();
    test_commands_->Uninstall();
    ASSERT_TRUE(WaitForUpdaterExit());
  }

  void ExpectCandidateUninstalled() {
    test_commands_->ExpectCandidateUninstalled();
  }

  void Clean() { test_commands_->Clean(); }

  void ExpectClean() { test_commands_->ExpectClean(); }

  void EnterTestMode(const GURL& update_url,
                     const GURL& crash_upload_url,
                     const GURL& device_management_url,
                     const base::TimeDelta& idle_timeout) {
    test_commands_->EnterTestMode(update_url, crash_upload_url,
                                  device_management_url, idle_timeout);
  }

  void ExitTestMode() { test_commands_->ExitTestMode(); }

  void SetGroupPolicies(const base::Value::Dict& values) {
    test_commands_->SetGroupPolicies(values);
  }

  void SetPlatformPolicies(const base::Value::Dict& values) {
    test_commands_->SetPlatformPolicies(values);
  }

  void SetMachineManaged(bool is_managed_device) {
    test_commands_->SetMachineManaged(is_managed_device);
  }

  void ExpectVersionActive(const std::string& version) {
    test_commands_->ExpectVersionActive(version);
  }

  void ExpectVersionNotActive(const std::string& version) {
    test_commands_->ExpectVersionNotActive(version);
  }

#if BUILDFLAG(IS_WIN)
  void ExpectInterfacesRegistered() {
    test_commands_->ExpectInterfacesRegistered();
  }

  void ExpectMarshalInterfaceSucceeds() {
    test_commands_->ExpectMarshalInterfaceSucceeds();
  }

  void ExpectLegacyUpdate3WebSucceeds(
      const std::string& app_id,
      AppBundleWebCreateMode app_bundle_web_create_mode,
      int expected_final_state,
      int expected_error_code) {
    test_commands_->ExpectLegacyUpdate3WebSucceeds(
        app_id, app_bundle_web_create_mode, expected_final_state,
        expected_error_code);
  }

  void ExpectLegacyProcessLauncherSucceeds() {
    test_commands_->ExpectLegacyProcessLauncherSucceeds();
  }

  void ExpectLegacyAppCommandWebSucceeds(const std::string& app_id,
                                         const std::string& command_id,
                                         const base::Value::List& parameters,
                                         int expected_exit_code) {
    test_commands_->ExpectLegacyAppCommandWebSucceeds(
        app_id, command_id, parameters, expected_exit_code);
  }

  void ExpectLegacyPolicyStatusSucceeds() {
    test_commands_->ExpectLegacyPolicyStatusSucceeds();
  }

  void RunUninstallCmdLine() { test_commands_->RunUninstallCmdLine(); }

  void RunHandoff(const std::string& app_id) {
    test_commands_->RunHandoff(app_id);
  }

#endif  // BUILDFLAG(IS_WIN)

  void InstallAppViaService(
      const std::string& app_id,
      const base::Value::Dict& expected_final_values = {}) {
    test_commands_->InstallAppViaService(app_id, expected_final_values);
  }

  void SetupFakeUpdaterHigherVersion() {
    test_commands_->SetupFakeUpdaterHigherVersion();
  }

  void SetupFakeUpdaterLowerVersion() {
    test_commands_->SetupFakeUpdaterLowerVersion();
  }

  void SetupRealUpdaterLowerVersion() {
    test_commands_->SetupRealUpdaterLowerVersion();
  }

  void SetActive(const std::string& app_id) {
    test_commands_->SetActive(app_id);
  }

  void ExpectActive(const std::string& app_id) {
    test_commands_->ExpectActive(app_id);
  }

  void ExpectNotActive(const std::string& app_id) {
    test_commands_->ExpectNotActive(app_id);
  }

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) {
    test_commands_->SetExistenceCheckerPath(app_id, path);
  }

  void SetServerStarts(int value) { test_commands_->SetServerStarts(value); }

  void FillLog() { test_commands_->FillLog(); }

  void ExpectLogRotated() { test_commands_->ExpectLogRotated(); }

  void ExpectRegistered(const std::string& app_id) {
    test_commands_->ExpectRegistered(app_id);
  }

  void ExpectNotRegistered(const std::string& app_id) {
    test_commands_->ExpectNotRegistered(app_id);
  }

  void ExpectAppTag(const std::string& app_id, const std::string& tag) {
    test_commands_->ExpectAppTag(app_id, tag);
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) {
    test_commands_->ExpectAppVersion(app_id, version);
  }

  void InstallApp(
      const std::string& app_id,
      const base::Version& version = base::Version("0.1"),
      base::FunctionRef<void()> post_install_action = []() {}) {
    test_commands_->InstallApp(app_id, version);
    post_install_action();
  }

  void UninstallApp(const std::string& app_id) {
    test_commands_->UninstallApp(app_id);
  }

  void RunWake(int exit_code) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWake(exit_code);
  }

  void RunWakeAll() {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeAll();
  }

  void RunCrashMe() { test_commands_->RunCrashMe(); }

  void RunWakeActive(int exit_code) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeActive(exit_code);
  }

  void RunServer(int exit_code, bool internal) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunServer(exit_code, internal);
  }

  void CheckForUpdate(const std::string& app_id) {
    test_commands_->CheckForUpdate(app_id);
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index) {
    test_commands_->Update(app_id, install_data_index);
  }

  void UpdateAll() { test_commands_->UpdateAll(); }

  void GetAppStates(const base::Value::Dict& expected_app_states) {
    test_commands_->GetAppStates(expected_app_states);
  }

  void DeleteUpdaterDirectory() { test_commands_->DeleteUpdaterDirectory(); }

  void DeleteActiveUpdaterExecutable() {
    test_commands_->DeleteActiveUpdaterExecutable();
  }

  void DeleteFile(const base::FilePath& path) {
    test_commands_->DeleteFile(path);
  }

  base::FilePath GetDifferentUserPath() {
    return test_commands_->GetDifferentUserPath();
  }

  [[nodiscard]] bool WaitForUpdaterExit() {
    return test_commands_->WaitForUpdaterExit();
  }

  void ExpectUpdateCheckRequest(ScopedServer* test_server) {
    test_commands_->ExpectUpdateCheckRequest(test_server);
  }

  void ExpectUpdateCheckSequence(ScopedServer* test_server,
                                 const std::string& app_id,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version) {
    test_commands_->ExpectUpdateCheckSequence(test_server, app_id, priority,
                                              from_version, to_version);
  }

  void ExpectUninstallPing(ScopedServer* test_server) {
    test_commands_->ExpectUninstallPing(test_server);
  }

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            UpdateService::Priority priority,
                            const base::Version& from_version,
                            const base::Version& to_version) {
    test_commands_->ExpectUpdateSequence(test_server, app_id,
                                         install_data_index, priority,
                                         from_version, to_version);
  }

  void ExpectUpdateSequenceBadHash(ScopedServer* test_server,
                                   const std::string& app_id,
                                   const std::string& install_data_index,
                                   UpdateService::Priority priority,
                                   const base::Version& from_version,
                                   const base::Version& to_version) {
    test_commands_->ExpectUpdateSequenceBadHash(test_server, app_id,
                                                install_data_index, priority,
                                                from_version, to_version);
  }

  void ExpectSelfUpdateSequence(ScopedServer* test_server) {
    test_commands_->ExpectSelfUpdateSequence(test_server);
  }

  void ExpectInstallSequence(ScopedServer* test_server,
                             const std::string& app_id,
                             const std::string& install_data_index,
                             UpdateService::Priority priority,
                             const base::Version& from_version,
                             const base::Version& to_version) {
    test_commands_->ExpectInstallSequence(test_server, app_id,
                                          install_data_index, priority,
                                          from_version, to_version);
  }

  void StressUpdateService() { test_commands_->StressUpdateService(); }

  void CallServiceUpdate(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::PolicySameVersionUpdate policy_same_version_update) {
    test_commands_->CallServiceUpdate(app_id, install_data_index,
                                      policy_same_version_update);
  }

  void SetupFakeLegacyUpdater() { test_commands_->SetupFakeLegacyUpdater(); }

#if BUILDFLAG(IS_WIN)
  void RunFakeLegacyUpdater() { test_commands_->RunFakeLegacyUpdater(); }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  void PrivilegedHelperInstall() { test_commands_->PrivilegedHelperInstall(); }
#endif  // BUILDFLAG(IS_WIN)

  void ExpectAppInstalled(const std::string& appid,
                          const base::Version& expected_version) {
    ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, expected_version));

    // Verify installed app artifacts.
#if BUILDFLAG(IS_WIN)
    std::wstring pv;
    EXPECT_EQ(
        ERROR_SUCCESS,
        base::win::RegKey(UpdaterScopeToHKeyRoot(UpdaterScope::kSystem),
                          GetAppClientsKey(appid).c_str(), Wow6432(KEY_READ))
            .ReadValue(kRegValuePV, &pv));
    EXPECT_EQ(pv, base::ASCIIToWide(expected_version.GetString()));
#else
    const base::FilePath app_json_path =
        GetInstallDirectory(UpdaterScope::kSystem)
            ->DirName()
            .AppendASCII(appid)
            .AppendASCII("app.json");
    JSONFileValueDeserializer parser(app_json_path,
                                     base::JSON_ALLOW_TRAILING_COMMAS);
    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> app_data(
        parser.Deserialize(&error_code, &error_message));
    EXPECT_EQ(error_code, 0)
        << "Failed to load app json file at: " << app_json_path;
    EXPECT_TRUE(app_data);
    EXPECT_TRUE(app_data->is_dict());
    const base::Value::Dict& app_info = app_data->GetDict();
    EXPECT_EQ(*app_info.FindString("app"), appid);
    EXPECT_EQ(*app_info.FindString("company"), COMPANY_SHORTNAME_STRING);
    EXPECT_EQ(*app_info.FindString("pv"), expected_version.GetString());
#endif  // BUILDFLAG(IS_WIN)
  }

  base::FilePath GetInstallerPath(const std::string& installer) const {
    return base::FilePath::FromASCII("test_installer").AppendASCII(installer);
  }

  void ExpectLegacyUpdaterMigrated() {
    test_commands_->ExpectLegacyUpdaterMigrated();
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) {
    test_commands_->RunRecoveryComponent(app_id, version);
  }

  void SetLastChecked(const base::Time& time) {
    test_commands_->SetLastChecked(time);
  }

  void ExpectLastChecked() { test_commands_->ExpectLastChecked(); }

  void ExpectLastStarted() { test_commands_->ExpectLastStarted(); }

  void RunOfflineInstall(bool is_legacy_install, bool is_silent_install) {
    test_commands_->RunOfflineInstall(is_legacy_install, is_silent_install);
  }

  void RunOfflineInstallOsNotSupported(bool is_legacy_install,
                                       bool is_silent_install) {
    test_commands_->RunOfflineInstallOsNotSupported(is_legacy_install,
                                                    is_silent_install);
  }

  void DMPushEnrollmentToken(const std::string& enrollment_token) {
    test_commands_->DMPushEnrollmentToken(enrollment_token);
  }

  void DMDeregisterDevice() { test_commands_->DMDeregisterDevice(); }

  void DMCleanup() { test_commands_->DMCleanup(); }

  scoped_refptr<IntegrationTestCommands> test_commands_;

 private:
  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
};

// TODO(crbug.com/1424548): re-enable the tests once they are passing on
// Windows ARM64.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_InstallLowerVersion DISABLED_InstallLowerVersion
#define MAYBE_OverinstallBroken DISABLED_OverinstallBroken
#define MAYBE_OverinstallBrokenSameVersion DISABLED_OverinstallBrokenSameVersion
#define MAYBE_OverinstallWorking DISABLED_OverinstallWorking
#define MAYBE_SelfUpdateFromOldReal DISABLED_SelfUpdateFromOldReal
#define MAYBE_UninstallIfUnusedSelfAndOldReal \
  DISABLED_UninstallIfUnusedSelfAndOldReal
#else
#define MAYBE_InstallLowerVersion InstallLowerVersion
#define MAYBE_SelfUpdateFromOldReal SelfUpdateFromOldReal
#define MAYBE_UninstallIfUnusedSelfAndOldReal UninstallIfUnusedSelfAndOldReal
#define MAYBE_OverinstallBrokenSameVersion OverinstallBrokenSameVersion
#define MAYBE_OverinstallWorking OverinstallWorking
#define MAYBE_OverinstallBroken OverinstallBroken

#endif  // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)

// TODO(crbug.com/1492981): re-enable the test once it's not flaky with ASAN
// build.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_UpdateServiceStress DISABLED_UpdateServiceStress
#else
#define MAYBE_UpdateServiceStress UpdateServiceStress
#endif

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

// Tests the setup and teardown of the fixture.
TEST_F(IntegrationTest, DoNothing) {}

TEST_F(IntegrationTest, Install) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
#if BUILDFLAG(IS_WIN)
  // Tests the COM registration after the install. For now, tests that the
  // COM interfaces are registered, which is indirectly testing the type
  // library separation for the public, private, and legacy interfaces.
  ASSERT_NO_FATAL_FAILURE(ExpectInterfacesRegistered());
#endif  // BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests running the installer when the updater is already installed at the
// same version. It should have no notable effect.
TEST_F(IntegrationTest, OverinstallRedundant) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MAYBE_OverinstallWorking) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // A new version hands off installation to the old version, and doesn't
  // change the active version of the updater.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MAYBE_OverinstallBroken) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(DeleteActiveUpdaterExecutable());

  // Since the old version is not working, the new version should install and
  // become active.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());

  // Cleanup the older version by reinstalling and uninstalling.
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MAYBE_OverinstallBrokenSameVersion) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(DeleteActiveUpdaterExecutable());

  // Since the existing version is now not working, it should reinstall. This
  // will ultimately result in no visible change to the prefs file since the
  // new active version number will be the same as the old one.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterHigherVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectCandidateUninstalled());
  // The candidate uninstall should not have altered global prefs.
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive("0.0.0.0"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // Do not call `Uninstall()` since the outdated updater uninstalled itself.
  // Additional clean up is needed because of how this test is set up. After
  // the outdated instance uninstalls, a few files are left in the product
  // directory: prefs.json, updater.log, and overrides.json. These files are
  // owned by the active instance of the updater but in this case there is
  // no active instance left; therefore, explicit clean up is required.
  PrintLog();
  CopyLog();
  Clean();
}

TEST_F(IntegrationTest, QualifyUpdater) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // This instance is now qualified and should activate itself and check itself
  // for updates on the next check.
  test_server.ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                          request::GetContentMatcher(
                              {base::StringPrintf(".*%s.*", kUpdaterAppId)})},
                         ")]}'\n");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CleanupOldVersion) {
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());

  // Since the old version is not working, the new version should install and
  // become active.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  // Waking the new version should clean up the old.
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  absl::optional<base::FilePath> path = GetInstallDirectory(GetTestScope());
  ASSERT_TRUE(path);
  int dirs = 0;
  base::FileEnumerator(*path, false, base::FileEnumerator::DIRECTORIES)
      .ForEach([&dirs](const base::FilePath& path) {
        if (base::Version(path.BaseName().MaybeAsASCII()).IsValid()) {
          ++dirs;
        }
      });
  EXPECT_EQ(dirs, 1);

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdateWithWakeAll) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  base::Version next_version(base::StringPrintf("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWakeAll());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ReportsActive) {
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and InstallApp cannot make progress until it shut downs and
  // releases the global prefs lock.
  ASSERT_GE(TestTimeouts::action_timeout(), base::Seconds(18));
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Register apps test1 and test2. Expect pings for each.
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  // Set test1 to be active and do a background updatecheck.
  ASSERT_NO_FATAL_FAILURE(SetActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"(.*"appid":"test1","enabled":true,"ping":{"a":-2,.*)"})},
      R"()]}')"
      "\n"
      R"({"response":{"protocol":"3.1","daystart":{"elapsed_)"
      R"(days":5098}},"app":[{"appid":"test1","status":"ok",)"
      R"("updatecheck":{"status":"noupdate"}},{"appid":"test2",)"
      R"("status":"ok","updatecheck":{"status":"noupdate"}}]})");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  // The updater has cleared the active bits.
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests calling `CheckForUpdate` when the updater is not installed.
TEST_F(IntegrationTest, CheckForUpdate_UpdaterNotInstalled) {
  scoped_refptr<UpdateService> update_service =
      CreateUpdateServiceProxy(GetTestScope());
  base::RunLoop loop;
  update_service->CheckForUpdate(
      "test", UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, base::DoNothing(),
      base::BindLambdaForTesting([&loop](UpdateService::Result result) {
        EXPECT_TRUE(result == UpdateService::Result::kServiceFailed ||
                    result == UpdateService::Result::kIPCConnectionFailed)
            << "result == " << result;
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IntegrationTest, CheckForUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      &test_server, kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateBadHash) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequenceBadHash(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateApp) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, kInstallDataIndex,
                           UpdateService::Priority::kForeground, v1, v2));
  ASSERT_NO_FATAL_FAILURE(Update(kAppId, kInstallDataIndex));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));
  ASSERT_NO_FATAL_FAILURE(ExpectLastChecked());
  ASSERT_NO_FATAL_FAILURE(ExpectLastStarted());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTest, UpdateAppSucceedsEvenAfterDeletingInterfaces) {
  if (!::IsUserAnAdmin()) {
    GTEST_SKIP() << "Need admin privileges to run this test";
  }

  // Skips `DUMP_WILL_BE_CHECK` when running this test.
  base::win::RegKey(HKEY_LOCAL_MACHINE, UPDATER_DEV_KEY, KEY_WRITE)
      .WriteValue(kRegValueIntegrationTestMode, 1);
  absl::Cleanup remove_test_mode = [] {
    base::win::RegKey(HKEY_LOCAL_MACHINE, UPDATER_DEV_KEY, DELETE)
        .DeleteValue(kRegValueIntegrationTestMode);
  };

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());

  const UpdaterScope scope = GetTestScope();
  ASSERT_TRUE(AreComInterfacesPresent(scope, true));
  ASSERT_TRUE(AreComInterfacesPresent(scope, false));
  // Delete IUpdaterXXX, used by `InstallApp` via `RegisterApp`.
  // Delete IUpdaterInternal, used by the `wake` task.
  {
    for (const IID& iid : [&scope]() -> std::vector<IID> {
           switch (scope) {
             case UpdaterScope::kUser:
               return {
                   __uuidof(IUpdaterUser),
                   __uuidof(IUpdaterCallbackUser),
                   __uuidof(IUpdaterInternalUser),
                   __uuidof(IUpdaterInternalCallbackUser),
               };
             case UpdaterScope::kSystem:
               return {
                   __uuidof(IUpdaterSystem),
                   __uuidof(IUpdaterCallbackSystem),
                   __uuidof(IUpdaterInternalSystem),
                   __uuidof(IUpdaterInternalCallbackSystem),
               };
           }
         }()) {
      LONG result =
          base::win::RegKey(UpdaterScopeToHKeyRoot(scope), L"", DELETE)
              .DeleteKey(GetComIidRegistryPath(iid).c_str());
      ASSERT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    }
  }
  ASSERT_FALSE(AreComInterfacesPresent(scope, true));
  ASSERT_FALSE(AreComInterfacesPresent(scope, false));

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, kInstallDataIndex,
                           UpdateService::Priority::kForeground, v1, v2));
  ASSERT_NO_FATAL_FAILURE(Update(kAppId, kInstallDataIndex));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));
  ASSERT_NO_FATAL_FAILURE(ExpectLastChecked());
  ASSERT_NO_FATAL_FAILURE(ExpectLastStarted());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(IntegrationTest, NoCheckWhenLastCheckedRecently) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Minutes(5)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoCheckWhenLastCheckedRecentlyPolicy) {
  ScopedServer test_server(test_commands_);
  base::Value::Dict group_policies;
  group_policies.Set("autoupdatecheckperiodminutes", 60 * 18);
  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Hours(12)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoCheckWhenSuppressed) {
  ScopedServer test_server(test_commands_);
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  base::Value::Dict group_policies;
  group_policies.Set("updatessuppressedstarthour", (now.hour - 1 + 24) % 24);
  group_policies.Set("updatessuppressedstartmin", 0);
  group_policies.Set("updatessuppresseddurationmin", 120);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallUpdaterAndApp) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(
      InstallUpdaterAndApp(kAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallUpdaterAndTwoApps) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const std::string kAppId2("test2");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo&usagestats=1"})));
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId2, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId2, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId2, "&ap=foo2&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId2, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId, "foo"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId2, "foo2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ChangeTag) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo&usagestats=1"})));
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({1}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo2&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId, "foo2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTest, Handoff) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  const base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(RunHandoff(kAppId));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ForceInstallApp) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  base::Value::Dict group_policies;
  group_policies.Set("installtest1", IsSystemInstall(GetTestScope())
                                         ? kPolicyForceInstallMachine
                                         : kPolicyForceInstallUser);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));

  const std::string kAppId("test1");
  base::Version v0point1("0.1");
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      &test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.0.0.0"), v0point1));
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kAppId, "",
                           UpdateService::Priority::kBackground, v0point1, v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(IntegrationTest, MultipleWakesOneNetRequest) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  // Only one sequence visible to the server despite multiple wakes.
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MultipleUpdateAllsMultipleNetRequests) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(&test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, GetAppStates) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  const base::Version v1("0.1");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  base::Value::Dict expected_app_state;
  expected_app_state.Set("app_id", kAppId);
  expected_app_state.Set("version", v1.GetString());
  expected_app_state.Set("ap", "");
  expected_app_state.Set("brand_code", "");
  expected_app_state.Set("brand_path", "");
  expected_app_state.Set("ecp", "");
  base::Value::Dict expected_app_states;
  expected_app_states.Set(kAppId, std::move(expected_app_state));

  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTest, MarshalInterface) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectMarshalInterfaceSucceeds());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyProcessLauncher) {
  // TODO(crbug.com/1453749): Remove procmon logging once flakiness is fixed.
  const base::ScopedClosureRunner stop_procmon_logging(
      base::BindOnce(&updater::test::StopProcmonLogging,
                     updater::test::StartProcmonLogging()));

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyProcessLauncherSucceeds());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyAppCommandWeb) {
  ASSERT_NO_FATAL_FAILURE(Install());

  const char kAppId[] = "test1";
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));

  base::Value::List parameters;
  parameters.Append("5432");
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyAppCommandWebSucceeds(kAppId, "command1", parameters, 5432));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, LegacyPolicyStatus) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      &test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectLegacyPolicyStatusSucceeds());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UninstallCmdLine) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  // Running the uninstall command does not uninstall this instance of the
  // updater right after installing it (not enough server starts).
  ASSERT_NO_FATAL_FAILURE(RunUninstallCmdLine());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));

  // Uninstall the idle updater.
  ASSERT_NO_FATAL_FAILURE(RunUninstallCmdLine());
  ASSERT_TRUE(WaitForUpdaterExit());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(UninstallApp("test1"));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UninstallIfMaxServerWakesBeforeRegistrationExceeded) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, UninstallUpdaterWhenAllAppsUninstalled) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(UninstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, RotateLog) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(FillLog());
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectLogRotated());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Windows does not currently have a concept of app ownership, so this
// test need not run on Windows.
#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, UnregisterUnownedApp) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(
      "test1", IsSystemInstall(GetTestScope()) ? temp_dir.GetPath()
                                               : GetDifferentUserPath()));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Since the updater may have chowned the temp dir, we may need to elevate to
  // delete it.
  ASSERT_NO_FATAL_FAILURE(DeleteFile(temp_dir.GetPath()));

  if (IsSystemInstall(GetTestScope())) {
    ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test1"));
  } else {
    ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  }

  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// The privileged helper only exists on macOS. This does not test installation
// of the helper itself, but is meant to cover its core functionality.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(IntegrationTest, PrivilegedHelperInstall) {
  if (GetTestScope() != UpdaterScope::kSystem) {
    return;  // Test is only applicable to system scope.
  }
  ASSERT_NO_FATAL_FAILURE(PrivilegedHelperInstall());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion("test1", base::Version("1.2.3.4")));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if !defined(COMPONENT_BUILD)
TEST_F(IntegrationTest, MAYBE_SelfUpdateFromOldReal) {
  ScopedServer test_server(test_commands_);

  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // Trigger an old instance update check.
  ASSERT_NO_FATAL_FAILURE(ExpectSelfUpdateSequence(&test_server));
  ASSERT_NO_FATAL_FAILURE(RunWakeActive(0));

  // Qualify the new instance.
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Activate the new instance. (It should not check itself for updates.)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MAYBE_UninstallIfUnusedSelfAndOldReal) {
  ScopedServer test_server(test_commands_);

  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // Trigger an old instance update check.
  ASSERT_NO_FATAL_FAILURE(ExpectSelfUpdateSequence(&test_server));
  ASSERT_NO_FATAL_FAILURE(RunWakeActive(0));

  // Qualify the new instance.
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(&test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Activate the new instance. (It should not check itself for updates.)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Expect that the updater uninstalled itself as well as the lower version.
}

// Tests that installing and uninstalling an old version of the updater from
// CIPD is possible.
TEST_F(IntegrationTest, MAYBE_InstallLowerVersion) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(Uninstall());

#if BUILDFLAG(IS_WIN)
  // This deletes a tree of empty subdirectories corresponding to the crash
  // handler of the lower version updater installed above. `Uninstall` runs
  // `updater --uninstall` from the out directory of the build, which attempts
  // to launch the `uninstall.cmd` script corresponding to this version of the
  // updater from the install directory. However, there is no such script
  // because this version was never installed, and the script is not found
  // there.
  ASSERT_NO_FATAL_FAILURE(DeleteUpdaterDirectory());
#endif  // IS_WIN
}

#endif
#endif

TEST_F(IntegrationTest, MAYBE_UpdateServiceStress) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(StressUpdateService());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, IdleServerExits) {
#if BUILDFLAG(IS_WIN)
  if (GetTestScope() == UpdaterScope::kSystem) {
    GTEST_SKIP() << "System server startup is complicated on Windows.";
  }
#endif
  ASSERT_NO_FATAL_FAILURE(EnterTestMode(
      GURL("http://localhost:1234"), GURL("http://localhost:1234"),
      GURL("http://localhost:1234"), base::Seconds(1)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunServer(kErrorIdle, true));
  ASSERT_NO_FATAL_FAILURE(RunServer(kErrorIdle, false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SameVersionUpdate) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string app_id = "test-appid";
  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));

  const std::string response = base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"noupdate")"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str());
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"("updatecheck":{"sameversionupdate":true},"version":"0.1"}.*)"})},
      response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kAllowed));

  test_server.ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                          request::GetContentMatcher(
                              {R"(.*"updatecheck":{},"version":"0.1"}.*)"})},
                         response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kNotAllowed));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallDataIndex) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string app_id = "test-appid";
  const std::string install_data_index = "test-install-data-index";

  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));

  const std::string response = base::StringPrintf(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"3.1",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"noupdate")"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str());

  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher({base::StringPrintf(
           R"(.*"data":\[{"index":"%s","name":"install"}],.*)",
           install_data_index.c_str())})},
      response);

  ASSERT_NO_FATAL_FAILURE(
      CallServiceUpdate(app_id, install_data_index,
                        UpdateService::PolicySameVersionUpdate::kAllowed));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(&test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MigrateLegacyUpdater) {
  ASSERT_NO_FATAL_FAILURE(SetupFakeLegacyUpdater());
#if BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(RunFakeLegacyUpdater());
#endif  // BUILDFLAG(IS_WIN)
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdaterMigrated());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RecoveryNoUpdater) {
  const std::string appid = "test1";
  const base::Version version("0.1");
  ASSERT_NO_FATAL_FAILURE(RunRecoveryComponent(appid, version));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, version));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_WIN) && !defined(COMPONENT_BUILD)
// TODO(crbug.com/1281688): standalone installers are supported on Windows only.
TEST_F(IntegrationTest, OfflineInstall) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOsNotSupported) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(
      RunOfflineInstallOsNotSupported(/*is_legacy_install=*/false,
                                      /*is_silent_install=*/false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallSilent) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/false,
                                            /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOsNotSupportedSilent) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(
      RunOfflineInstallOsNotSupported(/*is_legacy_install=*/false,
                                      /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallSilentLegacy) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunOfflineInstall(/*is_legacy_install=*/true,
                                            /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OfflineInstallOsNotSupportedSilentLegacy) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(
      RunOfflineInstallOsNotSupported(/*is_legacy_install=*/true,
                                      /*is_silent_install=*/true));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN) && !defined(COMPONENT_BUILD)

TEST_F(IntegrationTest, CrashUsageStatsEnabled) {
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
  GTEST_SKIP() << "Crash tests disabled for Win ASAN.";
#else
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  const std::string response;
  test_server.ExpectOnce(
      {
          request::GetPathMatcher(
              base::StringPrintf(R"(%s\?product=%s&version=%s&guid=.*)",
                                 test_server.crash_report_path().c_str(),
                                 CRASH_PRODUCT_NAME, kUpdaterVersion)),
          request::GetHeaderMatcher("User-Agent", R"(Crashpad/.*)"),
          request::GetMultipartContentMatcher({
              {"guid", std::vector<std::string>({})},  // Crash guid.
              {"process_type", std::vector<std::string>({R"(updater)"})},
              {"prod", std::vector<std::string>({CRASH_PRODUCT_NAME})},
              {"ver", std::vector<std::string>({kUpdaterVersion})},
              {"upload_file_minidump",  // Dump file name and its content.
               std::vector<std::string>(
                   {R"(filename=".*dmp")",
                    R"(Content-Type: application/octet-stream)", R"(MDMP)"})},
          }),
      },
      response);
  ExpectUninstallPing(&test_server);
  RunCrashMe();
  ASSERT_TRUE(WaitForUpdaterExit());

  // Delete the dmp files generated by this test, so `ExpectNoCrashes` won't
  // complain at TearDown.
  absl::optional<base::FilePath> database_path(
      GetCrashDatabasePath(GetTestScope()));
  if (database_path && base::PathExists(*database_path)) {
    base::FileEnumerator(*database_path, true, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.dmp"),
                         base::FileEnumerator::FolderSearchPolicy::ALL)
        .ForEach([](const base::FilePath& name) {
          VLOG(0) << "Deleting file at: " << name;
          EXPECT_TRUE(base::DeleteFile(name));
        });
  }
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif
}

#if BUILDFLAG(IS_WIN)
class IntegrationTestLegacyUpdate3Web : public IntegrationTest {
 public:
  IntegrationTestLegacyUpdate3Web() = default;
  ~IntegrationTestLegacyUpdate3Web() override = default;

 protected:
  void SetUp() override {
    IntegrationTest::SetUp();

    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    ASSERT_NO_FATAL_FAILURE(Install());
    ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  }

  void TearDown() override {
    ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
    ASSERT_NO_FATAL_FAILURE(Uninstall());

    IntegrationTest::TearDown();
  }

  std::unique_ptr<ScopedServer> test_server_;
  static constexpr char kAppId[] = "test1";
};

TEST_F(IntegrationTestLegacyUpdate3Web, NoUpdate) {
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server_.get(), kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_NO_UPDATE,
      S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, DisabledPolicyManual) {
  ASSERT_TRUE(WaitForUpdaterExit());
  base::Value::Dict group_policies;
  group_policies.Set("updatetest1", kPolicyAutomaticUpdatesOnly);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_ERROR,
      GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL));
}

TEST_F(IntegrationTestLegacyUpdate3Web, DisabledPolicy) {
  ASSERT_TRUE(WaitForUpdaterExit());
  base::Value::Dict group_policies;
  group_policies.Set("updatetest1", kPolicyDisabled);
  ASSERT_NO_FATAL_FAILURE(SetGroupPolicies(group_policies));
  ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp, STATE_ERROR,
      GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY);
}

TEST_F(IntegrationTestLegacyUpdate3Web, CheckForUpdate) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp,
      STATE_UPDATE_AVAILABLE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, Update) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server_.get(), kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdate3WebSucceeds(
      kAppId, AppBundleWebCreateMode::kCreateInstalledApp,
      STATE_INSTALL_COMPLETE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, CheckForInstall) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_UPDATE_AVAILABLE, S_OK));
}

TEST_F(IntegrationTestLegacyUpdate3Web, Install) {
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server_.get(), kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server_.get(), kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectLegacyUpdate3WebSucceeds(kAppId, AppBundleWebCreateMode::kCreateApp,
                                     STATE_INSTALL_COMPLETE, S_OK));
}
#endif  // BUILDFLAG(IS_WIN)

class IntegrationTestDeviceManagement : public IntegrationTest {
 public:
  IntegrationTestDeviceManagement() = default;
  ~IntegrationTestDeviceManagement() override = default;

 protected:
  struct TestApp {
    std::string appid;
    base::Version v1;
    std::string v1_crx;
    base::Version v2;
    std::string v2_crx;
  };

  void SetUp() override {
    IntegrationTest::SetUp();
    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    if (!IsSystemInstall(GetTestScope())) {
      GTEST_SKIP();
    }
    DMCleanup();
    ASSERT_NO_FATAL_FAILURE(SetMachineManaged(true));
  }

  void TearDown() override {
    DMCleanup();
    IntegrationTest::TearDown();
  }

  std::string BuildCommandLineArgs(UpdaterScope scope,
                                   const std::string& app_id,
                                   const base::Version& to_version) {
    return base::StringPrintf("%s --appid=%s --company=%s --product_version=%s",
                              IsSystemInstall(scope) ? "--system" : "",
                              app_id.c_str(), COMPANY_SHORTNAME_STRING,
                              to_version.GetString().c_str());
  }

  void InstallTestApp(const TestApp& app, bool install_v1 = true) {
    const base::Version version = install_v1 ? app.v1 : app.v2;
    const base::FilePath& installer_path =
        GetInstallerPath(install_v1 ? app.v1_crx : app.v2_crx);
    InstallApp(app.appid, version, [&]() {
      base::FilePath exe_path;
      ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
#if BUILDFLAG(IS_WIN)
      // Run test app installer to set app `pv` value to its initial
      // version.
      const std::wstring command(
          base::StrCat({base::CommandLine::QuoteForCommandLineToArgvW(
                            exe_path
                                .Append(installer_path.ReplaceExtension(
                                    FILE_PATH_LITERAL(".exe")))
                                .value()),
                        L" ",
                        base::ASCIIToWide(BuildCommandLineArgs(
                            GetTestScope(), app.appid, version))}));
      VLOG(2) << "Launch app setup command: " << command;
      base::Process process = base::LaunchProcess(command, {});
      if (!process.IsValid()) {
        VLOG(2) << "Invalid process launching command: " << command;
      }
      int exit_code = -1;
      EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                                 &exit_code));
      EXPECT_EQ(0, exit_code);
#else
      // Run test app installer to set app initial version artifacts.
      base::CommandLine command(exe_path
                   .Append(installer_path.DirName().AppendASCII(
                       "test_app_setup.sh")));
      command.AppendSwitchASCII("--appid", app.appid);
      command.AppendSwitchASCII("--company", COMPANY_SHORTNAME_STRING);
      command.AppendSwitchASCII("--product_version", version.GetString());
      VLOG(2) << "Launch app setup command: " << command.GetCommandLineString();
      base::Process process = base::LaunchProcess(MakeElevated(command), {});
      if (!process.IsValid()) {
        VLOG(2) << "Failed to launch the process";
      }
      int exit_code = -1;
      EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                                 &exit_code));
      EXPECT_EQ(0, exit_code);
      SetExistenceCheckerPath(app.appid,
          GetInstallDirectory(
              UpdaterScope::kSystem)->DirName().AppendASCII(app.appid));
#endif
    });

    ExpectAppInstalled(app.appid, version);
  }

  void SetCloudPolicyOverridesPlatformPolicy() {
// Cloud policy overrides platform policy default, except on Windows.
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(ERROR_SUCCESS,
              base::win::RegKey(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY,
                                Wow6432(KEY_WRITE))
                  .WriteValue(L"CloudPolicyOverridesPlatformPolicy", 1));
#endif  // BUILDFLAG(IS_WIN)
  }

  std::unique_ptr<ScopedServer> test_server_;
  static constexpr char kEnrollmentToken[] = "integration-enrollment-token";
  static constexpr char kDMToken[] = "integration-dm-token";

#if BUILDFLAG(IS_WIN)
  static constexpr char kGlobalPolicyKey[] = "";
  const TestApp kApp1 = {"test1", base::Version("1.0.0.0"),
                         "Testapp2Setup.crx3", base::Version("2.0.0.0"),
                         "Testapp2Setup.crx3"};
  const TestApp kApp2 = {"test2", base::Version("100.0.0.0"),
                         "Testapp2Setup.crx3", base::Version("101.0.0.0"),
                         "Testapp2Setup.crx3"};
  const TestApp kApp3 = {"test3", base::Version("1.0"), "Testapp2Setup.crx3",
                         base::Version("1.1"), "Testapp2Setup.crx3"};
#else
  static constexpr char kGlobalPolicyKey[] = "global";
  const TestApp kApp1 = {
      "test1", base::Version("1.0.0.0"), "test_installer_test1_v1.crx3",
      base::Version("2.0.0.0"), "test_installer_test1_v2.crx3"};
  const TestApp kApp2 = {
      "test2", base::Version("100.0.0.0"), "test_installer_test2_v1.crx3",
      base::Version("101.0.0.0"), "test_installer_test2_v2.crx3"};
  const TestApp kApp3 = {"test3", base::Version("1.0"),
                         "test_installer_test3_v1.crx3", base::Version("1.1"),
                         "test_installer_test3_v2.crx3"};
#endif  // BUILDFLAG(IS_WIN)
};

// Tests the setup and teardown of the fixture.
TEST_F(IntegrationTestDeviceManagement, Nothing) {}

TEST_F(IntegrationTestDeviceManagement, PolicyFetchBeforeInstall) {
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  omaha_settings.set_download_preference("not-cacheable");
  omaha_settings.set_proxy_mode("system");
  omaha_settings.set_proxy_server("test.proxy.server");
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  app.set_target_version_prefix("0.1");
  app.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);

  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  scoped_refptr<DMStorage> dm_storage = GetDefaultDMStorage();
  ASSERT_NE(dm_storage, nullptr);
  std::unique_ptr<OmahaSettingsClientProto> omaha_policy =
      dm_storage->GetOmahaPolicySettings();
  ASSERT_TRUE(omaha_policy != nullptr);
  EXPECT_EQ(omaha_policy->download_preference(), "not-cacheable");
  EXPECT_EQ(omaha_policy->proxy_mode(), "system");
  EXPECT_EQ(omaha_policy->proxy_server(), "test.proxy.server");
  ASSERT_GT(omaha_policy->application_settings_size(), 0);
  const ApplicationSettings& app_policy =
      omaha_policy->application_settings()[0];
  EXPECT_EQ(app_policy.app_guid(), kApp1.appid);
  EXPECT_EQ(app_policy.update(), enterprise_management::AUTOMATIC_UPDATES_ONLY);
  EXPECT_EQ(app_policy.target_version_prefix(), "0.1");
  EXPECT_EQ(app_policy.rollback_to_target_version(),
            enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(COMPONENT_BUILD)

TEST_F(IntegrationTestDeviceManagement, AppInstall) {
  const base::Version kApp1Version = base::Version("1.0.0.0");
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_install(enterprise_management::INSTALL_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp1.appid, kApp1.v1),
              kApp1.appid, base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      }));

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp2.appid));
  ExpectAppInstalled(kApp1.appid, kApp1.v1);
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kApp2.appid));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, ForceInstall) {
  const base::Version kApp1Version = base::Version("1.0.0.0");

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Force-install app1, enable install app2.
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_install(enterprise_management::INSTALL_FORCED);
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_install(enterprise_management::INSTALL_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp1.appid, kApp1.v1),
              kApp1.appid, base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      });
  ExpectUpdateCheckRequest(test_server_.get());

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ExpectAppInstalled(kApp1.appid, kApp1.v1);
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kApp2.appid));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// This test depends on platform policy overriding cloud policy, which is not
// the default on POSIX. Therefore, this test is Windows only.
#if BUILDFLAG(IS_WIN)
TEST_F(IntegrationTestDeviceManagement, AppUpdateConflictPolicies) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp2, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp3, /*install_v1=*/true));

  base::Value::Dict policies;
  policies.Set(kApp2.appid, base::Value::Dict().Set("Update", kPolicyEnabled));
  ASSERT_NO_FATAL_FAILURE(SetPlatformPolicies(policies));

  // Cloud policy sets update default to disabled, app1 to auto-update, and
  // app2 to manual-update.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_update_default(enterprise_management::UPDATES_DISABLED);
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_update(enterprise_management::MANUAL_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp1.appid, kApp1.v2),
              kApp1.appid, kApp1.v1, kApp1.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v2_crx)),
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp2.appid, kApp2.v2),
              kApp2.appid, kApp2.v1, kApp2.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp2.v2_crx)),
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp3.appid, kApp3.v2),
              kApp3.appid, kApp3.v1, kApp3.v2,
              /*is_install=*/false,
              /*should_update=*/false, false, "", "",
              GetInstallerPath(kApp3.v2_crx)),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp2.appid, kApp2.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp3.appid, kApp3.v1));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp2.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp3.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(IntegrationTestDeviceManagement, CloudPolicyOverridesPlatformPolicy) {
  ASSERT_NO_FATAL_FAILURE(SetCloudPolicyOverridesPlatformPolicy());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp2, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp3, /*install_v1=*/true));

  base::Value::Dict policies;
  policies.Set(kGlobalPolicyKey, base::Value::Dict()
                                     .Set("UpdateDefault", kPolicyDisabled)
                                     .Set("DownloadPreference", "cacheable"));
  policies.Set(kApp1.appid, base::Value::Dict()
                                .Set("Update", kPolicyDisabled)
                                .Set("TargetChannel", "beta"));
  policies.Set(kApp2.appid, base::Value::Dict().Set("Update", kPolicyEnabled));
  policies.Set(kApp3.appid, base::Value::Dict()
                                .Set("Update", kPolicyEnabled)
                                .Set("TargetChannel", "canary"));
  ASSERT_NO_FATAL_FAILURE(SetPlatformPolicies(policies));

  // Overrides app1 to auto-update, app2 to manual-update with cloud policy.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  app1.set_target_channel("beta_canary");
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_update(enterprise_management::MANUAL_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/base::Value::Dict().Set("dlpref", "cacheable"),
      {
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp1.appid, kApp1.v2),
              kApp1.appid, kApp1.v1, kApp1.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "beta_canary",
              GetInstallerPath(kApp1.v2_crx)),
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp2.appid, kApp2.v2),
              kApp2.appid, kApp2.v1, kApp2.v1,
              /*is_install=*/false,
              /*should_update=*/false, false, "", "",
              GetInstallerPath(kApp2.v2_crx)),
          AppUpdateExpectation(
              BuildCommandLineArgs(GetTestScope(), kApp3.appid, kApp3.v2),
              kApp3.appid, kApp3.v1, kApp3.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "canary",
              GetInstallerPath(kApp3.v2_crx)),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp2.appid, kApp2.v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp3.appid, kApp3.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp2.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp3.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, RollbackToTargetVersion) {
  constexpr char kTargetVersionPrefix[] = "1.0.";

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/false));

  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(test_server_.get(),
                                            kEnrollmentToken, kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_target_version_prefix(kTargetVersionPrefix);
  app.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));
  ExpectDeviceManagementPolicyFetchRequest(test_server_.get(), kDMToken,
                                           omaha_settings);

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {AppUpdateExpectation(
          BuildCommandLineArgs(GetTestScope(), kApp1.appid, kApp1.v1),
          kApp1.appid, kApp1.v2, kApp1.v1,
          /*is_install=*/false,
          /*should_update=*/true, /*allow_rollback=*/true, kTargetVersionPrefix,
          "", GetInstallerPath(kApp1.v1_crx))});
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // !defined(COMPONENT_BUILD)

#if BUILDFLAG(IS_WIN)
class IntegrationTestMsi : public IntegrationTest {
 public:
  IntegrationTestMsi() = default;
  ~IntegrationTestMsi() override = default;

  static constexpr char kMsiAppId[] = "{c28fcf72-bcf2-45c5-8def-31a74ac02012}";

 protected:
  void SetUp() override {
    if (!IsSystemInstall(GetTestScope())) {
      GTEST_SKIP();
    }
    IntegrationTest::SetUp();
    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdInitialVersion));
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdUpdatedVersion));
  }

  void TearDown() override {
    if (!IsSystemInstall(GetTestScope())) {
      return;
    }
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdInitialVersion));
    ASSERT_NO_FATAL_FAILURE(RemoveMsiProductData(kMsiProductIdUpdatedVersion));
    IntegrationTest::TearDown();
  }

  void InstallMsiWithVersion(const base::Version& version) {
    InstallApp(kMsiAppId, version, [&]() {
      base::FilePath msi_path;
      ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &msi_path));
      msi_path = msi_path.Append(
          GetInstallerPath(base::StrCat({kMsiAppId, ".", version.GetString()}))
              .AppendASCII(kMsiCrx)
              .RemoveExtension());
      const std::wstring command = BuildMsiCommandLine({}, {}, msi_path);
      base::Process process = base::LaunchProcess(command, {});
      if (!process.IsValid()) {
        LOG(ERROR) << "Invalid process launching command: " << command;
      }
      int exit_code = -1;
      EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                                 &exit_code));
      EXPECT_EQ(0, exit_code);
    });

    ExpectAppInstalled(kMsiAppId, version);
  }

  void RemoveMsiProductData(const std::wstring& msi_product_id) {
    ASSERT_FALSE(msi_product_id.empty());
    for (const auto& [root, key] :
         {std::make_pair(HKEY_LOCAL_MACHINE,
                         L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Instal"
                         L"ler\\UserData\\S-1-5-18\\Products"),
          std::make_pair(HKEY_CLASSES_ROOT, L"Installer\\Products")}) {
      for (const auto& access_mask : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        base::win::RegKey(root, key, DELETE | access_mask)
            .DeleteKey(msi_product_id.c_str());
      }
    }
  }

  std::unique_ptr<ScopedServer> test_server_;
  static constexpr char kMsiCrx[] = "TestSystemMsiInstaller.msi.crx3";
  static constexpr wchar_t kMsiProductIdInitialVersion[] =
      L"40C670A26D240095081B31C3EDEF2BD2";
  static constexpr wchar_t kMsiProductIdUpdatedVersion[] =
      L"D2B2AC298EFCE2757A975961532CDE7D";
  const base::Version kMsiInitialVersion = base::Version("1.0.0.0");
  const base::Version kMsiUpdatedVersion = base::Version("2.0.0.0");
};

TEST_F(IntegrationTestMsi, Install) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const base::FilePath crx_path = GetInstallerPath(kMsiCrx);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation({}, kMsiAppId, base::Version({0, 0, 0, 0}),
                               kMsiUpdatedVersion,
                               /*is_install=*/true,
                               /*should_update=*/true, false, "", "", crx_path),
      });

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kMsiAppId));
  ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestMsi, InstallViaCommandLine) {
  const base::FilePath crx_path = GetInstallerPath(kMsiCrx);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation({}, kMsiAppId, base::Version({0, 0, 0, 0}),
                               kMsiUpdatedVersion,
                               /*is_install=*/true,
                               /*should_update=*/true, false, "", "", crx_path),
      });

  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kMsiAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion);

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestMsi, Upgrade) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  InstallMsiWithVersion(kMsiInitialVersion);

  const base::FilePath crx_path = GetInstallerPath(kMsiCrx);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation({}, kMsiAppId, kMsiInitialVersion,
                               kMsiUpdatedVersion,
                               /*is_install=*/false,
                               /*should_update=*/true, false, "", "", crx_path),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

struct IntegrationInstallerResultsTestCase {
  const bool interactive_install;
  const std::string command_line_args;
  const int error_code;
  const std::string installer_text;
  const std::string installer_cmd_line;
  const std::string custom_app_response;
};

class IntegrationInstallerResultsTest
    : public ::testing::WithParamInterface<IntegrationInstallerResultsTestCase>,
      public IntegrationTestMsi {};

INSTANTIATE_TEST_SUITE_P(
    IntegrationInstallerResultsTestCases,
    IntegrationInstallerResultsTest,
    ::testing::ValuesIn(std::vector<IntegrationInstallerResultsTestCase>{
        // InstallerResult::kMsiError, explicit error code.
        {
            false,
            "INSTALLER_RESULT=2 INSTALLER_ERROR=1603",
            1603,
            "Fatal error during installation. ",
            {},
            {},
        },

        // `InstallerResult::kCustomError`, implicit error code
        // `kErrorApplicationInstallerFailed`.
        {
            false,
            "INSTALLER_RESULT=1 INSTALLER_RESULT_UI_STRING=TestUIString",
            kErrorApplicationInstallerFailed,
            "TestUIString",
            {},
            {},
        },

        // InstallerResult::kSystemError, explicit error code.
        {
            false,
            "INSTALLER_RESULT=3 INSTALLER_ERROR=99",
            99,
            "0x63",
            {},
            {},
        },

        // InstallerResult::kSuccess.
        {
            false,
            "INSTALLER_RESULT=0",
            0,
            {},
            {},
            {},
        },

        // Silent install with a launch command, InstallerResult::kSuccess, will
        // not run `more.com` since silent install.
        {
            false,
            "INSTALLER_RESULT=0 "
            "REGISTER_LAUNCH_COMMAND=more.com",
            0,
            {},
            "more.com",
            {},
        },

        // InstallerResult::kMsiError, `ERROR_SUCCESS_REBOOT_REQUIRED`.
        {
            false,
            base::StrCat({"INSTALLER_RESULT=2 INSTALLER_ERROR=",
                          base::NumberToString(ERROR_SUCCESS_REBOOT_REQUIRED)}),
            ERROR_SUCCESS_REBOOT_REQUIRED,
            "The requested operation is successful. Changes will not be "
            "effective "
            "until the system is rebooted. ",
            {},
            {},
        },

        // Interactive install via the command line with a launch command,
        // InstallerResult::kSuccess, will run `more.com` since interactive
        // install.
        {
            true,
            "INSTALLER_RESULT=0 "
            "REGISTER_LAUNCH_COMMAND=more.com",
            0,
            {},
            "more.com",
            {},
        },

        // InstallerResult::kMsiError, `ERROR_SUCCESS_REBOOT_REQUIRED`.
        {
            true,
            base::StrCat({"INSTALLER_RESULT=2 INSTALLER_ERROR=",
                          base::NumberToString(ERROR_SUCCESS_REBOOT_REQUIRED)}),
            ERROR_SUCCESS_REBOOT_REQUIRED,
            base::WideToUTF8(GetLocalizedStringF(IDS_TEXT_RESTART_COMPUTER_BASE,
                                                 L"")),
            {},
            {},
        },

        // Interactive install via the command line,
        // `update_client::ProtocolError::UNKNOWN_APPLICATION` error.
        {
            true,
            "INSTALLER_RESULT=0",
            static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION),
            base::WideToUTF8(GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE)),
            {},
            base::StrCat({"{\"appid\":\"", IntegrationTestMsi::kMsiAppId,
                          "\",\"status\":\"error-unknownApplication\"}"}),
        },
    }));

TEST_P(IntegrationInstallerResultsTest, TestCases) {
  const base::FilePath crx_relative_path = GetInstallerPath(kMsiCrx);
  const bool should_install_successfully =
      !GetParam().error_code ||
      GetParam().error_code == ERROR_SUCCESS_REBOOT_REQUIRED;

  if (!GetParam().interactive_install) {
    ASSERT_NO_FATAL_FAILURE(Install());
    ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  }

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, test_server_.get(),
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              GetParam().command_line_args, kMsiAppId,
              base::Version({0, 0, 0, 0}), kMsiUpdatedVersion,
              /*is_install=*/true, should_install_successfully, false, "", "",
              crx_relative_path,
              /*always_serve_crx=*/GetParam().custom_app_response.empty(),
              UpdateService::ErrorCategory::kInstall, GetParam().error_code,
              /*EVENT_INSTALL_COMPLETE=*/2, GetParam().custom_app_response),
      });
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server_.get()));

  if (GetParam().interactive_install) {
    ASSERT_NO_FATAL_FAILURE(
        InstallUpdaterAndApp(kMsiAppId, /*is_silent_install=*/false,
                             "usagestats=1", GetParam().installer_text));
    ASSERT_TRUE(WaitForUpdaterExit());
  } else {
    int64_t crx_file_size = 0;
    {
      base::FilePath exe_path;
      ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
      const base::FilePath crx_path = exe_path.Append(crx_relative_path);
      EXPECT_TRUE(base::GetFileSize(crx_path, &crx_file_size));
    }
    ASSERT_NO_FATAL_FAILURE(InstallAppViaService(
        kMsiAppId,
        base::Value::Dict()
            .Set("expected_update_state",
                 base::Value::Dict()
                     .Set("app_id", kMsiAppId)
                     .Set("state",
                          static_cast<int>(
                              should_install_successfully
                                  ? UpdateService::UpdateState::State::kUpdated
                                  : UpdateService::UpdateState::State::
                                        kUpdateError))
                     .Set("next_version", kMsiUpdatedVersion.GetString())
                     .Set("downloaded_bytes", static_cast<int>(crx_file_size))
                     .Set("total_bytes", static_cast<int>(crx_file_size))
                     .Set("install_progress", -1)
                     .Set("error_category",
                          should_install_successfully
                              ? 0
                              : static_cast<int>(
                                    UpdateService::ErrorCategory::kInstall))
                     .Set("error_code", GetParam().error_code)
                     .Set("extra_code1", 0)
                     .Set("installer_text", GetParam().installer_text)
                     .Set("installer_cmd_line", GetParam().installer_cmd_line))
            .Set("expected_result", 0)));
  }

  if (should_install_successfully) {
    ExpectAppInstalled(kMsiAppId, kMsiUpdatedVersion);
    if (!GetParam().installer_cmd_line.empty()) {
      const std::wstring post_install_launch_command_line =
          base::ASCIIToWide(GetParam().installer_cmd_line);
      EXPECT_EQ(test::IsProcessRunning(post_install_launch_command_line),
                GetParam().interactive_install);
      EXPECT_TRUE(test::KillProcesses(post_install_launch_command_line, 0));
    }
    ASSERT_NO_FATAL_FAILURE(Uninstall());
  } else {
    ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kMsiAppId));

    // Wait for the updater to uninstall itself automatically since the app
    // failed to install, and there are now no apps to manage.
    ASSERT_TRUE(WaitForUpdaterExit());
  }
}

#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace updater::test
