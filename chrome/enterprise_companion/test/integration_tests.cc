// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "chrome/enterprise_companion/test/test_server.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace enterprise_companion {

namespace {

constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kFakeMachineLevelOmahaPolicyValue[] =
    "machine-level-omaha payload";
constexpr char kFakeMachineLevelUserPolicyValue[] =
    "machine-level-user payload";
constexpr char kFakeMachineLevelExtensionPolicyValue[] =
    "machine-level-extension payload";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
std::string ToProxyURL(const GURL& url) {
  return base::StrCat({url.host(), ":", url.port()});
}
#endif

}  // namespace

class IntegrationTests : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(dm_test_server_.Start());
    ASSERT_NO_FATAL_FAILURE(GetTestMethods().Clean());
    ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectClean());
    ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides());

    scoped_refptr<device_management_storage::DMStorage> dm_storage =
        device_management_storage::GetDefaultDMStorage();
    ASSERT_TRUE(dm_storage);
    device_id_ = dm_storage->GetDeviceID();
    policy_cache_root_ = dm_storage->policy_cache_folder();
  }

  void TearDown() override {
    WaitForTestServerExpectationsToBeMet();
    if (server_process_.IsValid()) {
      ShutdownServerAndWaitForExit();
    }
    ASSERT_NO_FATAL_FAILURE(CopyApplicationArtifacts());
    ASSERT_NO_FATAL_FAILURE(GetTestMethods().Clean());
    ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectClean());
  }

 protected:
  // Launches the installed app.
  void LaunchApp() {
    std::optional<base::FilePath> install_dir = GetInstallDirectory();
    ASSERT_TRUE(install_dir);
    base::CommandLine command_line(install_dir->AppendASCII(kExecutableName));
    // This will change the verification key to be used by the
    // CloudPolicyValidator. It will allow for the policy data provided by tests
    // to pass signature validation.
    command_line.AppendSwitchASCII(
        policy::switches::kPolicyVerificationKey,
        policy::PolicyBuilder::GetEncodedPolicyVerificationKey());
    server_process_ = base::LaunchProcess(command_line, {});
    ASSERT_TRUE(server_process_.IsValid());
  }

  // Waits for the app to begin accepting Mojo connections.
  void WaitForServerStart() {
    ASSERT_TRUE(WaitFor(
        [] {
          return named_mojo_ipc_server::ConnectToServer(GetServerName())
              .is_valid();
        },
        [] { VLOG(1) << "Waiting for the app to accept connections..."; }));
  }

  // Waits for the test server to not have any unmet expectations. This is
  // useful to ensure that event logs are transmitted before the server is
  // shut down.
  void WaitForTestServerExpectationsToBeMet() {
    EXPECT_TRUE(WaitFor(
        [&] { return !test_server_.HasUnmetExpectations(); },
        [] {
          VLOG(1) << "Waiting for test server expectations to be met...";
        }));
  }

  // Sends a shutdown request to the server and waits for it to exit.
  void ShutdownServerAndWaitForExit() {
    EXPECT_TRUE(CreateAppShutdown()->Run().ok());
    EXPECT_EQ(WaitForProcess(server_process_), 0);
  }

  base::Value::Dict GetDefaultConstantsOverrides() {
    base::Value::Dict overrides;

#if BUILDFLAG(IS_WIN)
    // Allow access from builtin administrators.
    overrides.Set(kNamedPipeSecurityDescriptorKey, "D:(A;;GA;;;BA)");
#endif
    overrides.Set(kCrashUploadUrlKey, test_server_.crash_upload_url().spec());
    overrides.Set(
        kDMEncryptedReportingUrlKey,
        test_server_.device_management_encrypted_reporting_url().spec());
    overrides.Set(
        kDMRealtimeReportingUrlKey,
        test_server_.device_management_realtime_reporting_url().spec());
    overrides.Set(kDMServerUrlKey, dm_test_server_.GetServiceURL().spec());
    overrides.Set(kEventLoggingUrlKey, test_server_.event_logging_url().spec());
    overrides.Set(kEventLoggerMinTimeoutSecKey, 0);
    return overrides;
  }

  // Configures the overrides JSON file to inject test values into the app
  // under test.
  void InstallConstantsOverrides() {
    InstallConstantsOverrides(GetDefaultConstantsOverrides());
  }
  void InstallConstantsOverrides(const base::Value::Dict& overrides) {
    std::optional<base::FilePath> overrides_json_path = GetOverridesFilePath();
    ASSERT_TRUE(overrides_json_path);
    ASSERT_TRUE(base::CreateDirectory(overrides_json_path->DirName()));
    ASSERT_TRUE(
        JSONFileValueSerializer(*overrides_json_path).Serialize(overrides));
  }

  void StoreEnrollmentToken(const std::string& enrollment_token) {
    scoped_refptr<device_management_storage::DMStorage> dm_storage =
        device_management_storage::GetDefaultDMStorage();
    ASSERT_TRUE(dm_storage);
    dm_storage->StoreEnrollmentToken(enrollment_token);
  }

  void StoreDMToken(const std::string& dm_token) {
    scoped_refptr<device_management_storage::DMStorage> dm_storage =
        device_management_storage::GetDefaultDMStorage();
    ASSERT_TRUE(dm_storage);
    dm_storage->StoreDmToken(dm_token);
  }

  // Asserts that the contents of the policies persisted to disk match
  // expectations. `policy_value_map` associates policy types to policy value
  // payloads.
  void ExpectPersistedPolicyValues(
      const base::flat_map<std::string, std::string>& policy_value_map) {
    bool has_cached_policy_info = false;
    base::FileEnumerator(policy_cache_root_, false,
                         base::FileEnumerator::NAMES_ONLY)
        .ForEach([&](const base::FilePath& name) {
#if BUILDFLAG(IS_WIN)
          std::string file_name = base::WideToUTF8(name.BaseName().value());
#else
          std::string file_name = name.BaseName().value();
#endif
          if (file_name == "CachedPolicyInfo") {
            has_cached_policy_info = true;
            return;
          }

          std::string policy_type;
          ASSERT_TRUE(base::Base64Decode(file_name, &policy_type))
              << "Unexpected file name in policy cache: " << file_name;
          ASSERT_TRUE(policy_value_map.contains(policy_type))
              << "Unexpected persisted policy type: " << policy_type;
          ASSERT_TRUE(base::DirectoryExists(name))
              << "Cached policy type is not a directory";

          base::FilePath cached_response_path =
              name.AppendASCII("PolicyFetchResponse");
          ASSERT_TRUE(base::PathExists(cached_response_path));
          std::string cached_response_contents;
          ASSERT_TRUE(base::ReadFileToString(cached_response_path,
                                             &cached_response_contents));
          enterprise_management::PolicyFetchResponse cached_response;
          ASSERT_TRUE(
              cached_response.ParseFromString(cached_response_contents));
          enterprise_management::PolicyData policy_data;
          ASSERT_TRUE(
              policy_data.ParseFromString(cached_response.policy_data()));

          EXPECT_EQ(policy_data.policy_type(), policy_type);
          EXPECT_EQ(policy_data.policy_value(),
                    policy_value_map.at(policy_type));
        });
    EXPECT_TRUE(has_cached_policy_info);
  }

  // Configure the server to send the default policy values for
  // "google/machine-level-omaha", "google/chrome/machine-level-user", and
  // "google/chrome/machine-level-extension".
  void SetDefaultPolicyFetchResponses() {
    dm_test_server_.policy_storage()->SetPolicyPayload(
        policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
        kFakeMachineLevelOmahaPolicyValue);
    dm_test_server_.policy_storage()->SetPolicyPayload(
        policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
        kFakeMachineLevelUserPolicyValue);
    dm_test_server_.policy_storage()->SetPolicyPayload(
        policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
        "extension-1", kFakeMachineLevelExtensionPolicyValue);
  }

  // Expects that the policy values configured via
  // `SetDefaultPolicyFetchResponses` have been persisted to disk.
  void ExpectDefaultPolicyValuesPersisted() {
    ASSERT_NO_FATAL_FAILURE(ExpectPersistedPolicyValues({
        {policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
         kFakeMachineLevelOmahaPolicyValue},
        {policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
         kFakeMachineLevelUserPolicyValue},
        {policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
         kFakeMachineLevelExtensionPolicyValue},
    }));
  }

  void RegisterClientWithDMServer() {
    policy::ClientStorage::ClientInfo client_info;
    client_info.device_id = device_id_;
    client_info.device_token = policy::kFakeDeviceToken;
    client_info.allowed_policy_types = {
        policy::dm_protocol::kGoogleUpdateMachineLevelAppsPolicyType,
        policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
        policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
        policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType};
    dm_test_server_.client_storage()->RegisterClient(client_info);
  }

  base::test::TaskEnvironment environment_;
  base::Process server_process_;
  TestServer test_server_;
  policy::EmbeddedPolicyTestServer dm_test_server_;
  std::string device_id_;
  base::FilePath policy_cache_root_;

 private:
  // Copies artifacts from the installed application (e.g. logs, crash dumps,
  // etc.) to ISOLATED_OUTDIR, if present.
  void CopyApplicationArtifacts() {
    std::string isolated_outdir_str;
    if (!base::Environment::Create()->GetVar("ISOLATED_OUTDIR",
                                             &isolated_outdir_str)) {
      return;
    }

    std::optional<base::FilePath> install_dir = GetInstallDirectory();
    ASSERT_TRUE(install_dir);
    base::FilePath artifacts_dir =
        base::FilePath::FromASCII(isolated_outdir_str)
            .AppendASCII(base::StrCat(
                {testing::UnitTest::GetInstance()->current_test_suite()->name(),
                 ".",
                 testing::UnitTest::GetInstance()
                     ->current_test_info()
                     ->name()}));

    ASSERT_NO_FATAL_FAILURE(
        CopyApplicationArtifacts(*install_dir, artifacts_dir));

#if BUILDFLAG(IS_WIN)
    std::optional<base::FilePath> alt_install_dir =
        GetInstallDirectoryForAlternateArch();
    if (alt_install_dir) {
      ASSERT_NO_FATAL_FAILURE(CopyApplicationArtifacts(
          *alt_install_dir, artifacts_dir.AppendASCII("alt_arch")));
    }
#endif
  }

  void CopyApplicationArtifacts(const base::FilePath& install_dir,
                                const base::FilePath& artifacts_dir) {
    ASSERT_TRUE(base::CreateDirectory(artifacts_dir));
    base::FilePath log_path =
        install_dir.AppendASCII("enterprise_companion.log");
    if (base::PathExists(log_path)) {
      ASSERT_TRUE(
          base::CopyFile(log_path, artifacts_dir.Append(log_path.BaseName())));
    }

    base::FilePath crash_db_path = install_dir.AppendASCII("Crashpad");
    if (base::PathExists(crash_db_path)) {
      ASSERT_TRUE(base::CopyDirectory(
          crash_db_path, artifacts_dir.AppendASCII("Crashpad"), true));
    }
  }

  ScopedIPCSupportWrapper ipc_support_;
};

