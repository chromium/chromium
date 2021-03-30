// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/system/scheduler_configuration_manager_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/session/arc_client_adapter.h"
#include "components/arc/session/arc_session_impl.h"
#include "components/arc/session/arc_start_params.h"
#include "components/arc/session/arc_upgrade_params.h"
#include "components/arc/test/fake_arc_bridge_host.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptohome {
class Identification;
}  // namespace cryptohome

namespace arc {
namespace {

constexpr char kDefaultLocale[] = "en-US";

UpgradeParams DefaultUpgradeParams() {
  UpgradeParams params;
  params.locale = kDefaultLocale;
  return params;
}

// An ArcClientAdapter implementation that does the same as the real ones but
// without any D-Bus calls.
class FakeArcClientAdapter : public ArcClientAdapter {
 public:
  FakeArcClientAdapter() = default;
  ~FakeArcClientAdapter() override = default;

  FakeArcClientAdapter(const FakeArcClientAdapter&) = delete;
  FakeArcClientAdapter& operator=(const FakeArcClientAdapter&) = delete;

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    last_start_params_ = std::move(params);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FakeArcClientAdapter::OnMiniArcStarted,
                                  base::Unretained(this), std::move(callback),
                                  arc_available_));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    last_upgrade_params_ = std::move(params);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FakeArcClientAdapter::OnArcUpgraded,
                                  base::Unretained(this), std::move(callback),
                                  !force_upgrade_failure_));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeArcClientAdapter::NotifyArcInstanceStopped,
                       base::Unretained(this)));
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {}

  void SetDemoModeDelegate(DemoModeDelegate* delegate) override {}

  // Notifies ArcSessionImpl of the ARC instance stop event.
  void NotifyArcInstanceStopped() {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  void set_arc_available(bool arc_available) { arc_available_ = arc_available; }
  void set_force_upgrade_failure(bool force_upgrade_failure) {
    force_upgrade_failure_ = force_upgrade_failure;
  }
  const StartParams& last_start_params() const { return last_start_params_; }
  const UpgradeParams& last_upgrade_params() const {
    return last_upgrade_params_;
  }

 private:
  void OnMiniArcStarted(chromeos::VoidDBusMethodCallback callback,
                        bool result) {
    std::move(callback).Run(result);
  }

  void OnArcUpgraded(chromeos::VoidDBusMethodCallback callback, bool result) {
    std::move(callback).Run(result);
    if (!result)
      NotifyArcInstanceStopped();
  }

  bool arc_available_ = true;
  bool force_upgrade_failure_ = false;
  StartParams last_start_params_;
  UpgradeParams last_upgrade_params_;
};

class FakeDelegate : public ArcSessionImpl::Delegate {
 public:
  explicit FakeDelegate(int32_t lcd_density = 160)
      : lcd_density_(lcd_density) {}

  // Emulates to fail Mojo connection establishing. |callback| passed to
  // ConnectMojo will be called with nullptr.
  void EmulateMojoConnectionFailure() { success_ = false; }

  // Suspends to complete the MojoConnection, when ConnectMojo is called.
  // Later, when ResumeMojoConnection() is called, the passed callback will be
  // asynchronously called.
  void SuspendMojoConnection() { suspend_ = true; }

  // Resumes the pending Mojo connection establishment. Before,
  // SuspendMojoConnection() must be called followed by ConnectMojo().
  // ConnectMojo's |callback| will be called asynchronously.
  void ResumeMojoConnection() {
    DCHECK(!pending_callback_.is_null());
    PostCallback(std::move(pending_callback_));
  }

