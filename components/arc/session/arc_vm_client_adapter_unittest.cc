// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr const char kUserIdHash[] = "this_is_a_valid_user_id_hash";
constexpr const char kSerialNumber[] = "AAAABBBBCCCCDDDD1234";
constexpr int64_t kCid = 123;

StartParams GetPopulatedStartParams() {
  StartParams params;
  params.native_bridge_experiment = true;
  params.lcd_density = 240;
  params.arc_file_picker_experiment = true;
  params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON;
  params.arc_custom_tabs_experiment = true;
  params.arc_print_spooler_experiment = true;
  params.num_cores_disabled = 2;
  return params;
}

UpgradeParams GetPopulatedUpgradeParams() {
  UpgradeParams params;
  params.account_id = "fee1dead";
  params.skip_boot_completed_broadcast = true;
  params.packages_cache_mode = UpgradeParams::PackageCacheMode::COPY_ON_INIT;
  params.skip_gms_core_cache = true;
  params.supervision_transition = ArcSupervisionTransition::CHILD_TO_REGULAR;
  params.locale = "en-US";
  params.preferred_languages = {"en_US", "en", "ja"};
  params.is_demo_session = true;
  params.demo_session_apps_path = base::FilePath("/pato/to/demo.apk");
  return params;
}

// A debugd client that can fail to start Concierge.
// TODO(yusukes): Merge the feature to FakeDebugDaemonClient.
class TestDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void StartConcierge(ConciergeCallback callback) override {
    start_concierge_called_ = true;
    std::move(callback).Run(start_concierge_result_);
  }

  bool start_concierge_called() const { return start_concierge_called_; }
  void set_start_concierge_result(bool result) {
    start_concierge_result_ = result;
  }

 private:
  bool start_concierge_called_ = false;
  bool start_concierge_result_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestDebugDaemonClient);
};

// A concierge that remembers the parameter passed to StartArcVm.
// TODO(yusukes): Merge the feature to FakeConciergeClient.
class TestConciergeClient : public chromeos::FakeConciergeClient {
 public:
  TestConciergeClient() = default;
  ~TestConciergeClient() override = default;

