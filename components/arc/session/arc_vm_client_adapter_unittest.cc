// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/upstart/fake_upstart_client.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/file_system_status.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr const char kArcCreateDataJobName[] = "arc_2dcreate_2ddata";
constexpr const char kArcHostClockServiceJobName[] =
    "arc_2dhost_2dclock_2dservice";
constexpr const char kArcKeymasterJobName[] = "arc_2dkeymasterd";
constexpr const char kArcSensorServiceJobName[] = "arc_2dsensor_2dservice";
constexpr const char kArcVmMountMyFilesJobName[] = "arcvm_2dmount_2dmyfiles";
constexpr const char kArcVmMountRemovableMediaJobName[] =
    "arcvm_2dmount_2dremovable_2dmedia";
constexpr const char kArcVmServerProxyJobName[] = "arcvm_2dserver_2dproxy";
constexpr const char kArcVmAdbdJobName[] = "arcvm_2dadbd";
constexpr const char kArcVmPerBoardFeaturesJobName[] =
    "arcvm_2dper_2dboard_2dfeatures";
constexpr const char kArcVmBootNotificationServerJobName[] =
    "arcvm_2dboot_2dnotification_2dserver";
constexpr const size_t kUnixMaxPathLen = sizeof(sockaddr_un::sun_path);
constexpr const char kArcVmBootNotificationServerAddress[kUnixMaxPathLen] =
    "\0test_arcvm_boot_notification_server";

constexpr const char kUserIdHash[] = "this_is_a_valid_user_id_hash";
constexpr const char kSerialNumber[] = "AAAABBBBCCCCDDDD1234";
constexpr int64_t kCid = 123;

