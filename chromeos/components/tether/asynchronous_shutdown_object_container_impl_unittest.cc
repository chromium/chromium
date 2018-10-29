// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/asynchronous_shutdown_object_container_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_ble_advertiser.h"
#include "chromeos/components/tether/fake_ble_scanner.h"
#include "chromeos/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/tether_component_impl.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/fake_cryptauth_service.h"
#include "components/cryptauth/fake_remote_device_provider.h"
#include "components/cryptauth/remote_device_provider_impl.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace tether {

namespace {

class FakeRemoteDeviceProviderFactory
    : public cryptauth::RemoteDeviceProviderImpl::Factory {
 public:
  FakeRemoteDeviceProviderFactory() = default;
  ~FakeRemoteDeviceProviderFactory() override = default;

  // cryptauth::RemoteDeviceProviderImpl::Factory:
  std::unique_ptr<cryptauth::RemoteDeviceProvider> BuildInstance(
      cryptauth::CryptAuthDeviceManager* device_manager,
      const std::string& user_id,
      const std::string& user_private_key) override {
    return std::make_unique<cryptauth::FakeRemoteDeviceProvider>();
  }
};

}  // namespace

class AsynchronousShutdownObjectContainerImplTest : public testing::Test {
 protected:
  AsynchronousShutdownObjectContainerImplTest()
      : test_device_(cryptauth::CreateRemoteDeviceRefListForTest(1u)[0]) {}

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(features::kMultiDeviceApi);

    was_shutdown_callback_invoked_ = false;
    is_adapter_powered_ = true;

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(
            Invoke(this, &AsynchronousShutdownObjectContainerImplTest::
                             MockIsAdapterPowered));

    fake_remote_device_provider_factory_ =
        base::WrapUnique(new FakeRemoteDeviceProviderFactory());
    cryptauth::RemoteDeviceProviderImpl::Factory::SetInstanceForTesting(
        fake_remote_device_provider_factory_.get());

    fake_cryptauth_service_ =
        std::make_unique<cryptauth::FakeCryptAuthService>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_tether_host_fetcher_ = std::make_unique<FakeTetherHostFetcher>();

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    TetherComponentImpl::RegisterProfilePrefs(test_pref_service_->registry());

    // Note: The null pointers passed to the constructor are not actually used
    // by the object itself; rather, they are simply passed to the constructors
    // of objects created by the container.
    container_ = base::WrapUnique(new AsynchronousShutdownObjectContainerImpl(
        mock_adapter_, fake_cryptauth_service_.get(),
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        fake_tether_host_fetcher_.get() /* tether_host_fetcher */,
        nullptr /* network_state_handler */,
        nullptr /* managed_network_configuration_handler */,
        nullptr /* network_connection_handler */,
        test_pref_service_.get() /* pref_service */));

    fake_ble_advertiser_ = new FakeBleAdvertiser(
        false /* automatically_update_active_advertisements */);
    fake_ble_scanner_ =
        new FakeBleScanner(false /* automatically_update_discovery_session */);
    fake_disconnect_tethering_request_sender_ =
        new FakeDisconnectTetheringRequestSender();