  void StartArcVm(
      const vm_tools::concierge::StartArcVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override {
    start_arc_vm_request_ = request;
    chromeos::FakeConciergeClient::StartArcVm(request, std::move(callback));
  }

  const vm_tools::concierge::StartArcVmRequest& start_arc_vm_request() const {
    return start_arc_vm_request_;
  }

 private:
  vm_tools::concierge::StartArcVmRequest start_arc_vm_request_;

  DISALLOW_COPY_AND_ASSIGN(TestConciergeClient);
};

class ArcVmClientAdapterTest : public testing::Test,
                               public ArcClientAdapter::Observer {
 public:
  ArcVmClientAdapterTest() {
    // Create and set new fake clients every time to reset clients' status.
    chromeos::DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
        std::make_unique<TestDebugDaemonClient>());
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        std::make_unique<TestConciergeClient>());
    chromeos::UpstartClient::InitializeFake();
  }

  ~ArcVmClientAdapterTest() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        nullptr);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetDebugDaemonClient(
        nullptr);
  }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    adapter_ = CreateArcVmClientAdapter(version_info::Channel::STABLE);
    arc_instance_stopped_called_ = false;
    adapter_->AddObserver(this);
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    // The fake client returns VM_STATUS_STARTING by default. Change it
    // to VM_STATUS_RUNNING which is used by ARCVM.
    vm_tools::concierge::StartVmResponse start_vm_response;
    start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
    auto* vm_info = start_vm_response.mutable_vm_info();
    vm_info->set_cid(kCid);
    GetTestConciergeClient()->set_start_vm_response(start_vm_response);
  }

  void TearDown() override {
    adapter_->RemoveObserver(this);
    adapter_.reset();
    run_loop_.reset();
  }

  // ArcClientAdapter::Observer:
  void ArcInstanceStopped() override {
    arc_instance_stopped_called_ = true;
    run_loop()->Quit();
  }

  void ExpectTrueThenQuit(bool result) {
    EXPECT_TRUE(result);
    run_loop()->Quit();
  }

  void ExpectFalseThenQuit(bool result) {
    EXPECT_FALSE(result);
    run_loop()->Quit();
  }

 protected:
  bool GetStartConciergeCalled() {
    return GetTestDebugDaemonClient()->start_concierge_called();
  }

  void SetStartConciergeResponse(bool response) {
    GetTestDebugDaemonClient()->set_start_concierge_result(response);
  }

  void SetValidUserInfo() {
    adapter()->SetUserInfo(kUserIdHash, kSerialNumber);
  }

  void StartMiniArcWithParams(StartParams params) {
    adapter()->StartMiniArc(
        std::move(params),
        base::BindOnce(&ArcVmClientAdapterTest::ExpectTrueThenQuit,
                       base::Unretained(this)));
    run_loop()->Run();
    RecreateRunLoop();
  }

  void UpgradeArcWithParams(bool expect_success, UpgradeParams params) {
    adapter()->UpgradeArc(
        std::move(params),
        base::BindOnce(expect_success
                           ? &ArcVmClientAdapterTest::ExpectTrueThenQuit
                           : &ArcVmClientAdapterTest::ExpectFalseThenQuit,
                       base::Unretained(this)));
    run_loop()->Run();
    RecreateRunLoop();
  }

  // Starts mini instance with the default StartParams.
  void StartMiniArc() { StartMiniArcWithParams({}); }

  // Upgrades the instance with the default UpgradeParams.
  void UpgradeArc(bool expect_success) {
    UpgradeArcWithParams(expect_success, {});
  }

  void SendVmStartedSignal() {
    vm_tools::concierge::VmStartedSignal signal;
    signal.set_name(kArcVmName);
    for (auto& observer : GetTestConciergeClient()->vm_observer_list())
      observer.OnVmStarted(signal);
  }

  void SendVmStartedSignalNotForArcVm() {
    vm_tools::concierge::VmStartedSignal signal;
    signal.set_name("penguin");
    for (auto& observer : GetTestConciergeClient()->vm_observer_list())
      observer.OnVmStarted(signal);
  }

  void SendVmStoppedSignalForCid(int64_t cid) {
    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name(kArcVmName);
    signal.set_cid(cid);
    for (auto& observer : GetTestConciergeClient()->vm_observer_list())
      observer.OnVmStopped(signal);
  }

  void SendVmStoppedSignal() { SendVmStoppedSignalForCid(kCid); }

  void SendVmStoppedSignalNotForArcVm() {
    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name("penguin");
    signal.set_cid(kCid);
    for (auto& observer : GetTestConciergeClient()->vm_observer_list())
      observer.OnVmStopped(signal);
  }

  void SendNameOwnerChangedSignal() {
    for (auto& observer : GetTestConciergeClient()->observer_list())
      observer.ConciergeServiceStopped();
  }

  void RecreateRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  base::RunLoop* run_loop() { return run_loop_.get(); }
  ArcClientAdapter* adapter() { return adapter_.get(); }
  const base::FilePath& GetTempDir() const { return dir_.GetPath(); }

  bool arc_instance_stopped_called() const {
    return arc_instance_stopped_called_;
  }
  void reset_arc_instance_stopped_called() {
    arc_instance_stopped_called_ = false;
  }
  TestConciergeClient* GetTestConciergeClient() {
    return static_cast<TestConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

 private:
  TestDebugDaemonClient* GetTestDebugDaemonClient() {
    return static_cast<TestDebugDaemonClient*>(
        chromeos::DBusThreadManager::Get()->GetDebugDaemonClient());
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ArcClientAdapter> adapter_;
  bool arc_instance_stopped_called_;

  content::BrowserTaskEnvironment browser_task_environment_;
  base::ScopedTempDir dir_;

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapterTest);
};

// Tests that SetUserInfo() doesn't crash.
TEST_F(ArcVmClientAdapterTest, SetUserInfo) {
  adapter()->SetUserInfo(kUserIdHash, kSerialNumber);
}

