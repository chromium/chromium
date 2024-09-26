// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/asynchronous_shutdown_object_container_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/ash/components/tether/tether_component_impl.h"
#include "chromeos/ash/services/device_sync/fake_remote_device_provider.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/device_sync/remote_device_provider_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace ash {

namespace tether {

namespace {

class FakeRemoteDeviceProviderFactory
    : public device_sync::RemoteDeviceProviderImpl::Factory {
 public:
  FakeRemoteDeviceProviderFactory() = default;
  ~FakeRemoteDeviceProviderFactory() override = default;

  // device_sync::RemoteDeviceProviderImpl::Factory:
  std::unique_ptr<device_sync::RemoteDeviceProvider> CreateInstance(
      device_sync::CryptAuthV2DeviceManager* v2_device_manager,
      const std::string& user_email,
      const std::string& user_private_key) override {
    return std::make_unique<device_sync::FakeRemoteDeviceProvider>();
  }
};

}  // namespace

class AsynchronousShutdownObjectContainerImplTest : public testing::Test {
 public:
  AsynchronousShutdownObjectContainerImplTest(
      const AsynchronousShutdownObjectContainerImplTest&) = delete;
  AsynchronousShutdownObjectContainerImplTest& operator=(
      const AsynchronousShutdownObjectContainerImplTest&) = delete;

 protected:
  AsynchronousShutdownObjectContainerImplTest()
      : test_device_(multidevice::CreateRemoteDeviceRefListForTest(1u)[0]) {}

  void SetUp() override {
    was_shutdown_callback_invoked_ = false;

    fake_remote_device_provider_factory_ =
        base::WrapUnique(new FakeRemoteDeviceProviderFactory());
    device_sync::RemoteDeviceProviderImpl::Factory::SetFactoryForTesting(
        fake_remote_device_provider_factory_.get());

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_tether_host_fetcher_ =
        std::make_unique<FakeTetherHostFetcher>(/*tether_host=*/std::nullopt);

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    TetherComponentImpl::RegisterProfilePrefs(test_pref_service_->registry());

    // Note: The null pointers passed to the constructor are not actually used
    // by the object itself; rather, they are simply passed to the constructors
    // of objects created by the container.
    container_ = base::WrapUnique(new AsynchronousShutdownObjectContainerImpl(
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        fake_tether_host_fetcher_.get() /* tether_host_fetcher */,
        nullptr /* network_state_handler */,
        nullptr /* managed_network_configuration_handler */,
        nullptr /* network_connection_handler */,
        test_pref_service_.get() /* pref_service */));

    fake_disconnect_tethering_request_sender_ =
        new FakeDisconnectTetheringRequestSender();

    container_->SetTestDoubles(
        base::WrapUnique(fake_disconnect_tethering_request_sender_.get()));
  }

  void CallShutdown() {
    container_->Shutdown(base::BindOnce(
        &AsynchronousShutdownObjectContainerImplTest::OnShutdownComplete,
        base::Unretained(this)));
  }

  void OnShutdownComplete() { was_shutdown_callback_invoked_ = true; }

  base::test::TaskEnvironment task_environment_;
  const multidevice::RemoteDeviceRef test_device_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeRemoteDeviceProviderFactory>
      fake_remote_device_provider_factory_;
  raw_ptr<FakeDisconnectTetheringRequestSender, DanglingUntriaged>
      fake_disconnect_tethering_request_sender_;

  bool was_shutdown_callback_invoked_;

  std::unique_ptr<AsynchronousShutdownObjectContainerImpl> container_;
};

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_NoAsyncShutdown) {
  CallShutdown();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_AsyncDisconnectTetheringRequestSenderShutdown) {
  fake_disconnect_tethering_request_sender_->set_has_pending_requests(true);
  EXPECT_TRUE(fake_disconnect_tethering_request_sender_->HasPendingRequests());

  // Start the shutdown; it should not yet succeed since there are pending
  // requests.
  CallShutdown();
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, finish the pending requests; this should cause the shutdown to
  // complete.
  fake_disconnect_tethering_request_sender_->set_has_pending_requests(false);
  fake_disconnect_tethering_request_sender_
      ->NotifyPendingDisconnectRequestsComplete();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_MultipleSimultaneousAsyncShutdowns) {
  fake_disconnect_tethering_request_sender_->set_has_pending_requests(true);
  EXPECT_TRUE(fake_disconnect_tethering_request_sender_->HasPendingRequests());

  // Start the shutdown; it should not yet succeed since there are pending
  // requests.
  CallShutdown();
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, finish the pending requests; this should cause the shutdown to
  // complete.
  fake_disconnect_tethering_request_sender_->set_has_pending_requests(false);
  fake_disconnect_tethering_request_sender_
      ->NotifyPendingDisconnectRequestsComplete();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

}  // namespace tether

}  // namespace ash
