// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_classic_medium.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {
const char kDeviceAddress1[] = "DeviceAddress1";
const char kDeviceAddress2[] = "DeviceAddress2";
const char kDeviceName1[] = "DeviceName1";
const char kDeviceName2[] = "DeviceName2";
const char kNearbySharingServiceName[] = "NearbySharing";
const device::BluetoothUUID kNearbySharingServiceUuid =
    device::BluetoothUUID("a82efa21-ae5c-3dde-9bbc-f16da7b16c5a");
const base::TimeDelta kStaleDeviceTimeout = base::Seconds(20);
}  // namespace

class BluetoothClassicMediumTest : public testing::Test {
 public:
  BluetoothClassicMediumTest() = default;
  ~BluetoothClassicMediumTest() override = default;
  BluetoothClassicMediumTest(const BluetoothClassicMediumTest&) = delete;
  BluetoothClassicMediumTest& operator=(const BluetoothClassicMediumTest&) =
      delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();

    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());

    remote_adapter_.Bind(std::move(pending_adapter),
                         /*bind_task_runner=*/nullptr);

    bluetooth_classic_medium_ =
        std::make_unique<BluetoothClassicMedium>(remote_adapter_);

    discovery_callback_ = {
        .device_discovered_cb =
            [this](api::BluetoothDevice& device) {
              last_device_discovered_ = &device;
              std::move(on_device_discovered_callback_).Run();
            },
        .device_name_changed_cb =
            [this](api::BluetoothDevice& device) {
              last_device_name_changed_ = &device;
              std::move(on_device_name_changed_callback_).Run();
            },
        .device_lost_cb =
            [this](api::BluetoothDevice& device) {
              DCHECK_EQ(expected_last_device_lost_, &device);
              std::move(on_device_lost_callback_).Run();
            }};
  }

 protected:
  void StartDiscovery() {
    EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
    EXPECT_TRUE(bluetooth_classic_medium_->StartDiscovery(
        std::move(discovery_callback_)));
    EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());
  }

  void StopDiscovery() {
    base::RunLoop run_loop;
    fake_adapter_->SetDiscoverySessionDestroyedCallback(run_loop.QuitClosure());
    EXPECT_TRUE(bluetooth_classic_medium_->StopDiscovery());
    run_loop.Run();

    EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  }

  void NotifyDeviceAdded(const std::string& address, const std::string& name) {
    base::RunLoop run_loop;
    on_device_discovered_callback_ = run_loop.QuitClosure();
    fake_adapter_->NotifyDeviceAdded(CreateDeviceInfo(address, name));
    run_loop.Run();
  }

  void NotifyDeviceChanged(const std::string& address,
                           const std::string& name) {
    base::RunLoop run_loop;
    on_device_name_changed_callback_ = run_loop.QuitClosure();
    fake_adapter_->NotifyDeviceChanged(CreateDeviceInfo(address, name));
    run_loop.Run();
  }

  void NotifyDeviceRemoved(const std::string& address,
                           const std::string& name) {
    base::RunLoop run_loop;
    on_device_lost_callback_ = run_loop.QuitClosure();
    fake_adapter_->NotifyDeviceRemoved(CreateDeviceInfo(address, name));
    run_loop.Run();
  }

  raw_ptr<bluetooth::FakeAdapter> fake_adapter_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BluetoothClassicMedium> bluetooth_classic_medium_;
  BluetoothClassicMedium::DiscoveryCallback discovery_callback_;

  raw_ptr<api::BluetoothDevice, DanglingUntriaged> last_device_discovered_ =
      nullptr;
  raw_ptr<api::BluetoothDevice, DanglingUntriaged> last_device_name_changed_ =
      nullptr;
  raw_ptr<api::BluetoothDevice, DanglingUntriaged> expected_last_device_lost_ =
      nullptr;

  base::OnceClosure on_device_discovered_callback_;
  base::OnceClosure on_device_name_changed_callback_;
  base::OnceClosure on_device_lost_callback_;

  bluetooth::mojom::DeviceInfoPtr CreateDeviceInfo(const std::string& address,
                                                   const std::string& name) {
    auto device_info = bluetooth::mojom::DeviceInfo::New();
    device_info->address = address;
    device_info->name = name;
    device_info->name_for_display = name;
    return device_info;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(BluetoothClassicMediumTest, TestDiscovery_StartDiscoveryError) {
  fake_adapter_->SetShouldDiscoverySucceed(false);
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  EXPECT_FALSE(bluetooth_classic_medium_->StartDiscovery(
      std::move(discovery_callback_)));
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
}

TEST_F(BluetoothClassicMediumTest,
       TestDiscovery_BluetoothClassicScanningFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          ::features::kEnableNearbyBluetoothClassicScanning});

  // When classic scanning flag is disabled, Discovery will fail.
  fake_adapter_->SetShouldDiscoverySucceed(true);
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  EXPECT_FALSE(bluetooth_classic_medium_->StartDiscovery(
      std::move(discovery_callback_)));
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
}

