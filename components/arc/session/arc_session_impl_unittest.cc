// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/system/scheduler_configuration_manager_base.h"
#include "components/account_id/account_id.h"
#include "components/arc/session/arc_session_impl.h"
#include "components/arc/test/fake_arc_bridge_host.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kFakeGmail[] = "user@gmail.com";
constexpr char kFakeGmailGaiaId[] = "1234567890";
constexpr char kDefaultLocale[] = "en-US";

UpgradeParams DefaultUpgradeParams() {
  UpgradeParams params;
  params.locale = kDefaultLocale;
  return params;
}

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

class ArcSessionImplTest : public testing::Test {
 public:
  ArcSessionImplTest() {
    // Create a user and set it as the primary user.
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kFakeGmail, kFakeGmailGaiaId);
    const user_manager::User* user = GetUserManager()->AddUser(account_id);
    GetUserManager()->UserLoggedIn(account_id, user->username_hash(),
                                   false /* browser_restart */,
                                   false /* is_child */);
  }

  ~ArcSessionImplTest() override {
    GetUserManager()->RemoveUserFromList(
        AccountId::FromUserEmailGaiaId(kFakeGmail, kFakeGmailGaiaId));
  }

  void SetUp() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::FakeSessionManagerClient::Get()->set_arc_available(true);
  }

  void TearDown() override { chromeos::SessionManagerClient::Shutdown(); }

  user_manager::FakeUserManager* GetUserManager() {
    return static_cast<user_manager::FakeUserManager*>(
        user_manager::UserManager::Get());
  }

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
  FakeSchedulerConfigurationManager fake_schedule_configuration_manager_;

 private:
  std::unique_ptr<ArcSessionImpl, ArcSessionDeleter> CreateArcSessionInternal(
      std::unique_ptr<ArcSessionImpl::Delegate> delegate,
      int32_t lcd_density) {
    if (!delegate)
      delegate = std::make_unique<FakeDelegate>(lcd_density);
    return std::unique_ptr<ArcSessionImpl, ArcSessionDeleter>(
        new ArcSessionImpl(std::move(delegate),
                           &fake_schedule_configuration_manager_));
  }

  base::test::TaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};

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

// SessionManagerClient::StartArcMiniContainer() reports an error, causing the
// mini-container start to fail.
TEST_F(ArcSessionImplTest, MiniInstance_DBusFail) {
  chromeos::FakeSessionManagerClient::Get()->set_arc_available(false);

  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  arc_session->StartMiniInstance();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionImpl::State::STOPPED, arc_session->GetStateForTesting());
  ASSERT_TRUE(observer.on_session_stopped_args().has_value());
  EXPECT_EQ(ArcStopReason::GENERIC_BOOT_FAILURE,
            observer.on_session_stopped_args()->reason);
  EXPECT_FALSE(observer.on_session_stopped_args()->was_running);
  EXPECT_FALSE(observer.on_session_stopped_args()->upgrade_requested);
}

// SessionManagerClient::UpgradeArcContainer() reports an error due to low disk,
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

// SessionManagerClient::UpgradeArcContainer() reports an error, then the
// upgrade fails.
TEST_F(ArcSessionImplTest, Upgrade_DBusFail) {
  // Set up. Start a mini instance.
  auto arc_session = CreateArcSession();
  TestArcSessionObserver observer(arc_session.get());
  ASSERT_NO_FATAL_FAILURE(SetupMiniContainer(arc_session.get(), &observer));

  // Hereafter, let SessionManagerClient::UpgradeArcContainer() fail.
  chromeos::FakeSessionManagerClient::Get()->set_force_upgrade_failure(true);

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

  // Deliver the ArcInstanceStopped D-Bus signal.
  chromeos::FakeSessionManagerClient::Get()->NotifyArcInstanceStopped();

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
  login_manager::UpgradeArcContainerRequest_PackageCacheMode
      expected_packages_cache_mode;
};

