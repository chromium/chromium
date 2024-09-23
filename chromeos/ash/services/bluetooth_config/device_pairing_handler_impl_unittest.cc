// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_pairing_handler_impl.h"

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_device_pairing_delegate.h"
#include "chromeos/ash/services/bluetooth_config/fake_fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/fake_key_entered_handler.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::bluetooth_config {

namespace {

using NiceMockDevice =
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>;
using ConnectErrorCode = device::BluetoothDevice::ConnectErrorCode;

const char kTestDeviceIdSuffix[] = "-Identifier";
const uint32_t kTestBluetoothClass = 1337u;
const char kTestBluetoothName[] = "testName";
constexpr char kTestDefaultImage[] = "data:image/png;base64,TestDefaultImage";

const char kDefaultPinCode[] = "132546";
const uint32_t kDefaultPinCodeNum = 132546u;
const uint32_t kUninitializedPasskey = std::numeric_limits<uint32_t>::min();
const base::TimeDelta kTestDuration = base::Milliseconds(1000);

enum class AuthType {
  kNone,
  kRequestPinCode,
  kRequestPasskey,
  kDisplayPinCode,
  kDisplayPasskey,
  kConfirmPasskey,
  kAuthorizePairing,
};

}  // namespace

class DevicePairingHandlerImplTest : public testing::Test {
 protected:
  DevicePairingHandlerImplTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  DevicePairingHandlerImplTest(const DevicePairingHandlerImplTest&) = delete;
  DevicePairingHandlerImplTest& operator=(const DevicePairingHandlerImplTest&) =
      delete;
  ~DevicePairingHandlerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, GetDevices())
        .WillByDefault(testing::Invoke(
            this, &DevicePairingHandlerImplTest::GetMockDevices));

    device_pairing_handler_ = std::make_unique<DevicePairingHandlerImpl>(
        remote_handler_.BindNewPipeAndPassReceiver(),
        &fake_adapter_state_controller_, mock_adapter_,
        &fake_fast_pair_delegate_);
  }

  void SetBluetoothSystemState(mojom::BluetoothSystemState system_state) {
    fake_adapter_state_controller_.SetSystemState(system_state);
  }

  void DestroyHandler() { device_pairing_handler_.reset(); }

  void CheckPairingHistograms(device::BluetoothTransportType type,
                              int type_count,
                              int failure_count,
                              int success_count) {
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.TransportType", type, type_count);
    histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.Pairing.Result",
                                       false, failure_count);
    histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.Pairing.Result",
                                       true, success_count);
  }

  void CheckPairingFailureHistogram(
      device::ConnectionFailureReason failure_reason,
      size_t failure_count,
      size_t filtered_count) {
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Result.FailureReason", failure_reason,
        failure_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Result.FilteredFailureReason",
        failure_reason, filtered_count);
  }

  void CheckDurationHistogramMetrics(base::TimeDelta bucket,
                                     int success_count,
                                     int failure_count,
                                     std::string transport_name) {
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Duration.Success", bucket.InMilliseconds(),
        success_count);
    histogram_tester.ExpectBucketCount(
        base::StrCat(
            {"Bluetooth.ChromeOS.Pairing.Duration.Success.", transport_name}),
        bucket.InMilliseconds(), success_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Duration.Failure", bucket.InMilliseconds(),
        failure_count);
    histogram_tester.ExpectBucketCount(
        base::StrCat(
            {"Bluetooth.ChromeOS.Pairing.Duration.Failure.", transport_name}),
        bucket.InMilliseconds(), failure_count);
  }

  void AddDevice(std::string* id_out,
                 AuthType auth_type,
                 uint32_t passkey = kDefaultPinCodeNum) {
    // We use the number of devices created in this test as the address.
    std::string address = base::NumberToString(num_devices_created_);
    ++num_devices_created_;

    // Mock devices have their ID set to "${address}-Identifier".
    *id_out = base::StrCat({address, kTestDeviceIdSuffix});

    auto mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), kTestBluetoothClass, kTestBluetoothName,
            address, /*paired=*/false, /*connected=*/false);

    device::BluetoothDevice* device = mock_device.get();
    ON_CALL(*mock_device, Connect(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this, auth_type, device, passkey](
                device::BluetoothDevice::PairingDelegate* pairing_delegate,
                device::BluetoothDevice::ConnectCallback callback) {
              EXPECT_FALSE(connect_callback_);
              connect_callback_ = std::move(callback);

              did_use_auth_ = true;
              switch (auth_type) {
                case AuthType::kNone:
                  did_use_auth_ = false;
                  break;
                case AuthType::kRequestPinCode:
                  pairing_delegate->RequestPinCode(device);
                  break;
                case AuthType::kRequestPasskey:
                  pairing_delegate->RequestPasskey(device);
                  break;
                case AuthType::kDisplayPinCode:
                  pairing_delegate->DisplayPinCode(device, kDefaultPinCode);
                  break;
                case AuthType::kDisplayPasskey:
                  pairing_delegate->DisplayPasskey(device, passkey);
                  break;
                case AuthType::kConfirmPasskey:
                  pairing_delegate->ConfirmPasskey(device, passkey);
                  break;
                case AuthType::kAuthorizePairing:
                  pairing_delegate->AuthorizePairing(device);
                  break;
              }
            }));

    ON_CALL(*mock_device, CancelPairing())
        .WillByDefault(testing::Invoke([this]() {
          num_cancel_pairing_calls_++;
          std::move(connect_callback_)
              .Run(device::BluetoothDevice::ConnectErrorCode::
                       ERROR_AUTH_CANCELED);
        }));

    ON_CALL(*mock_device, ConfirmPairing())
        .WillByDefault(
            testing::Invoke([this]() { num_confirm_pairing_calls_++; }));

    ON_CALL(*mock_device, SetPinCode(testing::_))
        .WillByDefault(testing::Invoke([this](const std::string& pincode) {
          received_pin_code_ = pincode;
        }));

    ON_CALL(*mock_device, SetPasskey(testing::_))
        .WillByDefault(testing::Invoke(
            [this](const uint32_t passkey) { received_passkey_ = passkey; }));

    ON_CALL(*mock_device, GetType()).WillByDefault(testing::Invoke([]() {
      return device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC;
    }));

    mock_devices_.push_back(std::move(mock_device));
  }

  void ClearDevices() { mock_devices_.clear(); }

  std::unique_ptr<FakeDevicePairingDelegate> PairDevice(
      const std::string& device_id) {
    auto delegate = std::make_unique<FakeDevicePairingDelegate>();

    remote_handler_->PairDevice(
        device_id, delegate->GeneratePendingRemote(),
        base::BindOnce(&DevicePairingHandlerImplTest::OnPairDevice,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    return delegate;
  }

  void OnPairDevice(mojom::PairingResult pairing_result) {
    pairing_result_ = std::move(pairing_result);
  }

  bool HasPendingConnectCallback() const {
    return !connect_callback_.is_null();
  }

  base::TimeDelta GetPairingFailureDelay() {
    return DevicePairingHandlerImpl::kPairingFailureDelay;
  }

  void FastForwardOperation(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  void InvokePendingConnectCallback(
      std::optional<ConnectErrorCode> error_code) {
    std::move(connect_callback_).Run(error_code);
    device_pairing_handler_->FlushForTesting();
  }

  void EnterKeys(const std::string device_id, uint32_t num_keys_entered) {
    device_pairing_handler_->KeysEntered(FindDevice(device_id)->get(),
                                         num_keys_entered);
    base::RunLoop().RunUntilIdle();
  }

  void ChangeDeviceIsConnected(const std::string& device_id,
                               bool is_connected) {
    auto it = FindDevice(device_id);
    EXPECT_TRUE(it != mock_devices_.end());

    ON_CALL(**it, IsConnected()).WillByDefault(testing::Return(is_connected));
  }

  mojom::BluetoothDevicePropertiesPtr FetchDevice(
      const std::string& device_address) {
    mojom::BluetoothDevicePropertiesPtr result;
    remote_handler_->FetchDevice(
        device_address,
        base::BindLambdaForTesting(
            [&result](mojom::BluetoothDevicePropertiesPtr device) {
              result = std::move(device);
            }));
    base::RunLoop().RunUntilIdle();
    return result;
  }

  std::string GetDeviceAddress(const std::string& device_id) {
    return device_id.substr(
        /*pos=*/0, device_id.length() -
                       ((sizeof(kTestDeviceIdSuffix) - 1) / sizeof(char)));
  }

  const std::optional<mojom::PairingResult>& pairing_result() const {
    return pairing_result_;
  }
  size_t num_cancel_pairing_calls() const { return num_cancel_pairing_calls_; }
  size_t num_confirm_pairing_calls() const {
    return num_confirm_pairing_calls_;
  }
  std::string received_pin_code() const { return received_pin_code_; }
  uint32_t received_passkey() const { return received_passkey_; }

  FakeFastPairDelegate* fake_fast_pair_delegate() {
    return &fake_fast_pair_delegate_;
  }

 private:
  std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
  GetMockDevices() {
    std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
        devices;
    for (auto& device : mock_devices_)
      devices.push_back(device.get());
    return devices;
  }

  std::vector<NiceMockDevice>::iterator FindDevice(
      const std::string& device_id) {
    return base::ranges::find(
        mock_devices_, device_id,
        &testing::NiceMock<device::MockBluetoothDevice>::GetIdentifier);
  }

  base::test::TaskEnvironment task_environment_;

  base::HistogramTester histogram_tester;

  std::optional<mojom::PairingResult> pairing_result_;

  // Properties set by device::BluetoothDevice methods.
  device::BluetoothDevice::ConnectCallback connect_callback_;
  bool did_use_auth_ = false;
  size_t num_cancel_pairing_calls_ = 0u;
  size_t num_confirm_pairing_calls_ = 0u;
  std::string received_pin_code_;
  uint32_t received_passkey_ = kUninitializedPasskey;

  std::vector<NiceMockDevice> mock_devices_;
  size_t num_devices_created_ = 0u;

  FakeAdapterStateController fake_adapter_state_controller_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  FakeFastPairDelegate fake_fast_pair_delegate_;

  mojo::Remote<mojom::DevicePairingHandler> remote_handler_;
  std::unique_ptr<DevicePairingHandlerImpl> device_pairing_handler_;
};

TEST_F(DevicePairingHandlerImplTest, FetchDeviceExists) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  // Add device image info to ensure the FastPairDelegate is correctly provided
  // to DeviceConversionUtil.
  DeviceImageInfo image_info = DeviceImageInfo(
      /*default_image=*/kTestDefaultImage, /*left_bud_image=*/"",
      /*right_bud_image=*/"", /*case_image=*/"");
  std::string device_address = GetDeviceAddress(device_id);
  fake_fast_pair_delegate()->SetDeviceImageInfo(device_address, image_info);

  mojom::BluetoothDevicePropertiesPtr device = FetchDevice(device_address);
  EXPECT_TRUE(device);
  EXPECT_EQ(device->id, device_id);
  EXPECT_EQ(device->address, device_address);
  EXPECT_TRUE(device->image_info);
}

TEST_F(DevicePairingHandlerImplTest, FetchDeviceNotFound) {
  EXPECT_FALSE(FetchDevice("invalid"));
}

TEST_F(DevicePairingHandlerImplTest, MultipleDevicesPairAuthNone) {
  std::string device_id1;
  AddDevice(&device_id1, AuthType::kNone);
  std::string device_id2;
  AddDevice(&device_id2, AuthType::kNone);

  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/0,
                                /*transport_name=*/"Classic");

  std::unique_ptr<FakeDevicePairingDelegate> delegate1 = PairDevice(device_id1);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());

  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(ConnectErrorCode::ERROR_FAILED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");

  std::unique_ptr<FakeDevicePairingDelegate> delegate2 = PairDevice(device_id2);
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());

  EXPECT_TRUE(HasPendingConnectCallback());
  FastForwardOperation(kTestDuration);
  InvokePendingConnectCallback(/*error_code=*/std::nullopt);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kSuccess);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/2, /*failure_count=*/1,
                         /*success_count=*/1);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/1,
                                /*failure_count=*/0,
                                /*transport_name=*/"Classic");
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, DisableBluetoothBeforePairing) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  // Disable Bluetooth before attempting to pair.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabled);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());

  // Pairing should immediately fail.
  EXPECT_FALSE(HasPendingConnectCallback());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(
      device::ConnectionFailureReason::kBluetoothDisabled,
      /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(base::Milliseconds(0), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest, DisableBluetoothDuringPairing) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  FastForwardOperation(kTestDuration);

  // Disable Bluetooth before invoking the Connect() callback.
  SetBluetoothSystemState(mojom::BluetoothSystemState::kDisabling);

  // Pairing should fail.
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(num_cancel_pairing_calls(), 1u);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, DestroyHandlerBeforeConnectFinishes) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_TRUE(HasPendingConnectCallback());
  DestroyHandler();

  // CancelPairing() should be called since we had an active pairing.
  EXPECT_EQ(num_cancel_pairing_calls(), 1u);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(base::Milliseconds(0), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, DestroyHandlerAfterConnectFinishes) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());

  EXPECT_TRUE(HasPendingConnectCallback());
  FastForwardOperation(kTestDuration);
  InvokePendingConnectCallback(/*error_code=*/std::nullopt);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kSuccess);

  DestroyHandler();

  // CancelPairing() should not be called since we finished pairing.
  EXPECT_EQ(num_cancel_pairing_calls(), 0u);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/0,
                         /*success_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/1,
                                /*failure_count=*/0,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, DisconnectDelegateBeforeConnectFinishes) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kDisplayPinCode);

  std::unique_ptr<FakeDevicePairingDelegate> delegate1 = PairDevice(device_id);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());
  EXPECT_TRUE(HasPendingConnectCallback());
  EXPECT_EQ(delegate1->displayed_pin_code(), kDefaultPinCode);
  EXPECT_TRUE(delegate1->key_entered_handler()->IsMojoPipeConnected());

  // Simulate device temporarily being connected during auth.
  ChangeDeviceIsConnected(device_id, /*is_connected=*/true);

  FastForwardOperation(kTestDuration);
  delegate1->DisconnectMojoPipe();

  // Once the pairing has failed, the device will return to not connected.
  ChangeDeviceIsConnected(device_id, /*is_connected=*/false);

  // CancelPairing() should be called since we had an active pairing. There
  // should be no delay for handling the pairing failing. Even though
  // device->IsConnected() returns true when the pairing is canceled, it should
  // fail because CancelPairing() was called.
  EXPECT_EQ(num_cancel_pairing_calls(), 1u);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");

  // Attempt to pair with the device again.
  std::unique_ptr<FakeDevicePairingDelegate> delegate2 = PairDevice(device_id);
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_FAILED);

  // There should be a delay for handling the pairing failing.
  FastForwardOperation(GetPairingFailureDelay());

  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/2, /*failure_count=*/2,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest,
       DisconnectDelegateBeforeConnectFinishesDeviceNotFound) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_TRUE(HasPendingConnectCallback());
  ClearDevices();

  FastForwardOperation(kTestDuration);
  delegate->DisconnectMojoPipe();

  // CancelPairing() won't be called since the device won't be found. We should
  // still return with a pairing result.
  EXPECT_EQ(num_cancel_pairing_calls(), 0u);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kNotFound,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest,
       DisconnectDelegateAfterFailedConnectFinishes) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_TRUE(HasPendingConnectCallback());
  InvokePendingConnectCallback(ConnectErrorCode::ERROR_FAILED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);

  delegate->DisconnectMojoPipe();

  // CancelPairing() should not be called since we finished pairing.
  EXPECT_EQ(num_cancel_pairing_calls(), 0u);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairingFailsWhenThereIsAPendingPairing) {
  std::string device_id1;
  AddDevice(&device_id1, AuthType::kNone);

  std::string device_id2;
  AddDevice(&device_id2, AuthType::kNone);

  std::unique_ptr<FakeDevicePairingDelegate> delegate1 = PairDevice(device_id1);
  EXPECT_TRUE(HasPendingConnectCallback());
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());

  std::unique_ptr<FakeDevicePairingDelegate> delegate2 = PairDevice(device_id1);
  EXPECT_FALSE(delegate2->IsMojoPipeConnected());

  // Pending callback should still exist, this would be the first device pairing
  // pending callback.
  EXPECT_TRUE(HasPendingConnectCallback());

  // We expect the second pairing request to fail.
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);

  // Check that first pairing was successful.
  FastForwardOperation(kTestDuration);

  // We expect no failure histogram recorded since we returned early in
  // PairDevice.
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/0, /*failure_count=*/0,
                         /*success_count=*/0);

  InvokePendingConnectCallback(/*error_code=*/std::nullopt);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kSuccess);

  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/0,
                         /*success_count=*/1);
}