TEST_F(BluetoothClassicMediumTest,
       TestDiscovery_BluetoothClassicScanningFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBluetoothClassicScanning},
      /*disabled_features=*/{});

  // When classic scanning flag is enabled, normal Discovery operation is not
  // impacted.
  fake_adapter_->SetShouldDiscoverySucceed(false);
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());
  EXPECT_FALSE(bluetooth_classic_medium_->StartDiscovery(
      std::move(discovery_callback_)));
  EXPECT_FALSE(fake_adapter_->IsDiscoverySessionActive());

  fake_adapter_->SetShouldDiscoverySucceed(true);
  EXPECT_TRUE(bluetooth_classic_medium_->StartDiscovery(
      std::move(discovery_callback_)));
  EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());
}

TEST_F(BluetoothClassicMediumTest,
       TestDiscovery_GetRemoteDevice_GetUndiscovered) {
  StartDiscovery();

  EXPECT_TRUE(bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress1));

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest,
       TestDiscovery_DeviceDiscovered_BluetoothClassicDevice) {
  StartDiscovery();

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_TRUE(bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress1));
  EXPECT_EQ(last_device_discovered_,
            bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress1));
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());

  auto* first_device_discovered = last_device_discovered_.get();

  NotifyDeviceAdded(kDeviceAddress2, kDeviceName2);
  EXPECT_TRUE(bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress2));
  EXPECT_EQ(last_device_discovered_,
            bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress2));
  EXPECT_EQ(kDeviceName2, last_device_discovered_->GetName());

  EXPECT_NE(first_device_discovered, last_device_discovered_);

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_StaleDeviceRemoved) {
  StartDiscovery();

  bool device_lost_called = false;
  on_device_lost_callback_ =
      base::BindOnce([](bool* called) { *called = true; }, &device_lost_called);

  task_environment_.FastForwardBy(0.75 * kStaleDeviceTimeout);
  EXPECT_FALSE(device_lost_called);

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_TRUE(bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress1));
  EXPECT_EQ(last_device_discovered_,
            bluetooth_classic_medium_->GetRemoteDevice(kDeviceAddress1));
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());
  expected_last_device_lost_ = last_device_discovered_;

  // Trigger an adapter event before the device times out. Verify that the
  // device's time is renewed.
  task_environment_.FastForwardBy(0.75 * kStaleDeviceTimeout);
  EXPECT_FALSE(device_lost_called);
  NotifyDeviceChanged(kDeviceAddress1, kDeviceName2);
  task_environment_.FastForwardBy(0.75 * kStaleDeviceTimeout);
  EXPECT_FALSE(device_lost_called);

  // Wait for the device to time out. Verify that it is reported lost.
  task_environment_.FastForwardBy(1.5 * kStaleDeviceTimeout);
  EXPECT_TRUE(device_lost_called);

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest,
       TestDiscovery_DeviceDiscovered_BleAdvertisement) {
  StartDiscovery();

  on_device_discovered_callback_ = base::BindOnce([]() { FAIL(); });

  // Do not set |name|. This reflects Chrome's usual representation of a BLE
  // advertisement.
  auto device_info = bluetooth::mojom::DeviceInfo::New();
  device_info->address = kDeviceAddress1;
  device_info->name_for_display = kDeviceAddress1;

  fake_adapter_->NotifyDeviceAdded(std::move(device_info));

  EXPECT_FALSE(last_device_discovered_);

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_DeviceNameChanged) {
  StartDiscovery();

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());

  NotifyDeviceChanged(kDeviceAddress1, kDeviceName2);
  EXPECT_EQ(kDeviceName2, last_device_name_changed_->GetName());

  EXPECT_EQ(last_device_name_changed_, last_device_discovered_);

  // It is possible for DeviceChanged to trigger without a name change. This
  // previously caused a name change event. Here we verify if the name
  // does not change then we do not see the name change event.
  last_device_name_changed_ = nullptr;
  // We have to call NotifyDeviceChanged directly since we don't expect the
  // callback to be invoked.
  base::RunLoop run_loop;
  fake_adapter_->NotifyDeviceChanged(
      CreateDeviceInfo(kDeviceAddress1, kDeviceName2));
  run_loop.RunUntilIdle();
  EXPECT_EQ(nullptr, last_device_name_changed_.get());

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestDiscovery_DeviceLost) {
  StartDiscovery();

  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  EXPECT_EQ(kDeviceName1, last_device_discovered_->GetName());

  expected_last_device_lost_ = last_device_discovered_;
  NotifyDeviceRemoved(kDeviceAddress1, kDeviceName1);

  StopDiscovery();
}

