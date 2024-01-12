// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_component_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/tether/asynchronous_shutdown_object_container_impl.h"
#include "chromeos/ash/components/tether/crash_recovery_manager_impl.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_asynchronous_shutdown_object_container.h"
#include "chromeos/ash/components/tether/fake_crash_recovery_manager.h"
#include "chromeos/ash/components/tether/fake_host_scan_scheduler.h"
#include "chromeos/ash/components/tether/fake_synchronous_shutdown_object_container.h"
#include "chromeos/ash/components/tether/fake_tether_disconnector.h"
#include "chromeos/ash/components/tether/synchronous_shutdown_object_container_impl.h"
#include "chromeos/ash/components/tether/tether_session_completion_logger.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

class TestTetherComponentObserver : public TetherComponent::Observer {
 public:
  TestTetherComponentObserver() = default;
  ~TestTetherComponentObserver() override = default;

  bool shutdown_complete() { return shutdown_complete_; }

  // TetherComponent::Observer:
  void OnShutdownComplete() override { shutdown_complete_ = true; }

 private:
  bool shutdown_complete_ = false;
};

class FakeAsynchronousShutdownObjectContainerFactory
    : public AsynchronousShutdownObjectContainerImpl::Factory {
 public:
  explicit FakeAsynchronousShutdownObjectContainerFactory(
      FakeAsynchronousShutdownObjectContainer* fake_asynchronous_container)
      : fake_asynchronous_container_(fake_asynchronous_container) {}

  ~FakeAsynchronousShutdownObjectContainerFactory() override = default;

  // AsynchronousShutdownObjectContainerImpl::Factory:
  std::unique_ptr<AsynchronousShutdownObjectContainer> CreateInstance(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostFetcher* tether_host_fetcher,
      NetworkStateHandler* network_state_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkConnectionHandler* network_connection_handler,
      PrefService* pref_service) override {
    return base::WrapUnique(fake_asynchronous_container_.get());
  }

 private:
  raw_ptr<FakeAsynchronousShutdownObjectContainer, DanglingUntriaged>
      fake_asynchronous_container_;
};

class FakeSynchronousShutdownObjectContainerFactory
    : public SynchronousShutdownObjectContainerImpl::Factory {
 public:
  explicit FakeSynchronousShutdownObjectContainerFactory(
      FakeSynchronousShutdownObjectContainer* fake_synchronous_container)
      : fake_synchronous_container_(fake_synchronous_container) {}

  ~FakeSynchronousShutdownObjectContainerFactory() override = default;

  // SynchronousShutdownObjectContainerImpl::Factory:
  std::unique_ptr<SynchronousShutdownObjectContainer> CreateInstance(
      AsynchronousShutdownObjectContainer* asychronous_container,
      NotificationPresenter* notification_presenter,
      GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      PrefService* pref_service,
      NetworkHandler* network_handler,
      NetworkConnect* network_connect,
      session_manager::SessionManager* session_manager,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client) override {
    return base::WrapUnique(fake_synchronous_container_.get());
  }

 private:
  raw_ptr<FakeSynchronousShutdownObjectContainer, DanglingUntriaged>
      fake_synchronous_container_;
};

class FakeCrashRecoveryManagerFactory
    : public CrashRecoveryManagerImpl::Factory {
 public:
  explicit FakeCrashRecoveryManagerFactory(
      FakeCrashRecoveryManager* fake_crash_recovery_manager)
      : fake_crash_recovery_manager_(fake_crash_recovery_manager) {}

  ~FakeCrashRecoveryManagerFactory() override = default;

  // CrashRecoveryManagerImpl::Factory:
  std::unique_ptr<CrashRecoveryManager> CreateInstance(
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      HostScanCache* host_scan_cache) override {
    return base::WrapUnique(fake_crash_recovery_manager_.get());
  }

 private:
  raw_ptr<FakeCrashRecoveryManager, DanglingUntriaged>
      fake_crash_recovery_manager_;
};

}  // namespace

class TetherComponentImplTest : public testing::Test {
 public:
  TetherComponentImplTest(const TetherComponentImplTest&) = delete;
  TetherComponentImplTest& operator=(const TetherComponentImplTest&) = delete;

 protected:
  TetherComponentImplTest() = default;
  ~TetherComponentImplTest() override = default;

