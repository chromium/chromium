// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

namespace {

#if BUILDFLAG(IS_POSIX)
constexpr char kTestExe[] = "enterprise_companion_test";
#elif BUILDFLAG(IS_WIN)
constexpr char kTestExe[] = "enterprise_companion_test.exe";
#endif

}  // namespace

class IntegrationTests : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(base::PathExists(test_exe_path_));
    GetTestMethods().Clean();
    GetTestMethods().ExpectClean();
    InstallConstantsOverrides();
  }

  void TearDown() override {
    if (server_process_.IsValid()) {
      ShutdownServerAndWaitForExit();
    }
    GetTestMethods().Clean();
    GetTestMethods().ExpectClean();
  }

 protected:
  // Install the app under test.
  bool Install() {
#if BUILDFLAG(IS_MAC)
    InstallFakeKSAdmin(/*should_succeed=*/true);
#endif
    base::CommandLine command_line(test_exe_path_);
    command_line.AppendSwitch(kInstallSwitch);
    base::Process installer_process = base::LaunchProcess(command_line, {});
    return WaitForProcess(installer_process) == 0;
  }

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

  // Sends a shutdown request to the server and waits for it to exit.
  void ShutdownServerAndWaitForExit() {
    EXPECT_TRUE(CreateAppShutdown()->Run().ok());
    EXPECT_EQ(WaitForProcess(server_process_), 0);
  }

  // Configures the overrides JSON file to inject test values into the app
  // under test.
  void InstallConstantsOverrides() {
    base::Value::Dict overrides;

#if BUILDFLAG(IS_WIN)
    // Allow access from builtin administrators.
    overrides.Set(kNamedPipeSecurityDescriptorKey, "D:(A;;GA;;;BA)");
#endif

    std::optional<base::FilePath> overrides_json_path = GetOverridesFilePath();
    ASSERT_TRUE(overrides_json_path);
    ASSERT_TRUE(base::CreateDirectory(overrides_json_path->DirName()));
    ASSERT_TRUE(
        JSONFileValueSerializer(*overrides_json_path).Serialize(overrides));
  }

  base::test::TaskEnvironment environment_;
  base::Process server_process_;

 private:
  const base::FilePath test_exe_path_ =
      base::PathService::CheckedGet(base::DIR_EXE).AppendASCII(kTestExe);
  ScopedIPCSupportWrapper ipc_support_;
};

// Running the application installer should configure a valid installation.
TEST_F(IntegrationTests, Install) {
  ASSERT_TRUE(Install());

  GetTestMethods().ExpectInstalled();
}

// Attempting to shut down the server when it's not running should fail.
TEST_F(IntegrationTests, ShutdownWithoutServerFails) {
  EXPECT_TRUE(CreateAppShutdown()->Run().EqualsApplicationError(
      ApplicationError::kMojoConnectionFailed));
}

// The server should shut down upon request.
TEST_F(IntegrationTests, Shutdown) {
  ASSERT_TRUE(Install());
  LaunchApp();
  WaitForServerStart();

  ShutdownServerAndWaitForExit();
}

}  // namespace enterprise_companion