  // ArcSessionImpl::Delegate overrides:
  void CreateSocket(CreateSocketCallback callback) override {
    // Open /dev/null as a dummy FD.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::ScopedFD(open("/dev/null",
                                                      O_RDONLY | O_CLOEXEC))));
  }

  base::ScopedFD ConnectMojo(base::ScopedFD socket_fd,
                             ConnectMojoCallback callback) override {
    if (suspend_) {
      DCHECK(pending_callback_.is_null());
      pending_callback_ = std::move(callback);
    } else {
      PostCallback(std::move(callback));
    }

    // Open /dev/null as a dummy FD.
    return base::ScopedFD(open("/dev/null", O_RDONLY | O_CLOEXEC));
  }

  void GetLcdDensity(GetLcdDensityCallback callback) override {
    if (lcd_density_ > 0)
      std::move(callback).Run(lcd_density_);
    else
      lcd_density_callback_ = std::move(callback);
  }

  void GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) override {
    std::move(callback).Run(free_disk_space_);
  }

  version_info::Channel GetChannel() override {
    return version_info::Channel::DEFAULT;
  }

  std::unique_ptr<ArcClientAdapter> CreateClient() override {
    return std::make_unique<FakeArcClientAdapter>();
  }

  void SetLcdDensity(int32_t lcd_density) {
    lcd_density_ = lcd_density;
    ASSERT_TRUE(!lcd_density_callback_.is_null());
    std::move(lcd_density_callback_).Run(lcd_density_);
  }

  void SetFreeDiskSpace(int64_t space) { free_disk_space_ = space; }

 private:
  void PostCallback(ConnectMojoCallback callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            success_ ? std::make_unique<FakeArcBridgeHost>() : nullptr));
  }

  int32_t lcd_density_ = 0;
  bool success_ = true;
  bool suspend_ = false;
  int64_t free_disk_space_ = kMinimumFreeDiskSpaceBytes * 2;
  ConnectMojoCallback pending_callback_;
  GetLcdDensityCallback lcd_density_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelegate);
};

class TestArcSessionObserver : public ArcSession::Observer {
 public:
  struct OnSessionStoppedArgs {
    ArcStopReason reason;
    bool was_running;
    bool upgrade_requested;
  };

  explicit TestArcSessionObserver(ArcSession* arc_session)
      : arc_session_(arc_session) {
    arc_session_->AddObserver(this);
  }
  TestArcSessionObserver(ArcSession* arc_session, base::RunLoop* run_loop)
      : arc_session_(arc_session), run_loop_(run_loop) {
    arc_session_->AddObserver(this);
  }

  ~TestArcSessionObserver() override { arc_session_->RemoveObserver(this); }

  const base::Optional<OnSessionStoppedArgs>& on_session_stopped_args() const {
    return on_session_stopped_args_;
  }

  // ArcSession::Observer overrides:
  void OnSessionStopped(ArcStopReason reason,
                        bool was_running,
                        bool upgrade_requested) override {
    on_session_stopped_args_.emplace(
        OnSessionStoppedArgs{reason, was_running, upgrade_requested});
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  ArcSession* const arc_session_;            // Not owned.
  base::RunLoop* const run_loop_ = nullptr;  // Not owned.
  base::Optional<OnSessionStoppedArgs> on_session_stopped_args_;

  DISALLOW_COPY_AND_ASSIGN(TestArcSessionObserver);
};

// Custom deleter for ArcSession testing.
struct ArcSessionDeleter {
  void operator()(ArcSession* arc_session) {
    // ArcSessionImpl must be in STOPPED state, if the instance is being
    // destroyed. Calling OnShutdown() just before ensures it.
    arc_session->OnShutdown();
    delete arc_session;
  }
};

class FakeSchedulerConfigurationManager
    : public chromeos::SchedulerConfigurationManagerBase {
 public:
  FakeSchedulerConfigurationManager() = default;
  ~FakeSchedulerConfigurationManager() override = default;

  void SetLastReply(size_t num_cores_disabled) {
    reply_ = std::make_pair(true, num_cores_disabled);
    for (Observer& obs : observer_list_)
      obs.OnConfigurationSet(reply_->first, reply_->second);
  }

  base::Optional<std::pair<bool, size_t>> GetLastReply() const override {
    return reply_;
  }

 private:
  base::Optional<std::pair<bool, size_t>> reply_;

  DISALLOW_COPY_AND_ASSIGN(FakeSchedulerConfigurationManager);
};

class FakeAdbSideloadingAvailabilityDelegate
    : public AdbSideloadingAvailabilityDelegate {
 public:
  FakeAdbSideloadingAvailabilityDelegate() = default;
  ~FakeAdbSideloadingAvailabilityDelegate() override = default;

  void CanChangeAdbSideloading(
      base::OnceCallback<void(bool can_change_adb_sideloading)> callback)
      override {
    std::move(callback).Run(can_change_adb_sideloading_);
  }

  void SetCanChangeAdbSideloading(bool can_change) {
    can_change_adb_sideloading_ = can_change;
  }

 private:
  bool can_change_adb_sideloading_ = false;
};

class ArcSessionImplTest : public testing::Test {
 public:
  ArcSessionImplTest() = default;
  ~ArcSessionImplTest() override = default;

  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> CreateArcSession(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate = nullptr,
      int32_t lcd_density = 160) {
    auto arc_session =
        CreateArcSessionInternal(std::move(delegate), lcd_density);
    fake_schedule_configuration_manager_.SetLastReply(0);
    return arc_session;
  }

  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter>
  CreateArcSessionWithoutCpuInfo(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate = nullptr,
      int32_t lcd_density = 160) {
    return CreateArcSessionInternal(std::move(delegate), lcd_density);
  }