StartParams GetPopulatedStartParams() {
  StartParams params;
  params.native_bridge_experiment = false;
  params.lcd_density = 240;
  params.arc_file_picker_experiment = true;
  params.play_store_auto_update =
      StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON;
  params.arc_custom_tabs_experiment = true;
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

  void BackupArcBugReport(const cryptohome::AccountIdentifier& id,
                          chromeos::VoidDBusMethodCallback callback) override {
    backup_arc_bug_report_called_ = true;
    std::move(callback).Run(backup_arc_bug_report_result_);
  }

  bool backup_arc_bug_report_called() const {
    return backup_arc_bug_report_called_;
  }
  void set_backup_arc_bug_report_result(bool result) {
    backup_arc_bug_report_result_ = result;
  }

 private:
  bool backup_arc_bug_report_called_ = false;
  bool backup_arc_bug_report_result_ = true;

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

// A fake ArcVmBootNotificationServer that listens on an UDS and records
// connections and the data sent to it.
class TestArcVmBootNotificationServer
    : public base::MessagePumpForUI::FdWatcher {
 public:
  TestArcVmBootNotificationServer() = default;
  ~TestArcVmBootNotificationServer() override { Stop(); }
  TestArcVmBootNotificationServer(const TestArcVmBootNotificationServer&) =
      delete;
  TestArcVmBootNotificationServer& operator=(
      const TestArcVmBootNotificationServer&) = delete;

  // Creates a socket and binds it to a name in the abstract namespace, then
  // starts listening to the socket on another thread.
  void Start() {
    fd_.reset(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0));
    ASSERT_TRUE(fd_.is_valid());

    sockaddr_un addr{.sun_family = AF_UNIX};
    memcpy(addr.sun_path, kArcVmBootNotificationServerAddress,
           sizeof(kArcVmBootNotificationServerAddress));

    ASSERT_EQ(HANDLE_EINTR(bind(fd_.get(), reinterpret_cast<sockaddr*>(&addr),
                                sizeof(sockaddr_un))),
              0);
    ASSERT_EQ(HANDLE_EINTR(listen(fd_.get(), 5)), 0);

    controller_.reset(new base::MessagePumpForUI::FdWatchController(FROM_HERE));
    ASSERT_TRUE(base::CurrentUIThread::Get()->WatchFileDescriptor(
        fd_.get(), true, base::MessagePumpForUI::WATCH_READ, controller_.get(),
        this));
  }

  // Release the socket.
  void Stop() {
    controller_.reset(nullptr);
    fd_.reset(-1);
  }

  // Sets a callback to be run immediately after the next connection.
  void SetConnectionCallback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  int connection_count() { return num_connections_; }

  std::string received_data() { return received_; }

  // base::MessagePumpForUI::FdWatcher overrides
  void OnFileCanReadWithoutBlocking(int fd) override {
    base::ScopedFD client_fd(HANDLE_EINTR(accept(fd_.get(), nullptr, nullptr)));
    ASSERT_TRUE(client_fd.is_valid());

    ++num_connections_;

    // Attempt to read from connection until EOF
    std::string out;
    char buf[256];
    while (true) {
      ssize_t len = HANDLE_EINTR(read(client_fd.get(), buf, sizeof(buf)));
      if (len <= 0)
        break;
      out.append(buf, len);
    }
    received_.append(out);

    if (callback_)
      std::move(callback_).Run();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  base::ScopedFD fd_;
  std::unique_ptr<base::MessagePumpForUI::FdWatchController> controller_;
  int num_connections_ = 0;
  std::string received_;
  base::OnceClosure callback_;
};

class ArcVmClientAdapterTest : public testing::Test,
                               public ArcClientAdapter::Observer {
 public:
  ArcVmClientAdapterTest() {
    // Use the same VLOG() level as production. Note that session_manager sets
    // "--vmodule=*arc/*=1" in src/platform2/login_manager/chrome_setup.cc.
    logging::SetMinLogLevel(-1);

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
    adapter_ = CreateArcVmClientAdapterForTesting(base::BindRepeating(
        &ArcVmClientAdapterTest::RewriteStatus, base::Unretained(this)));
    arc_instance_stopped_called_ = false;
    adapter_->AddObserver(this);
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    host_rootfs_writable_ = false;
    system_image_ext_format_ = false;

    // The fake client returns VM_STATUS_STARTING by default. Change it
    // to VM_STATUS_RUNNING which is used by ARCVM.
    vm_tools::concierge::StartVmResponse start_vm_response;
    start_vm_response.set_status(vm_tools::concierge::VM_STATUS_RUNNING);
    auto* vm_info = start_vm_response.mutable_vm_info();
    vm_info->set_cid(kCid);
    GetTestConciergeClient()->set_start_vm_response(start_vm_response);

    // Reset to the original behavior.
    RemoveUpstartStartStopJobFailures();

    boot_server_ = std::make_unique<TestArcVmBootNotificationServer>();
    boot_server_->Start();
    SetArcVmBootNotificationServerAddressForTesting(
        std::string(kArcVmBootNotificationServerAddress,
                    sizeof(kArcVmBootNotificationServerAddress)),
        // connect_timeout_limit
        base::TimeDelta::FromMilliseconds(100),
        // connect_sleep_duration_initial
        base::TimeDelta::FromMilliseconds(20));
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
  void SetValidUserInfo() { SetUserInfo(kUserIdHash, kSerialNumber); }

  void SetUserInfo(const std::string& hash, const std::string& serial) {
    adapter()->SetUserInfo(
        cryptohome::Identification(user_manager::StubAccountId()), hash,
        serial);
  }

  void StartMiniArcWithParams(bool expect_success, StartParams params) {
    adapter()->StartMiniArc(
        std::move(params),
        base::BindOnce(expect_success
                           ? &ArcVmClientAdapterTest::ExpectTrueThenQuit
                           : &ArcVmClientAdapterTest::ExpectFalseThenQuit,
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
  void StartMiniArc() { StartMiniArcWithParams(true, {}); }

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

  void InjectUpstartStartJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void InjectUpstartStopJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_stop_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void StartRecordingUpstartOperations() {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, true);
          return true;
        }));
    upstart_client->set_stop_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, false);
          return true;
        }));
  }

  void RemoveUpstartStartStopJobFailures() {
    auto* upstart_client = chromeos::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        chromeos::FakeUpstartClient::StartStopJobCallback());
    upstart_client->set_stop_job_cb(
        chromeos::FakeUpstartClient::StartStopJobCallback());
  }

  void RecreateRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }

  base::RunLoop* run_loop() { return run_loop_.get(); }
  ArcClientAdapter* adapter() { return adapter_.get(); }

  bool arc_instance_stopped_called() const {
    return arc_instance_stopped_called_;
  }
  void reset_arc_instance_stopped_called() {
    arc_instance_stopped_called_ = false;
  }
  const std::vector<std::pair<std::string, bool>>& upstart_operations() const {
    return upstart_operations_;
  }
  TestConciergeClient* GetTestConciergeClient() {
    return static_cast<TestConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

  TestDebugDaemonClient* GetTestDebugDaemonClient() {
    return static_cast<TestDebugDaemonClient*>(
        chromeos::DBusThreadManager::Get()->GetDebugDaemonClient());
  }

  TestArcVmBootNotificationServer* boot_notification_server() {
    return boot_server_.get();
  }

  void set_host_rootfs_writable(bool host_rootfs_writable) {
    host_rootfs_writable_ = host_rootfs_writable;
  }

  void set_system_image_ext_format(bool system_image_ext_format) {
    system_image_ext_format_ = system_image_ext_format;
  }

 private:
  void RewriteStatus(FileSystemStatus* status) {
    status->set_host_rootfs_writable_for_testing(host_rootfs_writable_);
    status->set_system_image_ext_format_for_testing(system_image_ext_format_);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ArcClientAdapter> adapter_;
  bool arc_instance_stopped_called_;

  content::BrowserTaskEnvironment browser_task_environment_;
  base::ScopedTempDir dir_;

  // Variables to override the value in FileSystemStatus.
  bool host_rootfs_writable_;
  bool system_image_ext_format_;

  // List of upstart operations recorded. When it's "start" the boolean is set
  // to true.
  std::vector<std::pair<std::string, bool>> upstart_operations_;

  std::unique_ptr<TestArcVmBootNotificationServer> boot_server_;

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapterTest);
};