// Running the application installer should configure a valid installation.
TEST_F(IntegrationTests, Install) {
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectInstalled());
}

// Running the application uninstaller should remove all traces of the app from
// the system.
TEST_F(IntegrationTests, Uninstall) {
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  ASSERT_TRUE(install_dir);
  base::CommandLine command_line(install_dir->AppendASCII(kExecutableName));
  command_line.AppendSwitch(kUninstallSwitch);
  base::Process uninstall_process = base::LaunchProcess(command_line, {});
  ASSERT_TRUE(uninstall_process.IsValid());
  EXPECT_EQ(WaitForProcess(uninstall_process), 0);

  // The server process should be shut down by the uninstall process. Reset the
  // handle in the test fixture to ensure that a second shutdown is not
  // attempted during `TearDown`.
  EXPECT_EQ(WaitForProcess(server_process_), 0);
  server_process_ = base::Process();

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectClean());
}

// Running the application's "install if needed" command should install the
// application if an enrollment token is present.
TEST_F(IntegrationTests, InstallIfNeeded_WithEnrollmentToken_Installs) {
  StoreEnrollmentToken(kFakeEnrollmentToken);

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().InstallIfNeeded());

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectInstalled());
}

// Running the application's "install if needed" command should install the
// application if a device management token is present.
TEST_F(IntegrationTests, InstallIfNeeded_WithDMToken_Installs) {
  StoreDMToken(policy::kFakeDeviceToken);

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().InstallIfNeeded());

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().ExpectInstalled());
}