  void SetupMiniContainer(ArcSessionImpl* arc_session,
                          TestArcSessionObserver* observer) {
    arc_session->StartMiniInstance();
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
              arc_session->GetStateForTesting());
    ASSERT_FALSE(observer->on_session_stopped_args().has_value());
  }

 protected:
  FakeArcClientAdapter* GetClient(ArcSessionImpl* session) {
    return static_cast<FakeArcClientAdapter*>(session->GetClientForTesting());
  }

  FakeSchedulerConfigurationManager fake_schedule_configuration_manager_;

  std::unique_ptr<FakeAdbSideloadingAvailabilityDelegate>
      adb_sideloading_availability_delegate_ =
          std::make_unique<FakeAdbSideloadingAvailabilityDelegate>();

 private:
  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> CreateArcSessionInternal(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate,
      int32_t lcd_density) {
    if (!delegate)
      delegate = std::make_unique<FakeDelegate>(lcd_density);
    return std::unique_ptr<ArcSessionImpl, ArcSessionDeleter>(
        new ArcSessionImpl(std::move(delegate),
                           &fake_schedule_configuration_manager_,
                           adb_sideloading_availability_delegate_.get()));
  }

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionImplTest);
};

// Starting mini container success case.
TEST_F(ArcSessionImplTest, MiniInstance_Success) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// ArcClientAdapter::StartMiniArc() reports an error, causing the mini instance
// start to fail.
TEST_F(ArcSessionImplTest, MiniInstance_DBusFail) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  GetClient(arc_session.get())->set_arc_available(false);
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// ArcClientAdapter::UpgradeArc() reports an error due to low disk,
// causing the container upgrade to fail to start container with reason
// LOW_DISK_SPACE.
TEST_F(ArcSessionImplTest, Upgrade_LowDisk) {
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->SetFreeDiskSpace(kMinimumFreeDiskSpaceBytes / 2);

  // Set up. Start mini-container. The mini-container doesn't use the disk, so
  // there being low disk space won't cause it to start.
  auto arc_session = CreateArcSession(std::move(delegate));

  base::RunLoop run_loop;
  TestArcSessionObserver observer(arc_session.get(), &run_loop);
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  arc_session->RequestUpgrade(DefaultUpgradeParams());
  run_loop.Run();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::LOW_DISK_SPACE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Upgrading a mini container to a full container. Success case.
TEST_F(ArcSessionImplTest, Upgrade_Success) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then, upgrade to a full instance.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// ArcClientAdapter::UpgradeArc() reports an error, then the upgrade fails.
TEST_F(ArcSessionImplTest, Upgrade_DBusFail) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Hereafter, let ArcClientAdapter::UpgradeArc() fail.
  GetClient(arc_session.get())->set_force_upgrade_failure(true);

  // Then upgrade, which should fail.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Mojo connection fails on upgrading. Then, the upgrade fails.
TEST_F(ArcSessionImplTest, Upgrade_MojoConnectionFail) {
  // Let Mojo connection fail.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->EmulateMojoConnectionFailure();

  // Set up. Start mini instance.
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  // Starting mini instance should succeed, because it is not related to
  // Mojo connection.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Upgrade should fail, due to Mojo connection fail set above.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Calling UpgradeArcContainer() during STARTING_MINI_INSTANCE should eventually
// succeed to run a full container.
TEST_F(ArcSessionImplTest, Upgrade_StartingMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Before moving forward to RUNNING_MINI_INSTANCE, start upgrading it.
  arc_session->RequestUpgrade(DefaultUpgradeParams());

  // The state should not immediately switch to STARTING_FULL_INSTANCE, yet.
  EXPECT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Complete the upgrade procedure.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_FALSE(observer.on_session_stopped_args().has_value());
}

// Testing stop during START_MINI_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_StartingMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during RUNNING_MINI_INSTANCE.
TEST_F(ArcSessionImplTest, Stop_RunningMiniInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during STARTING_FULL_INSTANCE for upgrade.
TEST_F(ArcSessionImplTest, Stop_StartingFullInstanceForUpgrade) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then upgrade.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  ASSERT_EQ(ArcSessionImpl::State::STARTING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to stop during STARTING_FULL_INSTANCE state.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during CONNECTING_MOJO for upgrade.
TEST_F(ArcSessionImplTest, Stop_ConnectingMojoForUpgrade) {
  // Let Mojo connection suspend.
  auto delegate = std::make_unique<FakeDelegate>();
  delegate->SuspendMojoConnection();
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSession(std::move(delegate));
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Then upgrade. This should suspend at Mojo connection.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::CONNECTING_MOJO,
            arc_session->GetStateForTesting());

  // Request to stop, then resume the Mojo connection.
  arc_session->Stop();
  delegate_ptr->ResumeMojoConnection();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during RUNNING_FULL_INSTANCE after upgrade.
TEST_F(ArcSessionImplTest, Stop_RunningFullInstanceForUpgrade) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  // Start mini container.
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // And upgrade successfully.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Then request to stop.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Testing stop during STARTING_MINI_INSTANCE with upgrade request.
TEST_F(ArcSessionImplTest,
       Stop_StartingFullInstanceForUpgradeDuringMiniInstanceStart) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to upgrade during starting mini container.
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  // Then, the state should stay at STARTING_MINI_INSTANCE.
  ASSERT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());

  // Request to stop.
  arc_session->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::SHUTDOWN,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

// Emulating crash.
TEST_F(ArcSessionImplTest, ArcStopInstance) {
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionImpl::State::RUNNING_FULL_INSTANCE,
            arc_session->GetStateForTesting());

  // Notify ArcClientAdapter's observers of the crash event.
  GetClient(arc_session.get())->NotifyArcInstanceStopped();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::CRASH, observer.on_session_stopped_args()->reason);
  EXPECT_TRUE(observer.on_session_stopped_args()->was_running);
  EXPECT_TRUE(observer.on_session_stopped_args()->upgrade_requested);
}

struct PackagesCacheModeState {
  // Possible values for chromeos::switches::kArcPackagesCacheMode
  const char* chrome_switch;
  bool full_container;
  UpgradeParams::PackageCacheMode expected_packages_cache_mode;
};

constexpr PackagesCacheModeState kPackagesCacheModeStates[] = {
    {nullptr, true, UpgradeParams::PackageCacheMode::DEFAULT},
    {nullptr, false, UpgradeParams::PackageCacheMode::DEFAULT},
    {kPackagesCacheModeCopy, true,
     UpgradeParams::PackageCacheMode::COPY_ON_INIT},
    {kPackagesCacheModeCopy, false, UpgradeParams::PackageCacheMode::DEFAULT},
    {kPackagesCacheModeSkipCopy, true,
     UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT},
    {kPackagesCacheModeCopy, false, UpgradeParams::PackageCacheMode::DEFAULT},
};

class ArcSessionImplPackagesCacheModeTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<PackagesCacheModeState> {};

TEST_P(ArcSessionImplPackagesCacheModeTest, PackagesCacheModes) {
  auto arc_session = CreateArcSession();

  const PackagesCacheModeState& state = GetParam();
  if (state.chrome_switch) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(chromeos::switches::kArcPackagesCacheMode,
                                    state.chrome_switch);
  }

  arc_session->StartMiniInstance();
  if (state.full_container)
    arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      state.expected_packages_cache_mode,
      GetClient(arc_session.get())->last_upgrade_params().packages_cache_mode);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplPackagesCacheModeTest,
                         ::testing::ValuesIn(kPackagesCacheModeStates));

class ArcSessionImplGmsCoreCacheTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(ArcSessionImplGmsCoreCacheTest, GmsCoreCaches) {
  auto arc_session = CreateArcSession();

  if (GetParam()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kArcDisableGmsCoreCache);
  }

  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      GetParam(),
      GetClient(arc_session.get())->last_upgrade_params().skip_gms_core_cache);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplGmsCoreCacheTest,
                         ::testing::Bool());

TEST_F(ArcSessionImplTest, DemoSession) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  const base::FilePath demo_apps_path(
      "/run/imageloader/demo_mode_resources/android_apps.squash");
  UpgradeParams params;
  params.is_demo_session = true;
  params.demo_session_apps_path = base::FilePath(demo_apps_path);
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      GetClient(arc_session.get())->last_upgrade_params().is_demo_session);
  EXPECT_EQ(demo_apps_path, GetClient(arc_session.get())
                                ->last_upgrade_params()
                                .demo_session_apps_path);
}

TEST_F(ArcSessionImplTest, DemoSessionWithoutOfflineDemoApps) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  UpgradeParams params;
  params.is_demo_session = true;
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      GetClient(arc_session.get())->last_upgrade_params().is_demo_session);
  EXPECT_EQ(base::FilePath(), GetClient(arc_session.get())
                                  ->last_upgrade_params()
                                  .demo_session_apps_path);
}