// Tests that SetUserInfo() doesn't crash.
TEST_F(ArcVmClientAdapterTest, SetUserInfo) {
  SetUserInfo(kUserIdHash, kSerialNumber);
}

// Tests that StartMiniArc() succeeds by default.
TEST_F(ArcVmClientAdapterTest, StartMiniArc) {
  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());

  // TODO(wvk): Once mini VM is supported, call StopArcInstance() and
  // SendVmStoppedSignal() here, then verify arc_instance_stopped_called()
  // becomes true. See StopArcInstance test for more details.
}

// Tests that StartMiniArc() still succeeds even when Upstart fails to stop
// the arcvm-server-proxy job.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcVmServerProxyJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmServerProxyJobName);

  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());

  // TODO(wvk): Once mini VM is supported, call StopArcInstance() here,
  // then verify arc_instance_stopped_called() never becomes true. Same
  // for other StartMiniArc_...Fail tests.
}

// Tests that StartMiniArc() fails if Upstart fails to start
// arc-host-clock-service.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcHostClockServiceJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcHostClockServiceJobName);

  StartMiniArcWithParams(false, {});
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() succeeds if Upstart fails to stop
// arc-host-clock-service.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcHostClockServiceJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcHostClockServiceJobName);

  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() fails if Upstart fails to start arc-keymasterd.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcKeymasterJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcKeymasterJobName);

  StartMiniArcWithParams(false, {});
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() succeeds if Upstart fails to stop arc-keymasterd.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcKeymasterJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcKeymasterJobName);

  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() fails if Upstart fails to start arc-sensor-service.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcSensorServiceJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcSensorServiceJobName);

  StartMiniArcWithParams(false, {});
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() succeeds if Upstart fails to stop
// arc-sensor-service.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcSensorServiceJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcSensorServiceJobName);

  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() still succeeds even when Upstart fails to stop
// arcvm-mount-myfiles.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StopArcVmMountMyFilesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmMountMyFilesJobName);

  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() still succeeds even when Upstart fails to stop
// arcvm-mount-removable-media.
TEST_F(ArcVmClientAdapterTest,
       StartMiniArc_StopArcVmMountRemovableMediaJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmMountRemovableMediaJobName);

  StartMiniArc();
  // Confirm that no VM is started. ARCVM doesn't support mini ARC yet.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc() fails when Upstart fails to start the job.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_StartArcVmPerBoardFeaturesJobFail) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmPerBoardFeaturesJobName);

  StartMiniArcWithParams(false, {});
  // Confirm that no VM is started.
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
}