// Running the application's "install if needed" command should not install the
// application if the device does not appear to be managed.
TEST_F(IntegrationTests, InstallIfNeeded_NotManaged_SkipsInstall) {
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().InstallIfNeeded());

  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  ASSERT_TRUE(install_dir);
  EXPECT_FALSE(base::PathExists(install_dir->AppendASCII(kExecutableName)));
}

// Running the application's "install if needed" command should not install the
// application if the application is already installed.
TEST_F(IntegrationTests, InstallIfNeeded_AlreadyInstalled_SkipsInstall) {
  std::optional<base::FilePath> install_dir = GetInstallDirectory();
  ASSERT_TRUE(install_dir);
  ASSERT_TRUE(base::CreateDirectory(*install_dir));
  ASSERT_TRUE(
      base::WriteFile(install_dir->AppendASCII(kExecutableName), "fake_exe"));

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().InstallIfNeeded());

  std::string exe_contents;
  ASSERT_TRUE(base::ReadFileToStringWithMaxSize(
      install_dir->AppendASCII(kExecutableName), &exe_contents, 64));
  EXPECT_EQ(exe_contents, "fake_exe");
}

#if BUILDFLAG(IS_WIN)
// Running the application's "install if needed" command should not install the
// application if the application is already installed for a different
// architecture.
TEST_F(IntegrationTests, InstallIfNeeded_AlreadyInstalledAltArch_SkipsInstall) {
  std::optional<base::FilePath> install_dir =
      GetInstallDirectoryForAlternateArch();
  if (!install_dir) {
    GTEST_SKIP() << "Not implemented for x86 hosts.";
  }
  ASSERT_TRUE(base::CreateDirectory(*install_dir));
  ASSERT_TRUE(
      base::WriteFile(install_dir->AppendASCII(kExecutableName), "fake_exe"));

  ASSERT_NO_FATAL_FAILURE(GetTestMethods().InstallIfNeeded());

  std::string exe_contents;
  ASSERT_TRUE(base::ReadFileToStringWithMaxSize(
      install_dir->AppendASCII(kExecutableName), &exe_contents, 64));
  EXPECT_EQ(exe_contents, "fake_exe");
}
#endif