  void SetUp() override {
    was_synchronous_container_deleted_ = false;
    was_asynchronous_container_deleted_ = false;

    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_host_scan_scheduler_ = std::make_unique<FakeHostScanScheduler>();
    fake_tether_disconnector_ = std::make_unique<FakeTetherDisconnector>();

    fake_synchronous_container_ = new FakeSynchronousShutdownObjectContainer(
        base::BindOnce(&TetherComponentImplTest::OnSynchronousContainerDeleted,
                       base::Unretained(this)));
    fake_synchronous_container_->set_active_host(fake_active_host_.get());
    fake_synchronous_container_->set_host_scan_scheduler(
        fake_host_scan_scheduler_.get());
    fake_synchronous_container_->set_tether_disconnector(
        fake_tether_disconnector_.get());
    fake_synchronous_container_factory_ =
        base::WrapUnique(new FakeSynchronousShutdownObjectContainerFactory(
            fake_synchronous_container_));
    SynchronousShutdownObjectContainerImpl::Factory::SetFactoryForTesting(
        fake_synchronous_container_factory_.get());

    fake_asynchronous_container_ = new FakeAsynchronousShutdownObjectContainer(
        base::BindOnce(&TetherComponentImplTest::OnAsynchronousContainerDeleted,
                       base::Unretained(this)));
    fake_asynchronous_container_factory_ =
        base::WrapUnique(new FakeAsynchronousShutdownObjectContainerFactory(
            fake_asynchronous_container_));
    AsynchronousShutdownObjectContainerImpl::Factory::SetFactoryForTesting(
        fake_asynchronous_container_factory_.get());

    fake_crash_recovery_manager_ = new FakeCrashRecoveryManager();
    fake_crash_recovery_manager_factory_ = base::WrapUnique(
        new FakeCrashRecoveryManagerFactory(fake_crash_recovery_manager_));
    CrashRecoveryManagerImpl::Factory::SetFactoryForTesting(
        fake_crash_recovery_manager_factory_.get());

    component_ = TetherComponentImpl::Factory::Create(
        nullptr /* device_sync_client */, nullptr /* secure_channel_client */,
        nullptr /* tether_host_fetcher */, nullptr /* notification_presenter */,
        nullptr /* gms_core_notifications_state_trackerstate_ */,
        nullptr /* pref_service */, NetworkHandler::Get() /* network_handler */,
        nullptr /* network_connect */, nullptr /* adapter */,
        nullptr /* session_manager */);

    test_observer_ = std::make_unique<TestTetherComponentObserver>();
    component_->AddObserver(test_observer_.get());
  }

  void InvokeCrashRecoveryCallback() {
    base::OnceClosure on_restoration_finished_callback =
        fake_crash_recovery_manager_->TakeOnRestorationFinishedCallback();
    EXPECT_FALSE(on_restoration_finished_callback.is_null());
    std::move(on_restoration_finished_callback).Run();
  }

  void InvokeAsynchronousShutdownCallback() {
    base::OnceClosure shutdown_complete_callback =
        fake_asynchronous_container_->TakeShutdownCompleteCallback();
    EXPECT_FALSE(shutdown_complete_callback.is_null());
    std::move(shutdown_complete_callback).Run();
  }

  void OnSynchronousContainerDeleted() {
    was_synchronous_container_deleted_ = true;
  }

  void OnAsynchronousContainerDeleted() {
    was_asynchronous_container_deleted_ = true;
  }

  bool was_synchronous_container_deleted_;
  bool was_asynchronous_container_deleted_;

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeHostScanScheduler> fake_host_scan_scheduler_;
  std::unique_ptr<FakeTetherDisconnector> fake_tether_disconnector_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkHandlerTestHelper helper_;

  raw_ptr<FakeSynchronousShutdownObjectContainer, DanglingUntriaged>
      fake_synchronous_container_;
  std::unique_ptr<FakeSynchronousShutdownObjectContainerFactory>
      fake_synchronous_container_factory_;

  raw_ptr<FakeAsynchronousShutdownObjectContainer, DanglingUntriaged>
      fake_asynchronous_container_;
  std::unique_ptr<FakeAsynchronousShutdownObjectContainerFactory>
      fake_asynchronous_container_factory_;

  raw_ptr<FakeCrashRecoveryManager, DanglingUntriaged>
      fake_crash_recovery_manager_;
  std::unique_ptr<FakeCrashRecoveryManagerFactory>
      fake_crash_recovery_manager_factory_;

  std::unique_ptr<TetherComponent> component_;