// Tests that StartMiniArc()'s JOB_RESTART for |kArcSensorServiceJobName| is
// properly implemented.
TEST_F(ArcVmClientAdapterTest, StartMiniArc_JobRestart) {
  StartRecordingUpstartOperations();
  StartMiniArc();

  const auto& ops = upstart_operations();
  // Find the STOP operation for the job.
  auto it =
      std::find(ops.begin(), ops.end(),
                std::make_pair(std::string(kArcSensorServiceJobName), false));
  ASSERT_NE(ops.end(), it);
  ++it;
  ASSERT_NE(ops.end(), it);
  // The next operation must be START for the job.
  EXPECT_EQ(it->first, kArcSensorServiceJobName);
  EXPECT_TRUE(it->second);  // true means START.
}

// Tests that StopArcInstance() eventually notifies the observer.
TEST_F(ArcVmClientAdapterTest, StopArcInstance) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
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

// Tests that StopArcInstance() initiates ARC log backup.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_WithLogBackup) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/true);
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

TEST_F(ArcVmClientAdapterTest, StopArcInstance_WithLogBackup_BackupFailed) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);

  EXPECT_FALSE(GetTestDebugDaemonClient()->backup_arc_bug_report_called());
  GetTestDebugDaemonClient()->set_backup_arc_bug_report_result(false);

  adapter()->StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/true);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  // The callback for StopVm D-Bus reply does NOT call ArcInstanceStopped when
  // the D-Bus call result is successful.
  EXPECT_FALSE(arc_instance_stopped_called());

  // Instead, vm_concierge explicitly notifies Chrome of the VM termination.
  RecreateRunLoop();
  SendVmStoppedSignal();
  run_loop()->Run();

  EXPECT_TRUE(GetTestDebugDaemonClient()->backup_arc_bug_report_called());
  // ..and that calls ArcInstanceStopped.
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that StopArcInstance() called during shutdown doesn't do anything.
TEST_F(ArcVmClientAdapterTest, StopArcInstance_OnShutdown) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);

  adapter()->StopArcInstance(/*on_shutdown=*/true, /*should_backup_log=*/false);
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

  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
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
  InjectUpstartStartJobFailure(kArcVmServerProxyJobName);

  UpgradeArc(false);
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles arcvm-adbd stop failures properly.
// Note that arcvm-adbd is restarted AFTER ARCVM is started.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StopArcVmAdbdFailure) {
  SetValidUserInfo();
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmAdbdJobName);

  // Upgrade should still succeed.
  UpgradeArc(true);
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Make sure StopVm() is not called.
  EXPECT_FALSE(GetTestConciergeClient()->stop_vm_called());
}

// Tests that UpgradeArc() handles arcvm-adbd startup failures properly.
// Note that arcvm-adbd is restarted AFTER ARCVM is started.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmAdbdFailure) {
  SetValidUserInfo();
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmAdbdJobName);

  EnableAdbOverUsbForTesting();
  UpgradeArc(false);
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Make sure StopVm() *is* called.
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  // Run the loop and make sure the VM is stopped.
  SendVmStoppedSignal();
  run_loop()->Run();
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles arc-create-data startup failures properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcCreateDataFailure) {
  SetValidUserInfo();
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcCreateDataJobName);

  UpgradeArc(false);
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles arcvm-mount-myfiles startup failures
// properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_StartArcVmMountMyFilesJobFail) {
  SetValidUserInfo();
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmMountMyFilesJobName);

  UpgradeArc(false);
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that UpgradeArc() handles arcvm-mount-removable-media startup failures
// properly.
TEST_F(ArcVmClientAdapterTest,
       UpgradeArc_StartArcVmMountRemovableMediaJobFail) {
  SetValidUserInfo();
  StartMiniArc();

  // Inject failure to FakeUpstartClient.
  InjectUpstartStartJobFailure(kArcVmMountRemovableMediaJobName);

  UpgradeArc(false);
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that "no user ID hash" failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_NoUserId) {
  // Don't set the user id hash. Note that we cannot call StartArcVm() without
  // it.
  SetUserInfo(std::string(), kSerialNumber);
  StartMiniArc();

  UpgradeArc(false);
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that "no serial" failure is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_NoSerial) {
  // Don't set the serial number. Note that we cannot call StartArcVm() without
  // it.
  SetUserInfo(kUserIdHash, std::string());
  StartMiniArc();

  UpgradeArc(false);
  EXPECT_FALSE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
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
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
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
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM. StopVm will fail in this case because
  // no VM is running.
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  GetTestConciergeClient()->set_stop_vm_response(response);
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
  run_loop()->Run();
  EXPECT_TRUE(GetTestConciergeClient()->stop_vm_called());
  EXPECT_TRUE(arc_instance_stopped_called());
}