// Tests that StartMiniArc() always succeeds.
TEST_F(ArcVmClientAdapterTest, StartMiniArc) {
  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StopArcInstance() eventually notifies the observer.
TEST_F(ArcVmClientAdapterTest, StopArcInstance) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  // The callback for StopVm D-Bus reply does NOT call ArcInstanceStopped when
  // the D-Bus call result is successful.
  EXPECT_FALSE(arc_instance_stopped_called());

  // Instead, vm_concierge explicitly notifies Chrome of the VM termination.
  RecreateRunLoop();
  SendVmStoppedSignal();
  run_loop()->Run();
  // ..and that calls ArcInstanceStopped.
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that StopArcInstance() called during shutdown doesn't do anything.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_OnShutdown) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/true);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Tests that StopArcInstance() immediately notifies the observer on failure.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_Fail) {
  StartMiniArc();

  // Inject failure.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);

  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  // The callback for StopVm D-Bus reply does call ArcInstanceStopped when
  // the D-Bus call result is NOT successful.
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles arcvm-server-proxy startup failures properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmProxyFailure) {
  SetValidUserInfo();
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  auto* upstart_client = chromeos::FakeUpstartClient::Get();
  upstart_client->set_start_job_result(false);

  UpgradeArc(false);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());
  upstart_client->set_start_job_result(true);

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles StartConcierge() failures properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartConciergeFailure) {
  SetValidUserInfo();
  StartMiniArc();
  // Inject failure to StartConcierge().
  SetStartConciergeResponse(false);
  UpgradeArc(false);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that "no user ID hash" failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_NoUserInfo) {
  StartMiniArc();

  // Don't call SetValidUserInfo(). Note that we cannot call StartArcVm()
  // without valid user info.
  UpgradeArc(false);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that StartArcVm() failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmFailure) {
  SetValidUserInfo();
  StartMiniArc();
  // Inject failure to StartArcVm().
  vm_tools::concierge::StartVmResponse start_vm_response;
  start_vm_response.set_status(vm_tools::concierge::VM_STATUS_UNKNOWN);
  GetTestConciergeClient()->set_start_vm_response(start_vm_response);

  UpgradeArc(false);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmFailureEmptyReply) {
  SetValidUserInfo();
  StartMiniArc();
  // Inject failure to StartArcVm(). This emulates D-Bus timeout situations.
  GetTestConciergeClient()->set_start_vm_response(base::nullopt);

  UpgradeArc(false);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that successful StartArcVm() call is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_Success) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM.
  adapter()->StopArcInstance(/*on_shutdown=*/false);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  RecreateRunLoop();
  SendVmStoppedSignal();
  run_loop()->Run();
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Try to start and upgrade the instance with more params.
TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_VariousParams) {
  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(std::move(start_params));

  UpgradeParams params(GetPopulatedUpgradeParams());
  UpgradeArcWithParams(true, std::move(params));
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Try to start and upgrade the instance with slightly different params
// than StartUpgradeArc_VariousParams for better code coverage.
TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_VariousParams2) {
  StartParams start_params(GetPopulatedStartParams());
  // Use slightly different params than StartUpgradeArc_VariousParams.
  start_params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF;

  SetValidUserInfo();
  StartMiniArcWithParams(std::move(start_params));

  UpgradeParams params(GetPopulatedUpgradeParams());
  // Use slightly different params than StartUpgradeArc_VariousParams.
  params.packages_cache_mode =
      UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT;
  params.supervision_transition = ArcSupervisionTransition::REGULAR_TO_CHILD;
  params.preferred_languages = {"en_US"};

  UpgradeArcWithParams(true, std::move(params));
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Tests that StartArcVm() is called with valid parameters.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmParams) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  ASSERT_TRUE(GetTestConciergeClient()->start_arc_vm_called());

  // Verify parameters
  const auto& params = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_EQ("arcvm", params.name());
  EXPECT_EQ(kUserIdHash, params.owner_id());
  EXPECT_LT(0u, params.cpus());
  EXPECT_FALSE(params.vm().kernel().empty());
  // Make sure system.raw.img is passed.
  EXPECT_FALSE(params.vm().rootfs().empty());
  // Make sure vendor.raw.img is passed.
  EXPECT_LE(1, params.disks_size());
  EXPECT_LT(0, params.params_size());
}