// Attempting to shut down the server when it's not running should fail.
TEST_F(IntegrationTests, ShutdownWithoutServerFails) {
  EXPECT_TRUE(CreateAppShutdown()->Run().EqualsApplicationError(
      ApplicationError::kMojoConnectionFailed));
}

// The server should shut down upon request.
TEST_F(IntegrationTests, Shutdown) {
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  ShutdownServerAndWaitForExit();
}

// The server should fail to fetch policies if no enrollment token is present
// and the device is not registered.
TEST_F(IntegrationTests, FetchPoliciesWithoutRegistrationFails) {
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus(
                ApplicationError::kRegistrationPreconditionFailed)}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().EqualsApplicationError(
      ApplicationError::kRegistrationPreconditionFailed));
}

// The application should register the device and fetch policies upon request.
TEST_F(IntegrationTests, FetchPoliciesAndRegister) {
  SetDefaultPolicyFetchResponses();
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());

  ASSERT_NO_FATAL_FAILURE(ExpectDefaultPolicyValuesPersisted());
}

// The application should fetch policies upon request without re-registering
// if the device is already managed.
TEST_F(IntegrationTests, FetchPoliciesAlreadyRegistered) {
  SetDefaultPolicyFetchResponses();
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(StoreDMToken(policy::kFakeDeviceToken));
  RegisterClientWithDMServer();
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_, {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
                          EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());

  ASSERT_NO_FATAL_FAILURE(ExpectDefaultPolicyValuesPersisted());
}

// The application should invalidate the stored DM token if the server
// indicates that the device is unknown.
TEST_F(IntegrationTests, UnknownDMTokenInvalidated) {
  SetDefaultPolicyFetchResponses();
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(StoreDMToken(policy::kFakeDeviceToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_, {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
                          EnterpriseCompanionStatus::FromDeviceManagementStatus(
                              policy::DeviceManagementStatus::
                                  DM_STATUS_SERVICE_DEVICE_NOT_FOUND)}})},
      CreateLogResponse());
  EXPECT_TRUE(CreateAppFetchPolicies()->Run().EqualsDeviceManagementStatus(
      policy::DeviceManagementStatus::DM_STATUS_SERVICE_DEVICE_NOT_FOUND));

  // Shut down the server before reading the token back, as the server may
  // hold an exclusive lock on files opened by DMStorage.
  WaitForTestServerExpectationsToBeMet();
  ShutdownServerAndWaitForExit();

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_TRUE(dm_storage);
  EXPECT_FALSE(dm_storage->IsValidDMToken());
}