constexpr PackagesCacheModeState kPackagesCacheModeStates[] = {
    {nullptr, true,
     login_manager::UpgradeArcContainerRequest_PackageCacheMode_DEFAULT},
    {nullptr, false,
     login_manager::UpgradeArcContainerRequest_PackageCacheMode_DEFAULT},
    {kPackagesCacheModeCopy, true,
     login_manager::UpgradeArcContainerRequest_PackageCacheMode_COPY_ON_INIT},
    {kPackagesCacheModeCopy, false,
     login_manager::UpgradeArcContainerRequest_PackageCacheMode_DEFAULT},
    {kPackagesCacheModeSkipCopy, true,
     login_manager::
         UpgradeArcContainerRequest_PackageCacheMode_SKIP_SETUP_COPY_ON_INIT},
    {kPackagesCacheModeCopy, false,
     login_manager::UpgradeArcContainerRequest_PackageCacheMode_DEFAULT},
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
  EXPECT_EQ(state.expected_packages_cache_mode,
            chromeos::FakeSessionManagerClient::Get()
                ->last_upgrade_arc_request()
                .packages_cache_mode());
}

INSTANTIATE_TEST_SUITE_P(,
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
  EXPECT_EQ(GetParam(), chromeos::FakeSessionManagerClient::Get()
                            ->last_upgrade_arc_request()
                            .skip_gms_core_cache());
}

INSTANTIATE_TEST_SUITE_P(, ArcSessionImplGmsCoreCacheTest, ::testing::Bool());

TEST_F(ArcSessionImplTest, DemoSession) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  const std::string demo_apps_path =
      "/run/imageloader/demo_mode_resources/android_apps.squash";
  UpgradeParams params;
  params.is_demo_session = true;
  params.demo_session_apps_path = base::FilePath(demo_apps_path);
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->last_upgrade_arc_request()
                  .is_demo_session());
  EXPECT_EQ(demo_apps_path, chromeos::FakeSessionManagerClient::Get()
                                ->last_upgrade_arc_request()
                                .demo_session_apps_path());
}

TEST_F(ArcSessionImplTest, DemoSessionWithoutOfflineDemoApps) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  UpgradeParams params;
  params.is_demo_session = true;
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(chromeos::FakeSessionManagerClient::Get()
                  ->last_upgrade_arc_request()
                  .is_demo_session());
  EXPECT_EQ(std::string(), chromeos::FakeSessionManagerClient::Get()
                               ->last_upgrade_arc_request()
                               .demo_session_apps_path());
}

TEST_F(ArcSessionImplTest, SupervisionTransitionShouldGraduate) {
  auto arc_session = CreateArcSession();
  arc_session->StartMiniInstance();

  UpgradeParams params;
  params.supervision_transition = ArcSupervisionTransition::CHILD_TO_REGULAR;
  params.locale = kDefaultLocale;
  arc_session->RequestUpgrade(std::move(params));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      login_manager::
          UpgradeArcContainerRequest_SupervisionTransition_CHILD_TO_REGULAR,
      chromeos::FakeSessionManagerClient::Get()
          ->last_upgrade_arc_request()
          .supervision_transition());
  EXPECT_EQ(160, chromeos::FakeSessionManagerClient::Get()
                     ->last_start_arc_mini_container_request()
                     .lcd_density());
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
  EXPECT_EQ(240, chromeos::FakeSessionManagerClient::Get()
                     ->last_start_arc_mini_container_request()
                     .lcd_density());
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

  EXPECT_EQ(240, chromeos::FakeSessionManagerClient::Get()
                     ->last_start_arc_mini_container_request()
                     .lcd_density());
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

  EXPECT_EQ(240, chromeos::FakeSessionManagerClient::Get()
                     ->last_start_arc_mini_container_request()
                     .lcd_density());
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

  EXPECT_EQ(240, chromeos::FakeSessionManagerClient::Get()
                     ->last_start_arc_mini_container_request()
                     .lcd_density());
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

}  // namespace
}  // namespace arc