    container_->SetTestDoubles(
        base::WrapUnique(fake_ble_advertiser_),
        base::WrapUnique(fake_ble_scanner_),
        base::WrapUnique(fake_disconnect_tethering_request_sender_));
  }

  bool MockIsAdapterPowered() { return is_adapter_powered_; }

  void CallShutdown() {
    container_->Shutdown(base::Bind(
        &AsynchronousShutdownObjectContainerImplTest::OnShutdownComplete,
        base::Unretained(this)));
  }

  void OnShutdownComplete() { was_shutdown_callback_invoked_ = true; }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const cryptauth::RemoteDeviceRef test_device_;
  base::test::ScopedFeatureList scoped_feature_list_;

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<cryptauth::FakeCryptAuthService> fake_cryptauth_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeRemoteDeviceProviderFactory>
      fake_remote_device_provider_factory_;
  FakeBleAdvertiser* fake_ble_advertiser_;
  FakeBleScanner* fake_ble_scanner_;
  FakeDisconnectTetheringRequestSender*
      fake_disconnect_tethering_request_sender_;

  bool was_shutdown_callback_invoked_;
  bool is_adapter_powered_;

  std::unique_ptr<AsynchronousShutdownObjectContainerImpl> container_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsynchronousShutdownObjectContainerImplTest);
};

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_NoAsyncShutdown) {
  CallShutdown();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_AsyncBleAdvertiserShutdown) {
  fake_ble_advertiser_->set_are_advertisements_registered(true);
  EXPECT_TRUE(fake_ble_advertiser_->AreAdvertisementsRegistered());

  // Start the shutdown; it should not yet succeed since there are still
  // registered advertisements.
  CallShutdown();
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, remove these advertisements; this should cause the shutdown to
  // complete.
  fake_ble_advertiser_->set_are_advertisements_registered(false);
  fake_ble_advertiser_->NotifyAllAdvertisementsUnregistered();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_AsyncBleScannerShutdown) {
  fake_ble_scanner_->set_is_discovery_session_active(true);
  EXPECT_FALSE(fake_ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(fake_ble_scanner_->IsDiscoverySessionActive());

  // Start the shutdown; it should not yet succeed since there is an active
  // discovery session.
  CallShutdown();
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, remove the discovery session; this should cause the shutdown to
  // complete.
  fake_ble_scanner_->set_is_discovery_session_active(false);
  fake_ble_scanner_->NotifyDiscoverySessionStateChanged(
      false /* discovery_session_active */);
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
  fake_ble_advertiser_->set_are_advertisements_registered(true);
  EXPECT_TRUE(fake_ble_advertiser_->AreAdvertisementsRegistered());

  fake_ble_scanner_->set_is_discovery_session_active(true);
  EXPECT_FALSE(fake_ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(fake_ble_scanner_->IsDiscoverySessionActive());

  fake_disconnect_tethering_request_sender_->set_has_pending_requests(true);
  EXPECT_TRUE(fake_disconnect_tethering_request_sender_->HasPendingRequests());

  // Start the shutdown; it should not yet succeed since there are pending
  // requests.
  CallShutdown();
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, remove the advertisements; this should not cause the shutdown to
  // complete since there is still an active discovery session and pending
  // requests.
  fake_ble_advertiser_->set_are_advertisements_registered(false);
  fake_ble_advertiser_->NotifyAllAdvertisementsUnregistered();
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, remove the discovery session; this should not cause the shutdown to
  // complete since there are still pending requests.
  fake_ble_scanner_->set_is_discovery_session_active(false);
  fake_ble_scanner_->NotifyDiscoverySessionStateChanged(
      false /* discovery_session_active */);
  EXPECT_FALSE(was_shutdown_callback_invoked_);

  // Now, finish the pending requests; this should cause the shutdown to
  // complete.
  fake_disconnect_tethering_request_sender_->set_has_pending_requests(false);
  fake_disconnect_tethering_request_sender_
      ->NotifyPendingDisconnectRequestsComplete();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

TEST_F(AsynchronousShutdownObjectContainerImplTest,
       TestShutdown_MultipleSimultaneousAsyncShutdowns_BluetoothDisabled) {
  fake_ble_advertiser_->set_are_advertisements_registered(true);
  EXPECT_TRUE(fake_ble_advertiser_->AreAdvertisementsRegistered());

  fake_ble_scanner_->set_is_discovery_session_active(true);
  EXPECT_FALSE(fake_ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(fake_ble_scanner_->IsDiscoverySessionActive());

  fake_disconnect_tethering_request_sender_->set_has_pending_requests(true);
  EXPECT_TRUE(fake_disconnect_tethering_request_sender_->HasPendingRequests());

  // Shut off Bluetooth power. This should cause a synchronous shutdown despite
  // the fact that there are still objects which require an asynchronous
  // shutdown.
  is_adapter_powered_ = false;
  CallShutdown();
  EXPECT_TRUE(was_shutdown_callback_invoked_);
}

}  // namespace tether

}  // namespace chromeos
