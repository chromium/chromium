// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/enterprise_companion_service.h"
#include "chrome/enterprise_companion/enterprise_companion_service_stub.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/enterprise_companion/test/test_utils.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <optional>

#include "base/win/access_token.h"
#include "chrome/enterprise_companion/ipc_security.h"
#endif

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace enterprise_companion {
namespace {

constexpr char kServerNameFlag[] = "server-name";
constexpr int32_t kIpcCallerNotAllowedExitCode = 42;
constexpr int32_t kMojoConnectionFailedExitCode = 43;

class MockEnterpriseCompanionService final : public EnterpriseCompanionService {
 public:
  MOCK_METHOD(void, Shutdown, (base::OnceClosure callback), (override));
  MOCK_METHOD(void,
              FetchPolicies,
              (policy::PolicyFetchReason reason, StatusCallback callback),
              (override));
};

}  // namespace

class AppShutdownTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_service_ = std::make_unique<MockEnterpriseCompanionService>();
  }

  void TearDown() override {
    // NamedMojoIpcServer requires test environments to run until idle to avoid
    // a memory leak.
    environment_.RunUntilIdle();
  }

 protected:
  base::Process SpawnClient(const base::LaunchOptions& options = {}) {
    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    command_line.AppendSwitchNative(kServerNameFlag, server_name_);
    return base::SpawnMultiProcessTestChild("ShutdownAppClient", command_line,
                                            options);
  }

  mojo::NamedPlatformChannel::ServerName server_name_ = GetTestServerName();
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support_;
  std::unique_ptr<MockEnterpriseCompanionService> mock_service_;

 private:
  static mojo::NamedPlatformChannel::ServerName GetTestServerName() {
#if BUILDFLAG(IS_MAC)
    return base::StrCat({"org.chromium.ChromeEnterpriseCompanionTest",
                         base::UnguessableToken::Create().ToString(),
                         ".service"});
#elif BUILDFLAG(IS_LINUX)
    return base::GetTempDirForTesting()
        .AppendUTF8(base::StrCat({"ChromeEnterpriseCompanionTest",
                                  base::UnguessableToken::Create().ToString(),
                                  ".service.sk"}))
        .AsUTF8Unsafe();
#elif BUILDFLAG(IS_WIN)
    return base::UTF8ToWide(
        base::StrCat({"org.chromium.ChromeEnterpriseCompanionTest",
                      base::UnguessableToken::Create().ToString()}));
#endif
  }
};

TEST_F(AppShutdownTest, ServiceReachable) {
  EXPECT_CALL(*mock_service_, Shutdown)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });
  // Start the companion service and wait for it to become available before
  // launching the child process.
  base::RunLoop start_run_loop;
  std::unique_ptr<mojom::EnterpriseCompanion> stub =
      CreateEnterpriseCompanionServiceStub(
          std::move(mock_service_),
          {server_name_,
           named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection},
          base::BindRepeating([](const named_mojo_ipc_server::ConnectionInfo&) {
            return true;
          }),
          start_run_loop.QuitClosure());
  start_run_loop.Run(FROM_HERE);

  base::Process child_process = SpawnClient();
  EXPECT_EQ(WaitForProcess(child_process), 0);
}

TEST_F(AppShutdownTest, UntrustedCallerRejected) {
  EXPECT_CALL(*mock_service_, Shutdown).Times(0);
  base::RunLoop start_run_loop;
  std::unique_ptr<mojom::EnterpriseCompanion> stub =
      CreateEnterpriseCompanionServiceStub(
          std::move(mock_service_),
          {server_name_,
           named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection},
          base::BindRepeating([](const named_mojo_ipc_server::ConnectionInfo&) {
            return false;
          }),
          start_run_loop.QuitClosure());
  start_run_loop.Run(FROM_HERE);

  base::Process child_process = SpawnClient();
#if BUILDFLAG(IS_WIN)
  // On Windows, untrusted connections are rejected when setting up the Mojo
  // connection. On Linux and macOS untrusted connections are rejected per-RPC.
  EXPECT_EQ(WaitForProcess(child_process), kMojoConnectionFailedExitCode);
#else
  EXPECT_EQ(WaitForProcess(child_process), kIpcCallerNotAllowedExitCode);
#endif
}

#if BUILDFLAG(IS_WIN)
// Simulates a less privileged user squatting the server's pipe name with a
// permissive DACL: a Medium integrity caller can reach the server, but must be
// routed to the untrusted stub by the production trust decider. Completing the
// RPC also demonstrates that such a caller can connect even though the
// connection's Mojo invitation declares it untrusted.
TEST_F(AppShutdownTest, MediumIntegrityCallerRejected) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess(/*impersonation=*/false,
                                                 TOKEN_DUPLICATE);
  ASSERT_TRUE(token);
  if (!IsTrustedIntegrityLevel(token->IntegrityLevel())) {
    GTEST_SKIP() << "The test process must be at High integrity or above.";
  }
  std::optional<base::win::AccessToken> medium_token = token->DuplicatePrimary(
      TOKEN_ADJUST_DEFAULT | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE);
  ASSERT_TRUE(medium_token);
  ASSERT_TRUE(medium_token->SetIntegrityLevel(SECURITY_MANDATORY_MEDIUM_RID));

  EXPECT_CALL(*mock_service_, Shutdown).Times(0);
  named_mojo_ipc_server::EndpointOptions options{
      server_name_,
      named_mojo_ipc_server::EndpointOptions::kUseIsolatedConnection};
  // Grants access to everyone, as an attacker squatting the pipe name might.
  options.security_descriptor = L"D:(A;;GA;;;WD)";
  base::RunLoop start_run_loop;
  std::unique_ptr<mojom::EnterpriseCompanion> stub =
      CreateEnterpriseCompanionServiceStub(std::move(mock_service_), options,
                                           CreateIpcTrustDecider(),
                                           start_run_loop.QuitClosure());
  start_run_loop.Run(FROM_HERE);

  base::LaunchOptions launch_options;
  launch_options.using_token = medium_token->get();
  base::Process child_process = SpawnClient(launch_options);
  ASSERT_TRUE(child_process.IsValid());
  EXPECT_EQ(WaitForProcess(child_process), kIpcCallerNotAllowedExitCode);
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(AppShutdownTest, Timeout) {
  EnterpriseCompanionStatus result = CreateAppShutdown(server_name_)->Run();
  EXPECT_TRUE(
      result.EqualsApplicationError(ApplicationError::kMojoConnectionFailed));
}

MULTIPROCESS_TEST_MAIN(ShutdownAppClient) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support;

  EnterpriseCompanionStatus result =
      CreateAppShutdown(command_line->GetSwitchValueNative(kServerNameFlag))
          ->Run();
  if (result.EqualsApplicationError(ApplicationError::kIpcCallerNotAllowed)) {
    return kIpcCallerNotAllowedExitCode;
  }
  if (result.EqualsApplicationError(ApplicationError::kMojoConnectionFailed)) {
    return kMojoConnectionFailedExitCode;
  }
  return !result.ok();
}

}  // namespace enterprise_companion
