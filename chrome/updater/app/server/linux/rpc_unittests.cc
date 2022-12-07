// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/server/linux/update_service_stub.h"
#include "chrome/updater/app/server/posix/mojom/updater_service.mojom.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/ipc/update_service_internal_proxy_posix.h"
#include "chrome/updater/ipc/update_service_proxy_linux.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace updater {

class FakeUpdateService : public UpdateService {
 public:
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override {
    std::move(callback).Run(base::Version("9"));
  }

  void FetchPolicies(base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(42);
  }

  void RegisterApp(const RegistrationRequest& request,
                   base::OnceCallback<void(int)> callback) override {
    std::move(callback).Run(42);
  }

  void GetAppStates(base::OnceCallback<void(const std::vector<AppState>&)>
                        callback) override {
    AppState ex1;
    ex1.app_id = "ex1";
    ex1.version = base::Version("9.19.20");
    ex1.ap = "foo";
    ex1.brand_code = "FooBarInc";
    ex1.brand_path = base::FilePath("/path/to/foo_bar");
    ex1.ecp = base::FilePath("path/to/foo_ecp");

    AppState ex2;
    ex2.app_id = "ex2";
    ex2.version = base::Version("98.4.5");
    ex2.ap = "zaz";
    ex2.brand_code = "BazInc";
    ex2.brand_path = base::FilePath("/path/to/baz");
    ex2.ecp = base::FilePath("path/to/baz_ecp");

    std::move(callback).Run({ex1, ex2});
  }

  void RunPeriodicTasks(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  void UpdateAll(StateChangeCallback state_update, Callback callback) override {
    DoStateChangeCallbacks(std::move(state_update), std::move(callback));
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index,
              Priority priority,
              PolicySameVersionUpdate policy_same_version_update,
              StateChangeCallback state_update,
              Callback callback) override {
    DoStateChangeCallbacks(std::move(state_update), std::move(callback));
  }

  void Install(const RegistrationRequest& registration,
               const std::string& client_install_data,
               const std::string& install_data_index,
               Priority priority,
               StateChangeCallback state_update,
               Callback callback) override {
    DoStateChangeCallbacks(std::move(state_update), std::move(callback));
  }

  void CancelInstalls(const std::string& app_id) override {}

  void RunInstaller(const std::string& app_id,
                    const base::FilePath& installer_path,
                    const std::string& install_args,
                    const std::string& install_data,
                    const std::string& install_settings,
                    StateChangeCallback state_update,
                    Callback callback) override {
    DoStateChangeCallbacks(std::move(state_update), std::move(callback));
  }

 private:
  ~FakeUpdateService() override = default;

  void DoStateChangeCallbacks(StateChangeCallback state_update,
                              Callback callback) {
    UpdateService::UpdateState state1;
    state1.app_id = "ex1";
    state1.state = UpdateService::UpdateState::State::kCheckingForUpdates;
    state_update.Run(state1);

    UpdateService::UpdateState state2;
    state2.app_id = "ex2";
    state2.state = UpdateService::UpdateState::State::kDownloading;
    state2.next_version = base::Version("3.14");
    state2.downloaded_bytes = 1024;
    state2.total_bytes = 2048;
    state_update.Run(state2);

    UpdateService::UpdateState state3;
    state3.app_id = "ex3";
    state3.state = UpdateService::UpdateState::State::kUpdateError;
    state3.install_progress = 99;
    state3.error_code = 0xDEAD;
    state3.extra_code1 = 0xBEEF;
    state3.installer_text = "Error: The beef has died.";
    state3.installer_cmd_line = "path/to/updater --crash-me";
    state_update.Run(state3);

    std::move(callback).Run(UpdateService::Result::kInstallFailed);
  }
};

class UpdaterIPCTestCase : public testing::Test {
 public:
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

  void SetUp() override {
    scoped_refptr<UpdateService> service =
        base::MakeRefCounted<FakeUpdateService>();
    service_stub_ = std::make_unique<UpdateServiceStub>(std::move(service),
                                                        UpdaterScope::kUser);
    client_proxy_ =
        CreateUpdateServiceProxy(UpdaterScope::kUser, base::Seconds(3));
  }