TEST_F(ArcSessionImplTest, SupervisionTransitionShouldGraduate) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  UpgradeParams params;
  params.supervision_transition = ArcSupervisionTransition::CHILD_TO_REGULAR;
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSupervisionTransition::CHILD_TO_REGULAR,
            GetClient(arc_session.get())
                ->last_upgrade_params()
                .supervision_transition);
  EXPECT_EQ(160, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StartArcMiniContainerWithDensity) {
  auto arc_session = CreateArcSessionWithoutCpuInfo(nullptr, 240);
  arc_session->StartMiniInstance();
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  fake_schedule_configuration_manager_.SetLastReply(2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::RUNNING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  EXPECT_EQ(240, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StartArcMiniContainerWithDensityAsync) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_LCD_DENSITY,
            arc_session->GetStateForTesting());
  delegate_ptr->SetLcdDensity(240);
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  fake_schedule_configuration_manager_.SetLastReply(2);
  EXPECT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(240, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StartArcMiniContainerWithDensityAsyncReversedOrder) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  // This time, set the CPU cores information first.
  fake_schedule_configuration_manager_.SetLastReply(2);
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_LCD_DENSITY,
            arc_session->GetStateForTesting());
  delegate_ptr->SetLcdDensity(240);
  EXPECT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(240, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StartArcMiniContainerWithDensityAsyncCpuInfoEarly) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  // Set the CPU cores information even before StartMiniInstance() request.
  fake_schedule_configuration_manager_.SetLastReply(2);
  arc_session->StartMiniInstance();
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_LCD_DENSITY,
            arc_session->GetStateForTesting());
  delegate_ptr->SetLcdDensity(240);
  EXPECT_EQ(ArcSessionImpl::State::STARTING_MINI_INSTANCE,
            arc_session->GetStateForTesting());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(240, GetClient(arc_session.get())->last_start_params().lcd_density);
}

TEST_F(ArcSessionImplTest, StopWhileWaitingForLcdDensity) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  fake_schedule_configuration_manager_.SetLastReply(2);
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_LCD_DENSITY,
            arc_session->GetStateForTesting());
  arc_session->Stop();
  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
}

