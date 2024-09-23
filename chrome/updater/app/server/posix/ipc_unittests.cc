// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"
#include "chrome/updater/app/server/posix/update_service_stub.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace updater {

class UpdaterIPCTestCase : public testing::Test {
 public:
  static constexpr char kClientProcessName[] = "UpdateServiceClient";

  void SetUp() override {
    wait_for_process_exit_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
  }

  static std::vector<UpdateService::AppState> GetExampleAppStates() {
    UpdateService::AppState ex1;
    ex1.app_id = "ex1";
    ex1.version = base::Version("9.19.20");
    ex1.ap = "foo";
    ex1.brand_code = "FooBarInc";
    ex1.brand_path = base::FilePath("/path/to/foo_bar");
    ex1.ecp = base::FilePath("path/to/foo_ecp");

    UpdateService::AppState ex2;
    ex2.app_id = "ex2";
    ex2.version = base::Version("98.4.5");
    ex2.ap = "zaz";
    ex2.brand_code = "BazInc";
    ex2.brand_path = base::FilePath("/path/to/baz");
    ex2.ecp = base::FilePath("path/to/baz_ecp");

    return {ex1, ex2};
  }

  static std::vector<UpdateService::UpdateState> GetExampleUpdateStates() {
    UpdateService::UpdateState state1;
    state1.app_id = "ex1";
    state1.state = UpdateService::UpdateState::State::kCheckingForUpdates;

    UpdateService::UpdateState state2;
    state2.app_id = "ex2";
    state2.state = UpdateService::UpdateState::State::kDownloading;
    state2.next_version = base::Version("3.14");
    state2.downloaded_bytes = 1024;
    state2.total_bytes = 2048;

    UpdateService::UpdateState state3;
    state3.app_id = "ex3";
    state3.state = UpdateService::UpdateState::State::kUpdateError;
    state3.install_progress = 99;
    state3.error_code = 0xDEAD;
    state3.extra_code1 = 0xBEEF;
    state3.installer_text = "Error: The beef has died.";
    state3.installer_cmd_line = "path/to/updater --crash-me";

    return {state1, state2, state3};
  }

  static void ExpectUpdateStatesEqual(const UpdateService::UpdateState& lhs,
                                      const UpdateService::UpdateState& rhs) {
    EXPECT_EQ(lhs.app_id, rhs.app_id);
    EXPECT_EQ(lhs.state, rhs.state);
    EXPECT_EQ(lhs.next_version.IsValid(), rhs.next_version.IsValid());
    if (lhs.next_version.IsValid() && rhs.next_version.IsValid()) {
      EXPECT_EQ(lhs.next_version, rhs.next_version);
    }
    EXPECT_EQ(lhs.downloaded_bytes, rhs.downloaded_bytes);
    EXPECT_EQ(lhs.total_bytes, rhs.total_bytes);
    EXPECT_EQ(lhs.install_progress, rhs.install_progress);
    EXPECT_EQ(lhs.error_category, rhs.error_category);
    EXPECT_EQ(lhs.error_code, rhs.error_code);
    EXPECT_EQ(lhs.extra_code1, rhs.extra_code1);
    EXPECT_EQ(lhs.installer_text, rhs.installer_text);
    EXPECT_EQ(lhs.installer_cmd_line, rhs.installer_cmd_line);
  }

  static base::RepeatingCallback<void(const UpdateService::UpdateState&)>
  ExpectUpdateStatesCallback() {
    std::vector<UpdateService::UpdateState> states = GetExampleUpdateStates();
    // For the convenience of using `back` and `pop_back` below.
    base::ranges::reverse(states);
    return base::BindRepeating(
        [](std::vector<UpdateService::UpdateState>& states,
           const UpdateService::UpdateState& state) {
          ASSERT_GT(states.size(), 0U);
          ExpectUpdateStatesEqual(state, states.back());
          states.pop_back();
        },
        base::OwnedRef(states));
  }

  static base::OnceCallback<void(UpdateService::Result)> ExpectResultCallback(
      base::RunLoop& run_loop) {
    return base::BindOnce([](UpdateService::Result result) {
             EXPECT_EQ(result, UpdateService::Result::kInstallFailed);
           })
        .Then(run_loop.QuitClosure());
  }

  static RegistrationRequest GetExampleRegistrationRequest() {
    RegistrationRequest r;
    r.app_id = "app_id";
    r.brand_code = "BRND";
    r.brand_path = base::FilePath("brand_path");
    r.ap = "ap";
    r.ap_path = base::FilePath("ap_path");
    r.ap_key = "ap_key";
    r.version = base::Version("1.2.3.4");
    r.version_path = base::FilePath("version_path");
    r.version_key = "version_key";
    r.existence_checker_path = base::FilePath("ecp");
    return r;
  }