// Tests that successful StartArcVm() call is handled properly.
TEST_F(ArcVmClientAdapterTest, UpgradeArc_Success) {
  SetValidUserInfo();
  StartMiniArc();
  UpgradeArc(true);
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Try to stop the VM.
  adapter()->StopArcInstance(/*on_shutdown=*/false,
                             /*should_backup_log=*/false);
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
  StartMiniArcWithParams(true, std::move(start_params));

  UpgradeParams params(GetPopulatedUpgradeParams());
  UpgradeArcWithParams(true, std::move(params));
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
  StartMiniArcWithParams(true, std::move(start_params));

  UpgradeParams params(GetPopulatedUpgradeParams());
  // Use slightly different params than StartUpgradeArc_VariousParams.
  params.packages_cache_mode =
      UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT;
  params.supervision_transition = ArcSupervisionTransition::REGULAR_TO_CHILD;
  params.preferred_languages = {"en_US"};

  UpgradeArcWithParams(true, std::move(params));
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());
}

// Try to start and upgrade the instance with demo mode enabled.
TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_DemoMode) {
  constexpr char kDemoImage[] =
      "/run/imageloader/demo-mode-resources/0.0.1.7/android_demo_apps.squash";

  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));

  UpgradeParams params(GetPopulatedUpgradeParams());
  // Enable demo mode.
  params.is_demo_session = true;
  params.demo_session_apps_path = base::FilePath(kDemoImage);

  UpgradeArcWithParams(true, std::move(params));
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());

  // Verify the request.
  auto request = GetTestConciergeClient()->start_arc_vm_request();
  // Make sure disks have the squashfs image.
  EXPECT_TRUE(([&kDemoImage, &request]() {
    for (const auto& disk : request.disks()) {
      if (disk.path() == kDemoImage)
        return true;
    }
    return false;
  }()));
  EXPECT_TRUE(base::Contains(request.params(), "androidboot.arc_demo_mode=1"));
}

TEST_F(ArcVmClientAdapterTest, StartUpgradeArc_DisableSystemDefaultApp) {
  StartParams start_params(GetPopulatedStartParams());
  start_params.arc_disable_system_default_app = true;
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeParams params(GetPopulatedUpgradeParams());
  UpgradeArcWithParams(true, std::move(params));
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());
  EXPECT_FALSE(arc_instance_stopped_called());
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.disable_system_default_app=1"));
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

// Tests that ConciergeServiceStarted() doesn't crash.
TEST_F(ArcVmClientAdapterTest, TestConciergeServiceStarted) {
  StartMiniArc();
  for (auto& observer : GetTestConciergeClient()->observer_list())
    observer.ConciergeServiceStarted();
}

// Tests that the kernel parameter does not include "rw" by default.
TEST_F(ArcVmClientAdapterTest, KernelParam_RO) {
  SetValidUserInfo();
  StartMiniArc();
  set_host_rootfs_writable(false);
  set_system_image_ext_format(false);
  UpgradeArc(true);
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());

  // Check "rw" is not in |params|.
  auto request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_FALSE(base::Contains(request.params(), "rw"));
}

// Tests that the kernel parameter does include "rw" when '/' is writable and
// the image is in ext4.
TEST_F(ArcVmClientAdapterTest, KernelParam_RW) {
  SetValidUserInfo();
  StartMiniArc();
  set_host_rootfs_writable(true);
  set_system_image_ext_format(true);
  UpgradeArc(true);
  EXPECT_TRUE(GetTestConciergeClient()->start_arc_vm_called());

  // Check "rw" is in |params|.
  auto request = GetTestConciergeClient()->start_arc_vm_request();
  EXPECT_TRUE(base::Contains(request.params(), "rw"));
}

// Tests that CreateArcVmClientAdapter() doesn't crash.
TEST_F(ArcVmClientAdapterTest, TestCreateArcVmClientAdapter) {
  CreateArcVmClientAdapter();
}

TEST_F(ArcVmClientAdapterTest, ChromeOsChannelStable) {
  base::SysInfo::SetChromeOSVersionInfoForTest(
      "CHROMEOS_RELEASE_TRACK=stable-channel", base::Time::Now());

  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.chromeos_channel=stable"));
}