  std::unique_ptr<TestTetherComponentObserver> test_observer_;
};

TEST_F(TetherComponentImplTest, TestShutdown_Disconnected) {
  InvokeCrashRecoveryCallback();
  EXPECT_FALSE(test_observer_->shutdown_complete());

  component_->RequestShutdown(TetherComponent::ShutdownReason::USER_CLOSED_LID);
  EXPECT_TRUE(was_synchronous_container_deleted_);
  EXPECT_FALSE(was_asynchronous_container_deleted_);
  EXPECT_FALSE(test_observer_->shutdown_complete());

  // No disconnection attempt should have occurred since the active host was
  // disconnected.
  EXPECT_TRUE(fake_tether_disconnector_->last_disconnected_tether_network_guid()
                  .empty());

  InvokeAsynchronousShutdownCallback();
  EXPECT_TRUE(was_asynchronous_container_deleted_);
  EXPECT_TRUE(test_observer_->shutdown_complete());
}

TEST_F(TetherComponentImplTest, TestShutdown_Connecting) {
  InvokeCrashRecoveryCallback();
  EXPECT_FALSE(test_observer_->shutdown_complete());

  fake_active_host_->SetActiveHostConnecting("deviceId", "tetherNetworkGuid");
  component_->RequestShutdown(TetherComponent::ShutdownReason::USER_CLOSED_LID);
  EXPECT_TRUE(was_synchronous_container_deleted_);
  EXPECT_FALSE(was_asynchronous_container_deleted_);
  EXPECT_FALSE(test_observer_->shutdown_complete());

  // A disconnection attempt should have occurred.
  EXPECT_EQ("tetherNetworkGuid",
            fake_tether_disconnector_->last_disconnected_tether_network_guid());
  EXPECT_EQ(
      TetherSessionCompletionLogger::SessionCompletionReason::USER_CLOSED_LID,
      *fake_tether_disconnector_->last_session_completion_reason());

  InvokeAsynchronousShutdownCallback();
  EXPECT_TRUE(was_asynchronous_container_deleted_);
  EXPECT_TRUE(test_observer_->shutdown_complete());
}

TEST_F(TetherComponentImplTest, TestShutdown_Connected) {
  InvokeCrashRecoveryCallback();
  EXPECT_FALSE(test_observer_->shutdown_complete());

  fake_active_host_->SetActiveHostConnected("deviceId", "tetherNetworkGuid",
                                            "wifiNetworkGuid");
  component_->RequestShutdown(TetherComponent::ShutdownReason::USER_CLOSED_LID);
  EXPECT_TRUE(was_synchronous_container_deleted_);
  EXPECT_FALSE(was_asynchronous_container_deleted_);
  EXPECT_FALSE(test_observer_->shutdown_complete());

  // A disconnection attempt should have occurred.
  EXPECT_EQ("tetherNetworkGuid",
            fake_tether_disconnector_->last_disconnected_tether_network_guid());
  EXPECT_EQ(
      TetherSessionCompletionLogger::SessionCompletionReason::USER_CLOSED_LID,
      *fake_tether_disconnector_->last_session_completion_reason());

  InvokeAsynchronousShutdownCallback();
  EXPECT_TRUE(was_asynchronous_container_deleted_);
  EXPECT_TRUE(test_observer_->shutdown_complete());
}

TEST_F(TetherComponentImplTest, TestShutdown_BeforeCrashRecoveryComplete) {
  component_->RequestShutdown(TetherComponent::ShutdownReason::USER_CLOSED_LID);
  EXPECT_FALSE(test_observer_->shutdown_complete());

  // A shutdown attempt should not have occurred since crash recovery has
  // not completed.
  EXPECT_FALSE(was_synchronous_container_deleted_);
  EXPECT_FALSE(was_asynchronous_container_deleted_);
  EXPECT_TRUE(
      fake_asynchronous_container_->TakeShutdownCompleteCallback().is_null());

  InvokeCrashRecoveryCallback();
  EXPECT_TRUE(was_synchronous_container_deleted_);
  EXPECT_FALSE(test_observer_->shutdown_complete());

  // Now that crash recovery is complete, a shutdown attempt should have been
  // started.
  InvokeAsynchronousShutdownCallback();
  EXPECT_TRUE(was_asynchronous_container_deleted_);
  EXPECT_TRUE(test_observer_->shutdown_complete());
}

}  // namespace tether

}  // namespace ash