  static void ExpectRegistrationRequestsEqual(const RegistrationRequest& a,
                                              const RegistrationRequest& b) {
    EXPECT_EQ(a.app_id, b.app_id);
    EXPECT_EQ(a.brand_code, b.brand_code);
    EXPECT_EQ(a.brand_path, b.brand_path);
    EXPECT_EQ(a.ap, b.ap);
    EXPECT_EQ(a.ap_path, b.ap_path);
    EXPECT_EQ(a.ap_key, b.ap_key);
    EXPECT_EQ(a.version, b.version);
    EXPECT_EQ(a.version_path, b.version_path);
    EXPECT_EQ(a.version_key, b.version_key);
    EXPECT_EQ(a.existence_checker_path, b.existence_checker_path);
  }

 protected:
  class MockUpdateService final : public UpdateService {
   public:
    MOCK_METHOD(void,
                GetVersion,
                (base::OnceCallback<void(const base::Version&)> callback),
                (override));
    MOCK_METHOD(void,
                FetchPolicies,
                (base::OnceCallback<void(int)> callback),
                (override));
    MOCK_METHOD(void,
                RegisterApp,
                (const RegistrationRequest& request,
                 base::OnceCallback<void(int)> callback),
                (override));
    MOCK_METHOD(
        void,
        GetAppStates,
        (base::OnceCallback<void(const std::vector<AppState>&)> callback),
        (override));
    MOCK_METHOD(void,
                RunPeriodicTasks,
                (base::OnceClosure callback),
                (override));
    MOCK_METHOD(void,
                CheckForUpdate,
                (const std::string& app_id,
                 Priority priority,
                 PolicySameVersionUpdate policy_same_version_update,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void,
                Update,
                (const std::string& app_id,
                 const std::string& install_data_index,
                 Priority priority,
                 PolicySameVersionUpdate policy_same_version_update,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void,
                UpdateAll,
                (base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void,
                Install,
                (const RegistrationRequest& registration,
                 const std::string& client_install_data,
                 const std::string& install_data_index,
                 Priority priority,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));
    MOCK_METHOD(void, CancelInstalls, (const std::string& app_id), (override));
    MOCK_METHOD(void,
                RunInstaller,
                (const std::string& app_id,
                 const base::FilePath& installer_path,
                 const std::string& install_args,
                 const std::string& install_data,
                 const std::string& install_settings,
                 base::RepeatingCallback<void(const UpdateState&)> state_update,
                 base::OnceCallback<void(Result)> callback),
                (override));

   protected:
    ~MockUpdateService() override = default;
  };

  int WaitForProcessExit(base::Process& process) {
    int exit_code = 0;
    bool process_exited = false;
    base::RunLoop wait_for_process_exit_loop;
    wait_for_process_exit_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&] {
          process_exited = base::WaitForMultiprocessTestChildExit(
              process, TestTimeouts::action_timeout(), &exit_code);
        }),
        wait_for_process_exit_loop.QuitClosure());
    wait_for_process_exit_loop.Run();
    process.Close();
    EXPECT_TRUE(process_exited);
    return exit_code;
  }

  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support_;

  // Helper thread to wait for process exit without blocking the main thread.
  base::Thread wait_for_process_exit_thread_{"wait_for_process_exit"};
};