// The application should reload the enrollment token from storage on every
// registration attempt.
TEST_F(IntegrationTests, ReloadsTokens) {
  SetDefaultPolicyFetchResponses();
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  // Attempt a registration with the invalid enrollment token, it should fail.
  ASSERT_NO_FATAL_FAILURE(
      StoreEnrollmentToken(policy::kInvalidEnrollmentToken));
  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::FromDeviceManagementStatus(
                policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID)}})},
      CreateLogResponse());
  EXPECT_TRUE(CreateAppFetchPolicies()->Run().EqualsDeviceManagementStatus(
      policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID));

  // Change the enrollment token externally and attempt enrollment again, it
  // should succeed.
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());
  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());

  ASSERT_NO_FATAL_FAILURE(ExpectDefaultPolicyValuesPersisted());
}

// Tests relating to proxy configurations. The application does not support
// proxies on Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
// The application should tunnel network requests through the proxy server
// configured by Cloud Policy.
TEST_F(IntegrationTests, CloudPolicyProxy_FixedServer) {
  SetDefaultPolicyFetchResponses();

  wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;
  omaha_settings.set_proxy_mode("fixed_servers");
  omaha_settings.set_proxy_server(ToProxyURL(dm_test_server_.GetServiceURL()));
  dm_test_server_.policy_storage()->SetPolicyPayload(
      policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
      omaha_settings.SerializeAsString());

  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());
  WaitForTestServerExpectationsToBeMet();
  EXPECT_TRUE(CreateAppShutdown()->Run().ok());
  EXPECT_EQ(WaitForProcess(server_process_), 0);

  base::Value::Dict overrides = GetDefaultConstantsOverrides();
  overrides.Set(kDMServerUrlKey, "http://dm.server.not_exist/dmapi");
  ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides(overrides));

  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_, {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
                          EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());
}

// The application should tunnel network requests through the proxy server
// configured by Cloud Policy without having to restart.
TEST_F(IntegrationTests, CloudPolicyProxy_SettingsChangeAppliedAtRuntime) {
  SetDefaultPolicyFetchResponses();

  // Start the server with no proxy configuration, all hosts are reachable on
  // the local network.
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  // Configure cloud policies which configure the proxy. All hosts are still
  // reachable on the local network so these policies won't cause traffic to be
  // routed through the proxy.
  wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;
  omaha_settings.set_proxy_mode("fixed_servers");
  omaha_settings.set_proxy_server(ToProxyURL(dm_test_server_.GetServiceURL()));
  dm_test_server_.policy_storage()->SetPolicyPayload(
      policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
      omaha_settings.SerializeAsString());
  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());
  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());
  WaitForTestServerExpectationsToBeMet();

  // Restart the server and override the DM server URL. Subsequent requests to
  // this service should be routed through the proxy.
  EXPECT_TRUE(CreateAppShutdown()->Run().ok());
  EXPECT_EQ(WaitForProcess(server_process_), 0);
  base::Value::Dict overrides = GetDefaultConstantsOverrides();
  overrides.Set(kDMServerUrlKey, "http://dm.server.not_exist/dmapi");
  ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides(overrides));
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  // Configure the cloud policies to set the proxy mode to direct and trigger a
  // fetch. After the settings are applied, the DM server URL should be
  // unreachable.
  omaha_settings.Clear();
  omaha_settings.set_proxy_mode("direct");
  dm_test_server_.policy_storage()->SetPolicyPayload(
      policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
      omaha_settings.SerializeAsString());
  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_, {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
                          EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());
  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());
  WaitForTestServerExpectationsToBeMet();

  // Expect subsequent policy fetch requests to fail, as the DM server is
  // unreachable without a proxy.
  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::FromDeviceManagementStatus(
                policy::DeviceManagementStatus::DM_STATUS_REQUEST_FAILED)}})},
      CreateLogResponse());
  EXPECT_FALSE(CreateAppFetchPolicies()->Run().ok());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

// Tests relating to Windows-specific proxy settings.
#if BUILDFLAG(IS_WIN)

