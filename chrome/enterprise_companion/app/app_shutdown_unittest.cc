// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
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

namespace enterprise_companion {
namespace {

constexpr char kServerNameFlag[] = "server-name";
constexpr int32_t kIpcCallerNotAllowedExitCode = 42;

class MockEnterpriseCompanionService final : public EnterpriseCompanionService {
 public:
  MockEnterpriseCompanionService() = default;
  ~MockEnterpriseCompanionService() override = default;

  MOCK_METHOD(void, Shutdown, (base::OnceClosure callback), (override));
  MOCK_METHOD(void, FetchPolicies, (StatusCallback callback), (override));
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
  base::Process SpawnClient() {
    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    command_line.AppendSwitchNative(kServerNameFlag, server_name_);
    return base::SpawnMultiProcessTestChild("ShutdownAppClient", command_line,
                                            /*options=*/{});
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
        .AppendASCII(base::StrCat({"ChromeEnterpriseCompanionTest",
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
  EXPECT_EQ(WaitForProcess(child_process), kIpcCallerNotAllowedExitCode);
}

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
  return !result.ok();
}

}  // namespace enterprise_companion