TEST_F(DevicePairingHandlerImplTest, PairDeviceNotFound) {
  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice("device_id");
  EXPECT_TRUE(delegate->IsMojoPipeConnected());

  EXPECT_FALSE(HasPendingConnectCallback());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(base::Milliseconds(0), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest, PairFailsDeviceConnected) {
  // Simulate case where pairing succeeded but the subsequent connection request
  // returns with a failure. Empirically, it's found that the device actually
  // does connect and should be treated as a successful pairing (See
  // b/209531279).
  std::string device_id;
  AddDevice(&device_id, AuthType::kNone);

  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/0, /*failure_count=*/0,
                         /*success_count=*/0);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/0,
                                /*transport_name=*/"Classic");

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(HasPendingConnectCallback());
  EXPECT_TRUE(delegate->IsMojoPipeConnected());

  // Update device to be connected.
  ChangeDeviceIsConnected(device_id, /*is_connected=*/true);

  // Fail the pairing.
  InvokePendingConnectCallback(ConnectErrorCode::ERROR_FAILED);
  FastForwardOperation(GetPairingFailureDelay());

  // The pairing should still return as a success.
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kSuccess);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/0,
                         /*success_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/1,
                                /*failure_count=*/0,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthRequestPinCode) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kRequestPinCode);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_TRUE(delegate->HasPendingRequestPinCodeCallback());

  delegate->InvokePendingRequestPinCodeCallback(kDefaultPinCode);
  EXPECT_EQ(received_pin_code(), kDefaultPinCode);

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_REJECTED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthRejected,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthRequestPinCodeRemoveDevice) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kRequestPinCode);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->HasPendingRequestPinCodeCallback());
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  FastForwardOperation(kTestDuration);
  // Simulate device no longer being available.
  ClearDevices();

  delegate->InvokePendingRequestPinCodeCallback(kDefaultPinCode);

  // Failure result should be returned.
  EXPECT_TRUE(received_pin_code().empty());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kNotFound,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthRequestPasskey) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kRequestPasskey);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->HasPendingRequestPasskeyCallback());
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  delegate->InvokePendingRequestPasskeyCallback(kDefaultPinCode);
  EXPECT_EQ(received_passkey(), kDefaultPinCodeNum);

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_INPROGRESS);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kInprogress,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthRequestPasskeyRemoveDevice) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kRequestPasskey);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->HasPendingRequestPasskeyCallback());
  EXPECT_TRUE(delegate->IsMojoPipeConnected());

  FastForwardOperation(kTestDuration);
  // Simulate device no longer being available.
  ClearDevices();

  delegate->InvokePendingRequestPasskeyCallback(kDefaultPinCode);

  // Failure result should be returned.
  EXPECT_EQ(received_passkey(), kUninitializedPasskey);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kNotFound,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthRequestPasskeyInvalidKey) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kRequestPasskey);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->HasPendingRequestPasskeyCallback());
  EXPECT_TRUE(delegate->IsMojoPipeConnected());

  FastForwardOperation(kTestDuration);
  delegate->InvokePendingRequestPasskeyCallback("hello_world");
  EXPECT_EQ(received_passkey(), kUninitializedPasskey);

  // CancelPairing() should be called since we had an invalid passkey.
  EXPECT_EQ(num_cancel_pairing_calls(), 1u);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");

  delegate->DisconnectMojoPipe();

  // CancelPairing() should not be called again since we already cancelled the
  // pairing.
  EXPECT_EQ(num_cancel_pairing_calls(), 1u);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthDisplayPinCode) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kDisplayPinCode);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(delegate->displayed_pin_code(), kDefaultPinCode);
  EXPECT_TRUE(delegate->key_entered_handler()->IsMojoPipeConnected());

  // Simulate device temporarily being connected during auth.
  ChangeDeviceIsConnected(device_id, /*is_connected=*/true);

  EnterKeys(device_id, /*num_keys_entered=*/0);
  EXPECT_EQ(delegate->key_entered_handler()->num_keys_entered(), 0);

  EnterKeys(device_id, /*num_keys_entered=*/6u);
  EXPECT_EQ(delegate->key_entered_handler()->num_keys_entered(), 6u);

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_FAILED);

  // Once the pairing has failed, the device will return to not connected.
  ChangeDeviceIsConnected(device_id, /*is_connected=*/false);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthDisplayPinCodeDisconnectHandler) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kDisplayPinCode);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(delegate->displayed_pin_code(), kDefaultPinCode);
  EXPECT_TRUE(delegate->key_entered_handler()->IsMojoPipeConnected());

  EnterKeys(device_id, /*num_keys_entered=*/0);
  EXPECT_EQ(delegate->key_entered_handler()->num_keys_entered(), 0);

  delegate->key_entered_handler()->DisconnectMojoPipe();
  FastForwardOperation(GetPairingFailureDelay());

  EnterKeys(device_id, /*num_keys_entered=*/6u);
  // Number of keys entered should still be zero since pipe is disconnected.
  EXPECT_EQ(delegate->key_entered_handler()->num_keys_entered(), 0);

  // Metrics is not recorded because pairing did not finish.
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/0, /*failure_count=*/0,
                         /*success_count=*/0);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/0,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthDisplayPasskey) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kDisplayPasskey);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(delegate->displayed_passkey(), kDefaultPinCode);
  EXPECT_TRUE(delegate->key_entered_handler()->IsMojoPipeConnected());

  EnterKeys(device_id, /*num_keys_entered=*/0);
  EXPECT_EQ(delegate->key_entered_handler()->num_keys_entered(), 0);

  EnterKeys(device_id, /*num_keys_entered=*/6u);
  EXPECT_EQ(delegate->key_entered_handler()->num_keys_entered(), 6u);

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_FAILED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthDisplayPasskeyPadZeroes) {
  std::string device_id1;
  AddDevice(&device_id1, AuthType::kDisplayPasskey, /*passkey=*/33333);

  std::unique_ptr<FakeDevicePairingDelegate> delegate1 = PairDevice(device_id1);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());

  // Passkey displayed should be a 6-digit number, padded with zeroes if needed.
  EXPECT_EQ(delegate1->displayed_passkey(), "033333");
  EXPECT_TRUE(delegate1->key_entered_handler()->IsMojoPipeConnected());

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_FAILED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");

  // Pair a new device.
  std::string device_id2;
  AddDevice(&device_id2, AuthType::kDisplayPasskey, /*passkey=*/0);

  std::unique_ptr<FakeDevicePairingDelegate> delegate2 = PairDevice(device_id2);
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());
  FastForwardOperation(GetPairingFailureDelay());

  // Passkey displayed should be a 6-digit number, padded with zeroes if needed.
  EXPECT_EQ(delegate2->displayed_passkey(), "000000");
  EXPECT_TRUE(delegate2->key_entered_handler()->IsMojoPipeConnected());
  // Expect value to be 1 since pairing was not finished.
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthConfirmPasskey) {
  std::string device_id1;
  AddDevice(&device_id1, AuthType::kConfirmPasskey);

  std::unique_ptr<FakeDevicePairingDelegate> delegate1 = PairDevice(device_id1);
  EXPECT_TRUE(delegate1->IsMojoPipeConnected());
  EXPECT_EQ(delegate1->passkey_to_confirm(), kDefaultPinCode);
  EXPECT_TRUE(delegate1->HasPendingConfirmPasskeyCallback());
  EXPECT_EQ(num_confirm_pairing_calls(), 0u);

  // Confirm the pairing.
  delegate1->InvokePendingConfirmPasskeyCallback(/*confirmed=*/true);
  EXPECT_EQ(num_confirm_pairing_calls(), 1u);

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_FAILED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");

  // Pair a new device.
  std::string device_id2;
  AddDevice(&device_id2, AuthType::kConfirmPasskey, /*passkey=*/0);

  std::unique_ptr<FakeDevicePairingDelegate> delegate2 = PairDevice(device_id2);
  EXPECT_TRUE(delegate2->IsMojoPipeConnected());

  // Passkey to confirm should be a 6-digit number, padded with zeroes if
  // needed.
  EXPECT_EQ(delegate2->passkey_to_confirm(), "000000");
  EXPECT_TRUE(delegate2->HasPendingConfirmPasskeyCallback());
  EXPECT_EQ(num_cancel_pairing_calls(), 0u);

  // Reject the pairing.
  delegate2->InvokePendingConfirmPasskeyCallback(/*confirmed=*/false);
  FastForwardOperation(GetPairingFailureDelay());

  // ConfirmPairing() should not be called again, CancelPairing() should.
  EXPECT_EQ(num_confirm_pairing_calls(), 1u);
  EXPECT_EQ(num_cancel_pairing_calls(), 1u);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/2, /*failure_count=*/2,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthCanceled,
                               /*failure_count=*/1, /*filtered_count=*/0);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/2,
                                /*transport_name=*/"Classic");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthConfirmPasskeyRemoveDevice) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kConfirmPasskey);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_EQ(delegate->passkey_to_confirm(), kDefaultPinCode);
  EXPECT_TRUE(delegate->HasPendingConfirmPasskeyCallback());

  // Simulate device no longer being available.
  ClearDevices();

  // Confirm the pairing.
  FastForwardOperation(kTestDuration);
  delegate->InvokePendingConfirmPasskeyCallback(/*confirmed=*/true);

  // Failure result should be returned.
  EXPECT_EQ(num_confirm_pairing_calls(), 0u);
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kNonAuthFailure);
  CheckPairingHistograms(device::BluetoothTransportType::kInvalid,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kNotFound,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(kTestDuration, /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Invalid");
}

TEST_F(DevicePairingHandlerImplTest, PairAuthAuthorizePairing) {
  std::string device_id;
  AddDevice(&device_id, AuthType::kAuthorizePairing);

  std::unique_ptr<FakeDevicePairingDelegate> delegate = PairDevice(device_id);
  EXPECT_TRUE(delegate->IsMojoPipeConnected());
  EXPECT_TRUE(delegate->HasPendingAuthorizePairingCallback());

  // Confirm the pairing.
  delegate->InvokePendingAuthorizePairingCallback(/*confirmed=*/true);
  EXPECT_EQ(num_confirm_pairing_calls(), 1u);

  InvokePendingConnectCallback(ConnectErrorCode::ERROR_AUTH_FAILED);
  FastForwardOperation(GetPairingFailureDelay());
  EXPECT_EQ(pairing_result(), mojom::PairingResult::kAuthFailed);
  CheckPairingHistograms(device::BluetoothTransportType::kClassic,
                         /*type_count=*/1, /*failure_count=*/1,
                         /*success_count=*/0);
  CheckPairingFailureHistogram(device::ConnectionFailureReason::kAuthFailed,
                               /*failure_count=*/1, /*filtered_count=*/1);
  CheckDurationHistogramMetrics(GetPairingFailureDelay(), /*success_count=*/0,
                                /*failure_count=*/1,
                                /*transport_name=*/"Classic");
}

}  // namespace ash::bluetooth_config