// The application should tunnel network requests through the proxy server
// specified by a PAC script pointed to by Cloud Policy. This test is only
// enabled on Windows because there is no system PAC implementation on Linux,
// and the application does not support proxies on Mac.
TEST_F(IntegrationTests, CloudPolicyProxy_PacScript) {
  SetDefaultPolicyFetchResponses();

  wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
      omaha_settings;
  omaha_settings.set_proxy_mode("pac_script");
  omaha_settings.set_proxy_pac_url(test_server_.proxy_pac_url().spec());
  dm_test_server_.policy_storage()->SetPolicyPayload(
      policy::dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
      omaha_settings.SerializeAsString());

  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());
  WaitForTestServerExpectationsToBeMet();
  EXPECT_TRUE(CreateAppShutdown()->Run().ok());
  EXPECT_EQ(WaitForProcess(server_process_), 0);

  base::Value::Dict overrides = GetDefaultConstantsOverrides();
  overrides.Set(kDMServerUrlKey, "http://dm.server.not_exist/dmapi");
  ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides(overrides));

  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreatePacUrlMatcher(test_server_)},
      base::StringPrintf(
          "function FindProxyForURL(url, host) { return \"PROXY %s\"; }",
          ToProxyURL(dm_test_server_.GetServiceURL())));
  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_, {{proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
                          EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());
}

// The application should tunnel network requests through the proxy server
// configured by Group Policy.
TEST_F(IntegrationTests, GroupPolicyProxy_ProxyServer) {
  base::Value::Dict overrides = GetDefaultConstantsOverrides();
  overrides.Set(kDMServerUrlKey, "http://dm.server.not_exist/dmapi");
  ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides(overrides));
  ASSERT_NO_FATAL_FAILURE(SetLocalProxyPolicies(
      /*proxy_mode=*/"fixed_servers",
      /*pac_url=*/std::nullopt, ToProxyURL(dm_test_server_.GetServiceURL()),
      /*cloud_policy_overrides_platform_policy=*/std::nullopt));

  SetDefaultPolicyFetchResponses();
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());

  ASSERT_NO_FATAL_FAILURE(ExpectDefaultPolicyValuesPersisted());
}

// The application should tunnel network requests through the proxy server
// configured by the PAC script specified by Group Policy.
TEST_F(IntegrationTests, GroupPolicyProxy_PacScript) {
  base::Value::Dict overrides = GetDefaultConstantsOverrides();
  overrides.Set(kDMServerUrlKey, "http://dm.server.not_exist/dmapi");
  ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides(overrides));
  ASSERT_NO_FATAL_FAILURE(SetLocalProxyPolicies(
      /*proxy_mode=*/"pac_script", test_server_.proxy_pac_url().spec(),
      /*proxy_server=*/std::nullopt,
      /*cloud_policy_overrides_platform_policy=*/std::nullopt));
  test_server_.ExpectOnce(
      {CreatePacUrlMatcher(test_server_)},
      base::StringPrintf(
          "function FindProxyForURL(url, host) { return \"PROXY %s\"; }",
          ToProxyURL(dm_test_server_.GetServiceURL())));

  SetDefaultPolicyFetchResponses();
  ASSERT_NO_FATAL_FAILURE(StoreEnrollmentToken(kFakeEnrollmentToken));
  ASSERT_NO_FATAL_FAILURE(GetTestMethods().Install());
  ASSERT_NO_FATAL_FAILURE(LaunchApp());
  ASSERT_NO_FATAL_FAILURE(WaitForServerStart());

  test_server_.ExpectOnce(
      {CreateEventLogMatcher(
          test_server_,
          {{proto::EnterpriseCompanionEvent::kBrowserEnrollmentEvent,
            EnterpriseCompanionStatus::Success()},
           {proto::EnterpriseCompanionEvent::kPolicyFetchEvent,
            EnterpriseCompanionStatus::Success()}})},
      CreateLogResponse());

  EXPECT_TRUE(CreateAppFetchPolicies()->Run().ok());

  ASSERT_NO_FATAL_FAILURE(ExpectDefaultPolicyValuesPersisted());
}

// The application should exit with a failure if proxy navigation fails and the
// server is not directly reachable.
TEST_F(IntegrationTests, GroupPolicyProxy_BadProxyServer) {
  base::Value::Dict overrides = GetDefaultConstantsOverrides();
  overrides.Set(kDMServerUrlKey, "http://dm.server.not_exist/dmapi");
  ASSERT_NO_FATAL_FAILURE(InstallConstantsOverrides(overrides));
  ASSERT_NO_FATAL_FAILURE(SetLocalProxyPolicies(
      /*proxy_mode=*/"fixed_servers",
      /*pac_url=*/std::nullopt, "http://proxy.server.not_exist",
      /*cloud_policy_overrides_platform_policy=*/std::nullopt));
  EXPECT_FALSE(CreateAppFetchPolicies()->Run().ok());
}

#endif  // BUILDFLAG(IS_WIN)
}  // namespace enterprise_companion