TEST_F(UpdaterIPCTestCase, AllRpcsComplete) {
  scoped_refptr<MockUpdateService> mock_service =
      base::MakeRefCounted<MockUpdateService>();

  // The RPC calls should be received and processed in order.
  EXPECT_CALL(*mock_service, GetVersion)
      .WillOnce([](base::OnceCallback<void(const base::Version&)> callback) {
        std::move(callback).Run(base::Version("9"));
      });

  EXPECT_CALL(*mock_service, FetchPolicies)
      .WillOnce([](base::OnceCallback<void(int)> callback) {
        std::move(callback).Run(42);
      });

  EXPECT_CALL(*mock_service, RegisterApp)
      .WillOnce([](const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) {
        ExpectRegistrationRequestsEqual(request,
                                        GetExampleRegistrationRequest());
        std::move(callback).Run(42);
      });

  EXPECT_CALL(*mock_service, GetAppStates)
      .WillOnce([](base::OnceCallback<void(
                       const std::vector<UpdateService::AppState>&)> callback) {
        std::move(callback).Run(GetExampleAppStates());
      });

  EXPECT_CALL(*mock_service, RunPeriodicTasks)
      .WillOnce([](base::OnceClosure callback) { std::move(callback).Run(); });

  EXPECT_CALL(*mock_service, UpdateAll)
      .WillOnce(
          [](base::RepeatingCallback<void(const UpdateService::UpdateState&)>
                 state_change_callback,
             base::OnceCallback<void(UpdateService::Result)> callback) {
            for (const UpdateService::UpdateState& state :
                 GetExampleUpdateStates()) {
              state_change_callback.Run(state);
            }
            std::move(callback).Run(UpdateService::Result::kInstallFailed);
          });

  EXPECT_CALL(*mock_service, Update)
      .WillOnce(
          [](const std::string& app_id, const std::string& install_data_index,
             UpdateService::Priority priority,
             UpdateService::PolicySameVersionUpdate policy_same_version_update,
             base::RepeatingCallback<void(const UpdateService::UpdateState&)>
                 state_change_callback,
             base::OnceCallback<void(UpdateService::Result)> callback) {
            EXPECT_EQ(app_id, "ex1");
            EXPECT_EQ(install_data_index, "install_data_index");
            EXPECT_EQ(priority, UpdateService::Priority::kBackground);
            EXPECT_EQ(policy_same_version_update,
                      UpdateService::PolicySameVersionUpdate::kAllowed);

            for (const UpdateService::UpdateState& state :
                 GetExampleUpdateStates()) {
              state_change_callback.Run(state);
            }
            std::move(callback).Run(UpdateService::Result::kInstallFailed);
          });

  EXPECT_CALL(*mock_service, Install)
      .WillOnce(
          [](const RegistrationRequest&, const std::string& client_install_data,
             const std::string& install_data_index,
             UpdateService::Priority priority,
             base::RepeatingCallback<void(const UpdateService::UpdateState&)>
                 state_change_callback,
             base::OnceCallback<void(UpdateService::Result)> callback) {
            EXPECT_EQ(client_install_data, "client_install_data");
            EXPECT_EQ(install_data_index, "install_data_index");
            EXPECT_EQ(priority, UpdateService::Priority::kForeground);

            for (const UpdateService::UpdateState& state :
                 GetExampleUpdateStates()) {
              state_change_callback.Run(state);
            }
            std::move(callback).Run(UpdateService::Result::kInstallFailed);
          });

  EXPECT_CALL(*mock_service, CancelInstalls)
      .WillOnce([](const std::string& app_id) { EXPECT_EQ(app_id, "ex1"); });

  EXPECT_CALL(*mock_service, RunInstaller)
      .WillOnce(
          [](const std::string& app_id, const base::FilePath& installer_path,
             const std::string& install_args, const std::string& install_data,
             const std::string& install_settings,
             base::RepeatingCallback<void(const UpdateService::UpdateState&)>
                 state_change_callback,
             base::OnceCallback<void(UpdateService::Result)> callback) {
            EXPECT_EQ(app_id, "ex1");
            EXPECT_EQ(installer_path, base::FilePath("/path/to/installer"));
            EXPECT_EQ(install_args, "install_args");
            EXPECT_EQ(install_data, "install_data");
            EXPECT_EQ(install_settings, "install_settings");

            for (const UpdateService::UpdateState& state :
                 GetExampleUpdateStates()) {
              state_change_callback.Run(state);
            }
            std::move(callback).Run(UpdateService::Result::kInstallFailed);
          });

  // Create a stub and wait for the endpoint to be created before launching the
  // client process.
  base::RunLoop run_loop;
  auto service_stub = std::unique_ptr<UpdateServiceStub>(new UpdateServiceStub(
      mock_service, UpdaterScope::kUser, base::DoNothing(), base::DoNothing(),
      run_loop.QuitClosure()));
  run_loop.Run();

  base::Process child_process = base::SpawnMultiProcessTestChild(
      kClientProcessName, base::GetMultiProcessTestChildBaseCommandLine(),
      /*options=*/{});
  EXPECT_EQ(WaitForProcessExit(child_process), 0);
}