TEST_F(ArcSessionImplTest, ShutdownWhileWaitingForLcdDensity) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  fake_schedule_configuration_manager_.SetLastReply(2);
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_LCD_DENSITY,
            arc_session->GetStateForTesting());
  arc_session->OnShutdown();
  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
}

TEST_F(ArcSessionImplTest, StopWhileWaitingForNumCores) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  delegate_ptr->SetLcdDensity(240);
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  arc_session->Stop();
  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
}

TEST_F(ArcSessionImplTest, ShutdownWhileWaitingForNumCores) {
  auto delegate = std::make_unique<FakeDelegate>(0);
  auto* delegate_ptr = delegate.get();
  auto arc_session = CreateArcSessionWithoutCpuInfo(std::move(delegate));
  arc_session->StartMiniInstance();
  delegate_ptr->SetLcdDensity(240);
  EXPECT_EQ(ArcSessionImpl::State::WAITING_FOR_NUM_CORES,
            arc_session->GetStateForTesting());
  arc_session->OnShutdown();
  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
}

// Test that correct value false for managed sideloading is passed
TEST_F(ArcSessionImplTest, CanChangeAdbSideloading_False) {
  auto arc_session = CreateArcSession();
  adb_sideloading_availability_delegate_->SetCanChangeAdbSideloading(false);

  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetClient(arc_session.get())
                   ->last_upgrade_params()
                   .is_managed_adb_sideloading_allowed);
}

// Test that correct value true for managed sideloading is passed
TEST_F(ArcSessionImplTest, CanChangeAdbSideloading_True) {
  auto arc_session = CreateArcSession();
  adb_sideloading_availability_delegate_->SetCanChangeAdbSideloading(true);

  arc_session->StartMiniInstance();
  arc_session->RequestUpgrade(DefaultUpgradeParams());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetClient(arc_session.get())
                  ->last_upgrade_params()
                  .is_managed_adb_sideloading_allowed);
}

struct DalvikMemoryProfileVariant {
  // Memory stat file
  const char* file_name;
  const StartParams::DalvikMemoryProfile expected_profile;
};

constexpr DalvikMemoryProfileVariant kDalvikMemoryProfileVariant[] = {
    {"non-existing", StartParams::DalvikMemoryProfile::DEFAULT},
    {"2G", StartParams::DalvikMemoryProfile::DEFAULT},
    {"4G", StartParams::DalvikMemoryProfile::M4G},
    {"8G", StartParams::DalvikMemoryProfile::M8G},
    {"16G", StartParams::DalvikMemoryProfile::M16G},
};

class ArcSessionImplDalvikMemoryProfileTest
    : public ArcSessionImplTest,
      public ::testing::WithParamInterface<DalvikMemoryProfileVariant> {};

bool GetSystemMemoryInfo(const std::string& file_name,
                         base::SystemMemoryInfoKB* mem_info) {
  base::FilePath base_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path);
  const base::FilePath test_path = base_path.Append("components")
                                       .Append("test")
                                       .Append("data")
                                       .Append("arc_dalvik_profile")
                                       .Append(file_name);
  base::ScopedAllowBlockingForTesting allowBlocking;
  std::string mem_info_data;
  return base::ReadFileToString(test_path, &mem_info_data) &&
         base::ParseProcMeminfo(mem_info_data, mem_info);
}

TEST_P(ArcSessionImplDalvikMemoryProfileTest, DalvikMemoryProfiles) {
  const DalvikMemoryProfileVariant& variant = GetParam();

  auto arc_session = CreateArcSession();
  arc_session->SetSystemMemoryInfoCallbackForTesting(
      base::BindRepeating(&GetSystemMemoryInfo, variant.file_name));

  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      variant.expected_profile,
      GetClient(arc_session.get())->last_start_params().dalvik_memory_profile);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionImplDalvikMemoryProfileTest,
                         ::testing::ValuesIn(kDalvikMemoryProfileVariant));

}  // namespace
}  // namespace arc