TEST_F(ArcVmClientAdapterTest, ChromeOsChannelUnknown) {
  base::SysInfo::SetChromeOSVersionInfoForTest("CHROMEOS_RELEASE_TRACK=invalid",
                                               base::Time::Now());

  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.chromeos_channel=unknown"));
}

// Tests that the binary translation type is set to None when no library is
// enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeNone) {
  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=0"));
}

// Tests that the binary translation type is set to Houdini when only 32-bit
// Houdini library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeHoudini) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini"});
  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=libhoudini.so"));
}

// Tests that the binary translation type is set to Houdini when only 64-bit
// Houdini library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeHoudini64) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini64"});
  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=libhoudini.so"));
}

// Tests that the binary translation type is set to NDK translation when only
// 32-bit NDK translation library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeNdkTranslation) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-ndk-translation"});
  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=libndk_translation.so"));
}

// Tests that the binary translation type is set to NDK translation when only
// 64-bit NDK translation library is enabled by USE flags.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeNdkTranslation64) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-ndk-translation64"});
  StartParams start_params(GetPopulatedStartParams());
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=libndk_translation.so"));
}

// Tests that the binary translation type is set to NDK translation when both
// Houdini and NDK translation libraries are enabled by USE flags, and the
// parameter start_params.native_bridge_experiment is set to true.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeNativeBridgeExperiment) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini", "--enable-ndk-translation"});
  StartParams start_params(GetPopulatedStartParams());
  start_params.native_bridge_experiment = true;
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=libndk_translation.so"));
}

// Tests that the binary translation type is set to Houdini when both Houdini
// and NDK translation libraries are enabled by USE flags, and the parameter
// start_params.native_bridge_experiment is set to false.
TEST_F(ArcVmClientAdapterTest, BintaryTranslationTypeNoNativeBridgeExperiment) {
  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--enable-houdini", "--enable-ndk-translation"});
  StartParams start_params(GetPopulatedStartParams());
  start_params.native_bridge_experiment = false;
  SetValidUserInfo();
  StartMiniArcWithParams(true, std::move(start_params));
  UpgradeArc(true);
  EXPECT_TRUE(
      base::Contains(GetTestConciergeClient()->start_arc_vm_request().params(),
                     "androidboot.native_bridge=libhoudini.so"));
}

// Tests that ArcVmClientAdapter connects to the boot notification server
// twice: once in StartMiniArc to check that it is listening, and the second
// time in UpgradeArc to send props.
TEST_F(ArcVmClientAdapterTest, TestConnectToBootNotificationServer) {
  // Stop the RunLoop after a connection to the server.
  boot_notification_server()->SetConnectionCallback(run_loop()->QuitClosure());
  SetValidUserInfo();
  adapter()->StartMiniArc(
      {}, base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  run_loop()->Run();

  EXPECT_EQ(boot_notification_server()->connection_count(), 1);
  EXPECT_TRUE(boot_notification_server()->received_data().empty());

  RecreateRunLoop();
  boot_notification_server()->SetConnectionCallback(run_loop()->QuitClosure());
  adapter()->UpgradeArc(
      GetPopulatedUpgradeParams(),
      base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  run_loop()->Run();

  EXPECT_EQ(boot_notification_server()->connection_count(), 2);
  EXPECT_FALSE(boot_notification_server()->received_data().empty());
  // Compare received data to expected output
  std::string expected_props =
      base::JoinString(GenerateUpgradeProps(GetPopulatedUpgradeParams(),
                                            kSerialNumber, "ro.boot"),
                       "\n");
  EXPECT_EQ(boot_notification_server()->received_data(), expected_props);
}

// Tests that StartMiniArc fails when the boot notification server's Upstart
// job fails.
TEST_F(ArcVmClientAdapterTest, TestBootNotificationServerUpstartJobFails) {
  InjectUpstartStartJobFailure(kArcVmBootNotificationServerJobName);

  StartMiniArcWithParams(false, {});
}

// Tests that StartMiniArc fails when the boot notification server is not
// listening.
TEST_F(ArcVmClientAdapterTest, TestBootNotificationServerIsNotListening) {
  boot_notification_server()->Stop();
  // Change timeout to 26 seconds to allow for exponential backoff.
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           base::TimeDelta::FromSeconds(26));

  StartMiniArcWithParams(false, {});
}

}  // namespace
}  // namespace arc