  UpdateService::StateChangeCallback ExpectUpdateStatesCallback() {
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

    std::vector<UpdateService::UpdateState> states = {state3, state2, state1};

    return base::BindRepeating(
        [](std::vector<UpdateService::UpdateState>& states,
           const UpdateService::UpdateState& state) {
          ASSERT_GT(states.size(), 0U);
          ExpectUpdateStatesEqual(state, states.back());
          states.pop_back();
        },
        base::OwnedRef(states));
  }

  UpdateService::Callback ExpectResultCallback() {
    return base::BindOnce([](UpdateService::Result result) {
             EXPECT_EQ(result, UpdateService::Result::kInstallFailed);
           })
        .Then(run_loop_.QuitClosure());
  }

 protected:
  ScopedIPCSupportWrapper ipc_support_;

  base::test::TaskEnvironment environment_;
  base::RunLoop run_loop_;

  std::unique_ptr<UpdateServiceStub> service_stub_;
  scoped_refptr<UpdateService> client_proxy_;
};

TEST_F(UpdaterIPCTestCase, GetVersion) {
  client_proxy_->GetVersion(base::BindOnce([](const base::Version& version) {
                              EXPECT_EQ(version, base::Version("9"));
                            }).Then(run_loop_.QuitClosure()));
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, FetchPolicies) {
  client_proxy_->FetchPolicies(base::BindOnce([](int result) {
                                 EXPECT_EQ(result, 42);
                               }).Then(run_loop_.QuitClosure()));
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, RegisterApp) {
  client_proxy_->RegisterApp({}, base::BindOnce([](int result) {
                                   EXPECT_EQ(result, 42);
                                 }).Then(run_loop_.QuitClosure()));
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, GetAppStates) {
  client_proxy_->GetAppStates(
      base::BindOnce([](const std::vector<UpdateService::AppState>&
                            app_states) {
        ASSERT_EQ(app_states.size(), 2U);

        EXPECT_EQ(app_states[0].app_id, "ex1");
        EXPECT_EQ(app_states[0].version, base::Version("9.19.20"));
        EXPECT_EQ(app_states[0].ap, "foo");
        EXPECT_EQ(app_states[0].brand_code, "FooBarInc");
        EXPECT_EQ(app_states[0].brand_path, base::FilePath("/path/to/foo_bar"));
        EXPECT_EQ(app_states[0].ecp, base::FilePath("path/to/foo_ecp"));

        EXPECT_EQ(app_states[1].app_id, "ex2");
        EXPECT_EQ(app_states[1].version, base::Version("98.4.5"));
        EXPECT_EQ(app_states[1].ap, "zaz");
        EXPECT_EQ(app_states[1].brand_code, "BazInc");
        EXPECT_EQ(app_states[1].brand_path, base::FilePath("/path/to/baz"));
        EXPECT_EQ(app_states[1].ecp, base::FilePath("path/to/baz_ecp"));
      }).Then(run_loop_.QuitClosure()));
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, UpdateAll) {
  client_proxy_->UpdateAll(ExpectUpdateStatesCallback(),
                           ExpectResultCallback());
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, Update) {
  client_proxy_->Update("ex1", "install_data_index",
                        UpdateService::Priority::kBackground,
                        UpdateService::PolicySameVersionUpdate::kAllowed,
                        ExpectUpdateStatesCallback(), ExpectResultCallback());
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, Install) {
  RegistrationRequest request;

  client_proxy_->Install(request, "client_install_data", "install_data_index",
                         UpdateService::Priority::kForeground,
                         ExpectUpdateStatesCallback(), ExpectResultCallback());
  run_loop_.Run();
}

TEST_F(UpdaterIPCTestCase, RunInstaller) {
  RegistrationRequest request;

  client_proxy_->RunInstaller("ex1", base::FilePath("/path/to/installer"),
                              "install_args", "install_data",
                              "install_settings", ExpectUpdateStatesCallback(),
                              ExpectResultCallback());
  run_loop_.Run();
}

class UpdaterIPCErrorTestCase : public UpdaterIPCTestCase {
 public:
  void SetUp() override {
    client_proxy_ = CreateUpdateServiceProxy(UpdaterScope::kUser);
  }
};

TEST_F(UpdaterIPCErrorTestCase, DroppedGetVersion) {
  client_proxy_->GetVersion(base::BindOnce([](const base::Version& version) {
                              EXPECT_FALSE(version.IsValid());
                            }).Then(run_loop_.QuitClosure()));

  run_loop_.Run();
}

TEST_F(UpdaterIPCErrorTestCase, DroppedUpdateAll) {
  client_proxy_->UpdateAll(
      base::BindRepeating([](const UpdateService::UpdateState& state) {}),
      base::BindOnce([](UpdateService::Result result) {
        EXPECT_EQ(result, UpdateService::Result::kIPCConnectionFailed);
      }).Then(run_loop_.QuitClosure()));

  run_loop_.Run();
}

class FakeUpdateServiceInternal : public UpdateServiceInternal {
 public:
  enum class FuncTag { Run, Hello };