TEST_F(BluetoothClassicMediumTest, TestConnectToService_Success) {
  StartDiscovery();
  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  StopDiscovery();

  fake_adapter_->AllowConnectionForAddressAndUuidPair(
      kDeviceAddress1, kNearbySharingServiceUuid);

  auto cancellation_flag = std::make_unique<CancellationFlag>();
  auto bluetooth_socket = bluetooth_classic_medium_->ConnectToService(
      *last_device_discovered_, kNearbySharingServiceUuid.value(),
      cancellation_flag.get());
  EXPECT_EQ(last_device_discovered_, bluetooth_socket->GetRemoteDevice());

  EXPECT_TRUE(bluetooth_socket->Close().Ok());
}

TEST_F(BluetoothClassicMediumTest,
       TestConnectToService_CancelledByCancellationFlag) {
  StartDiscovery();
  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  StopDiscovery();

  fake_adapter_->AllowConnectionForAddressAndUuidPair(
      kDeviceAddress1, kNearbySharingServiceUuid);

  auto cancellation_flag = std::make_unique<CancellationFlag>();
  cancellation_flag->Cancel();

  EXPECT_FALSE(bluetooth_classic_medium_->ConnectToService(
      *last_device_discovered_, kNearbySharingServiceUuid.value(),
      cancellation_flag.get()));
}

TEST_F(BluetoothClassicMediumTest, TestConnectToService_Failure) {
  StartDiscovery();
  NotifyDeviceAdded(kDeviceAddress1, kDeviceName1);
  NotifyDeviceAdded(kDeviceAddress2, kDeviceName2);
  StopDiscovery();

  // Do not allow "Device 2".
  fake_adapter_->AllowConnectionForAddressAndUuidPair(
      kDeviceAddress1, kNearbySharingServiceUuid);

  EXPECT_FALSE(bluetooth_classic_medium_->ConnectToService(
      *last_device_discovered_, kNearbySharingServiceUuid.value(), nullptr));
}

TEST_F(BluetoothClassicMediumTest, TestListenForService_Success) {
  fake_adapter_->AllowIncomingConnectionForServiceNameAndUuidPair(
      kNearbySharingServiceName, kNearbySharingServiceUuid);

  EXPECT_TRUE(bluetooth_classic_medium_->ListenForService(
      kNearbySharingServiceName, kNearbySharingServiceUuid.value()));
}

TEST_F(BluetoothClassicMediumTest, TestListenForService_Failure) {
  fake_adapter_->AllowIncomingConnectionForServiceNameAndUuidPair(
      "DifferentServiceName", kNearbySharingServiceUuid);
  fake_adapter_->AllowIncomingConnectionForServiceNameAndUuidPair(
      kNearbySharingServiceName, device::BluetoothUUID("DifferentServiceId"));

  EXPECT_FALSE(bluetooth_classic_medium_->ListenForService(
      kNearbySharingServiceName, kNearbySharingServiceUuid.value()));
}

}  // namespace nearby::chrome