// Tests that crosvm crash is handled properly.
TEST_F(ArcVmClientAdapterTest, CrosvmCrash) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Kill crosvm and verify StopArcInstance is called.
  SendVmStoppedSignal();
  run_loop()->Run();
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that vm_concierge crash is handled properly.
TEST_F(ArcVmClientAdapterTest, ConciergeCrash) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Kill vm_concierge and verify StopArcInstance is called.
  SendNameOwnerChangedSignal();
  run_loop()->Run();
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests the case where crosvm crashes, then vm_concierge crashes too.
TEST_F(ArcVmClientAdapterTest, CrosvmAndConciergeCrashes) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Kill crosvm and verify StopArcInstance is called.
  SendVmStoppedSignal();
  run_loop()->Run();
  EXPECT_TRUE(arc_instance_stopped_called());

  // Kill vm_concierge and verify StopArcInstance is NOT called since
  // the observer has already been called.
  RecreateRunLoop();
  reset_arc_instance_stopped_called();
  SendNameOwnerChangedSignal();
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Tests the case where a unknown VmStopped signal is sent to Chrome.
TEST_F(ArcVmClientAdapterTest, VmStoppedSignal_UnknownCid) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  EXPECT_TRUE(GetStartConciergeCalled());
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  SendVmStoppedSignalForCid(42);  // unknown CID
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Tests the case where a stale VmStopped signal is sent to Chrome.
TEST_F(ArcVmClientAdapterTest, VmStoppedSignal_Stale) {
  SendVmStoppedSignalForCid(42);
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Tests the case where a VmStopped signal not for ARCVM (e.g. Termina) is sent
// to Chrome.
TEST_F(ArcVmClientAdapterTest, VmStoppedSignal_Termina) {
  SendVmStoppedSignalNotForArcVm();
  run_loop()->RunUntilIdle();
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Tests that receiving VmStarted signal is no-op.
TEST_F(ArcVmClientAdapterTest, VmStartedSignal) {
  SendVmStartedSignal();
  run_loop()->RunUntilIdle();
  RecreateRunLoop();
  SendVmStartedSignalNotForArcVm();
  run_loop()->RunUntilIdle();
}

// Tests if androidboot.debuggable is set properly.
TEST_F(ArcVmClientAdapterTest, IsAndroidDebuggable) {
  constexpr const char kAndroidDebuggableTrueJson[] = R"json({
    "ANDROID_DEBUGGABLE": true
  })json";
  constexpr const char kAndroidDebuggableFalseJson[] = R"json({
    "ANDROID_DEBUGGABLE": false
  })json";
  constexpr const char kInvalidTypeJson[] = R"json([
    42
  ])json";
  constexpr const char kInvalidJson[] = R"json({
    "ANDROID_DEBUGGABLE": true,
  })json";
  constexpr const char kKeyNotFoundJson[] = R"json({
    "BADKEY": "a"
  })json";
  constexpr const char kNonBooleanValue[] = R"json({
    "ANDROID_DEBUGGABLE": "a"
  })json";
  constexpr const char kBadKeyType[] = R"json({
    42: true
  })json";

  auto test = [](const base::FilePath& dir, const std::string& str) {
    base::FilePath path;
    if (!CreateTemporaryFileInDir(dir, &path))
      return false;
    base::WriteFile(path, str.data(), str.size());
    return IsAndroidDebuggableForTesting(path);
  };

  EXPECT_TRUE(test(GetTempDir(), kAndroidDebuggableTrueJson));
  EXPECT_FALSE(test(GetTempDir(), kAndroidDebuggableFalseJson));
  EXPECT_FALSE(test(GetTempDir(), kInvalidTypeJson));
  EXPECT_FALSE(test(GetTempDir(), kInvalidJson));
  EXPECT_FALSE(test(GetTempDir(), kKeyNotFoundJson));
  EXPECT_FALSE(test(GetTempDir(), kNonBooleanValue));
  EXPECT_FALSE(test(GetTempDir(), kBadKeyType));
}

// Tests the case where the json file doesn't exist.
TEST_F(ArcVmClientAdapterTest, IsAndroidDebuggable_NonExistent) {
  EXPECT_FALSE(
      IsAndroidDebuggableForTesting(base::FilePath("/nonexistent-path")));
}