  explicit FakeUpdateServiceInternal(
      const base::RepeatingCallback<void(FuncTag)>& on_rpc_callback)
      : on_rpc_callback_(on_rpc_callback) {}

  // Overrides for UpdateServiceInternal
  void Run(base::OnceClosure callback) override {
    on_rpc_callback_.Run(FuncTag::Run);
    std::move(callback).Run();
  }

  void Hello(base::OnceClosure callback) override {
    on_rpc_callback_.Run(FuncTag::Hello);
    std::move(callback).Run();
  }

 private:
  base::RepeatingCallback<void(FuncTag)> on_rpc_callback_;
  ~FakeUpdateServiceInternal() override = default;
};

class UpdaterIPCInternalTestCase : public testing::Test {
 public:
  static constexpr char kClientProcessName[] = "UpdateServiceInternalClient";
  static constexpr char kServerProcessName[] = "UpdateServiceInternalServer";

  UpdaterIPCInternalTestCase() {
    wait_for_process_exit_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
  }

 protected:
  int WaitForProcessExit(base::Process& process) {
    int exit_code;
    bool process_exited = false;
    base::RunLoop wait_for_process_exit_loop;
    wait_for_process_exit_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          process_exited = base::WaitForMultiprocessTestChildExit(
              process, TestTimeouts::action_timeout(), &exit_code);
        }),
        wait_for_process_exit_loop.QuitClosure());
    wait_for_process_exit_loop.Run();
    process.Close();
    EXPECT_TRUE(process_exited);
    return exit_code;
  }

 private:
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  ScopedIPCSupportWrapper ipc_support_;
  // Helper thread to wait for process exit without blocking the main thread.
  base::Thread wait_for_process_exit_thread_{"wait_for_process_exit"};
};

TEST_F(UpdaterIPCInternalTestCase, AllRpcsComplete) {
  base::MockCallback<
      base::RepeatingCallback<void(FakeUpdateServiceInternal::FuncTag)>>
      on_rpc_callback;
  // The RPC calls should be received and processed in order.
  EXPECT_CALL(on_rpc_callback, Run(FakeUpdateServiceInternal::FuncTag::Run));
  EXPECT_CALL(on_rpc_callback, Run(FakeUpdateServiceInternal::FuncTag::Hello));

  auto service_stub = std::make_unique<UpdateServiceInternalStub>(
      base::MakeRefCounted<FakeUpdateServiceInternal>(on_rpc_callback.Get()),
      UpdaterScope::kUser, base::DoNothing(), base::DoNothing());

  base::Process child_process = base::SpawnMultiProcessTestChild(
      kClientProcessName, base::GetMultiProcessTestChildBaseCommandLine(),
      /*options= */ {});
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