MULTIPROCESS_TEST_MAIN(UpdateServiceClient) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support;
  scoped_refptr<UpdateService> client_proxy =
      CreateUpdateServiceProxy(UpdaterScope::kUser);
  {
    base::RunLoop run_loop;
    client_proxy->GetVersion(base::BindOnce([](const base::Version& version) {
                               EXPECT_EQ(version, base::Version("9"));
                             }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    client_proxy->FetchPolicies(base::BindOnce([](int result) {
                                  EXPECT_EQ(result, 42);
                                }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    client_proxy->RegisterApp(
        UpdaterIPCTestCase::GetExampleRegistrationRequest(),
        base::BindOnce([](int result) {
          EXPECT_EQ(result, 42);
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    client_proxy->GetAppStates(
        base::BindOnce([](const std::vector<UpdateService::AppState>&
                              app_states) {
          ASSERT_EQ(app_states.size(), 2U);

          EXPECT_EQ(app_states[0].app_id, "ex1");
          EXPECT_EQ(app_states[0].version, base::Version("9.19.20"));
          EXPECT_EQ(app_states[0].ap, "foo");
          EXPECT_EQ(app_states[0].brand_code, "FooBarInc");
          EXPECT_EQ(app_states[0].brand_path,
                    base::FilePath("/path/to/foo_bar"));
          EXPECT_EQ(app_states[0].ecp, base::FilePath("path/to/foo_ecp"));

          EXPECT_EQ(app_states[1].app_id, "ex2");
          EXPECT_EQ(app_states[1].version, base::Version("98.4.5"));
          EXPECT_EQ(app_states[1].ap, "zaz");
          EXPECT_EQ(app_states[1].brand_code, "BazInc");
          EXPECT_EQ(app_states[1].brand_path, base::FilePath("/path/to/baz"));
          EXPECT_EQ(app_states[1].ecp, base::FilePath("path/to/baz_ecp"));
        }).Then(run_loop.QuitClosure()));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    client_proxy->RunPeriodicTasks(run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    client_proxy->UpdateAll(UpdaterIPCTestCase::ExpectUpdateStatesCallback(),
                            UpdaterIPCTestCase::ExpectResultCallback(run_loop));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    client_proxy->Update("ex1", "install_data_index",
                         UpdateService::Priority::kBackground,
                         UpdateService::PolicySameVersionUpdate::kAllowed,
                         UpdaterIPCTestCase::ExpectUpdateStatesCallback(),
                         UpdaterIPCTestCase::ExpectResultCallback(run_loop));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    RegistrationRequest request;
    client_proxy->Install(request, "client_install_data", "install_data_index",
                          UpdateService::Priority::kForeground,
                          UpdaterIPCTestCase::ExpectUpdateStatesCallback(),
                          UpdaterIPCTestCase::ExpectResultCallback(run_loop));
    run_loop.Run();
  }

  client_proxy->CancelInstalls("ex1");

  {
    RegistrationRequest request;
    base::RunLoop run_loop;
    client_proxy->RunInstaller(
        "ex1", base::FilePath("/path/to/installer"), "install_args",
        "install_data", "install_settings",
        UpdaterIPCTestCase::ExpectUpdateStatesCallback(),
        UpdaterIPCTestCase::ExpectResultCallback(run_loop));
    run_loop.Run();
  }

  return 0;
}

class FakeUpdateServiceInternal : public UpdateServiceInternal {
 public:
  enum class FuncTag { kRun, kHello };

  explicit FakeUpdateServiceInternal(
      const base::RepeatingCallback<void(FuncTag)>& on_ipc_callback)
      : on_ipc_callback_(on_ipc_callback) {}

  // Overrides for UpdateServiceInternal
  void Run(base::OnceClosure callback) override {
    on_ipc_callback_.Run(FuncTag::kRun);
    std::move(callback).Run();
  }

  void Hello(base::OnceClosure callback) override {
    on_ipc_callback_.Run(FuncTag::kHello);
    std::move(callback).Run();
  }

 private:
  ~FakeUpdateServiceInternal() override = default;
  base::RepeatingCallback<void(FuncTag)> on_ipc_callback_;
};

class UpdaterIPCInternalTestCase : public UpdaterIPCTestCase {
 public:
  static constexpr char kClientProcessName[] = "UpdateServiceInternalClient";
};

TEST_F(UpdaterIPCInternalTestCase, AllIpcsComplete) {
  base::MockCallback<
      base::RepeatingCallback<void(FakeUpdateServiceInternal::FuncTag)>>
      on_ipc_callback;
  // The Ipc calls should be received and processed in order.
  EXPECT_CALL(on_ipc_callback, Run(FakeUpdateServiceInternal::FuncTag::kRun));
  EXPECT_CALL(on_ipc_callback, Run(FakeUpdateServiceInternal::FuncTag::kHello));

  auto service_stub = std::make_unique<UpdateServiceInternalStub>(
      base::MakeRefCounted<FakeUpdateServiceInternal>(on_ipc_callback.Get()),
      UpdaterScope::kUser, base::DoNothing(), base::DoNothing());

  base::Process child_process = base::SpawnMultiProcessTestChild(
      kClientProcessName, base::GetMultiProcessTestChildBaseCommandLine(),
      /*options=*/{});
  EXPECT_EQ(WaitForProcessExit(child_process), 0);
}

MULTIPROCESS_TEST_MAIN(UpdateServiceInternalClient) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support;
  scoped_refptr<UpdateServiceInternal> client_proxy =
      CreateUpdateServiceInternalProxy(UpdaterScope::kUser);
  {
    base::RunLoop wait_for_response_run_loop;
    client_proxy->Run(wait_for_response_run_loop.QuitClosure());
    wait_for_response_run_loop.Run();
  }
  {
    base::RunLoop wait_for_response_run_loop;
    client_proxy->Hello(wait_for_response_run_loop.QuitClosure());
    wait_for_response_run_loop.Run();
  }
  return 0;
}

}  // namespace updater