// Tests the case where the json file is not readable.
TEST_F(ArcVmClientAdapterTest, IsAndroidDebuggable_CannotRead) {
  constexpr const char kValidJson[] = R"json({
    "ANDROID_DEBUGGABLE": true
  })json";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidJson, strlen(kValidJson));
  base::SetPosixFilePermissions(path, 0300);  // not readable
  EXPECT_FALSE(IsAndroidDebuggableForTesting(path));
}

TEST_F(ArcVmClientAdapterTest, ExpandPropertyFilesForTesting_NoSource) {
  // Both source and dest are not found.
  EXPECT_FALSE(ExpandPropertyFilesForTesting(base::FilePath("/nonexistent1"),
                                             base::FilePath("/nonexistent2")));

  // Both source and dest exist, but the source directory is empty.
  base::FilePath source_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &source_dir));
  base::FilePath dest_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(GetTempDir(), "test", &dest_dir));
  EXPECT_FALSE(ExpandPropertyFilesForTesting(source_dir, dest_dir));

  // Add default.prop to the source, but not build.prop.
  base::FilePath default_prop = source_dir.Append("default.prop");
  constexpr const char kDefaultProp[] = "ro.foo=bar\n";
  base::WriteFile(default_prop, kDefaultProp, strlen(kDefaultProp));
  EXPECT_FALSE(ExpandPropertyFilesForTesting(source_dir, dest_dir));

  // Add build.prop too. Then the call should succeed.
  base::FilePath build_prop = source_dir.Append("build.prop");
  constexpr const char kBuildProp[] = "ro.baz=boo\n";
  base::WriteFile(build_prop, kBuildProp, strlen(kBuildProp));
  EXPECT_TRUE(ExpandPropertyFilesForTesting(source_dir, dest_dir));

  // Verify two dest files are there.
  EXPECT_TRUE(base::PathExists(dest_dir.Append("default.prop")));
  EXPECT_TRUE(base::PathExists(dest_dir.Append("build.prop")));

  // Verify their content.
  // Note: ExpandPropertyFile() adds a trailing LF.
  std::string content;
  EXPECT_TRUE(
      base::ReadFileToString(dest_dir.Append("default.prop"), &content));
  EXPECT_EQ(std::string(kDefaultProp) + "\n", content);
  EXPECT_TRUE(base::ReadFileToString(dest_dir.Append("build.prop"), &content));
  EXPECT_EQ(std::string(kBuildProp) + "\n", content);

  // Finally, test the case where source is valid but the dest is not.
  EXPECT_FALSE(ExpandPropertyFilesForTesting(source_dir,
                                             base::FilePath("/nonexistent")));
}

TEST_F(ArcVmClientAdapterTest, IsSystemImageExtFormatForTesting_FileMissing) {
  EXPECT_FALSE(
      IsSystemImageExtFormatForTesting(base::FilePath("/nonexistent")));
}

TEST_F(ArcVmClientAdapterTest,
       IsSystemImageExtFormatForTesting_FileSizeTooSmall) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  char data[100];
  memset(data, 0, sizeof(data));
  base::WriteFile(file, data, sizeof(data));

  EXPECT_FALSE(IsSystemImageExtFormatForTesting(file));
}

TEST_F(ArcVmClientAdapterTest,
       IsSystemImageExtFormatForTesting_MagicNumberDoesNotMatch) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  char data[2048];
  memset(data, 0, sizeof(data));
  base::WriteFile(file, data, sizeof(data));

  EXPECT_FALSE(IsSystemImageExtFormatForTesting(file));
}

TEST_F(ArcVmClientAdapterTest,
       IsSystemImageExtFormatForTesting_MagicNumberMatches) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  char data[2048];
  memset(data, 0, sizeof(data));
  // Magic signature (0xEF53) is in little-endian order.
  data[0x400 + 0x38] = 0x53;
  data[0x400 + 0x39] = 0xEF;
  base::WriteFile(file, data, sizeof(data));

  EXPECT_TRUE(IsSystemImageExtFormatForTesting(file));
}

}  // namespace
}  // namespace arc
