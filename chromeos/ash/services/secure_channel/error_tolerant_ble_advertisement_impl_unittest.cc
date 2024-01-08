// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/error_tolerant_ble_advertisement_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/fake_ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const uint8_t kInvertedConnectionFlag = 0x01;
const char kDeviceId[] = "deviceId";
const char kLocalDeviceId[] = "localDeviceId";

std::unique_ptr<DataWithTimestamp> GenerateAdvertisementData() {
  return std::make_unique<DataWithTimestamp>("advertisement1", 1000L, 2000L);
}

}  // namespace

class SecureChannelErrorTolerantBleAdvertisementImplTest
    : public testing::Test {
 public:
  SecureChannelErrorTolerantBleAdvertisementImplTest(
      const SecureChannelErrorTolerantBleAdvertisementImplTest&) = delete;
  SecureChannelErrorTolerantBleAdvertisementImplTest& operator=(
      const SecureChannelErrorTolerantBleAdvertisementImplTest&) = delete;

 protected:
  SecureChannelErrorTolerantBleAdvertisementImplTest()
      : fake_advertisement_data_(GenerateAdvertisementData()) {}

  void SetUp() override {
    fake_advertisement_ = nullptr;
    stopped_callback_called_ = false;

    fake_synchronizer_ = std::make_unique<FakeBleSynchronizer>();

    advertisement_ = ErrorTolerantBleAdvertisementImpl::Factory::Create(
        DeviceIdPair(kDeviceId, kLocalDeviceId),
        std::make_unique<DataWithTimestamp>(*fake_advertisement_data_),
        fake_synchronizer_.get());

    VerifyServiceDataMatches(0u /* command_index */);
  }

  void VerifyServiceDataMatches(size_t command_index) {
    device::BluetoothAdvertisement::Data& data =
        fake_synchronizer_->GetAdvertisementData(command_index);

    // First, verify that the service UUID list is correct.
    std::optional<device::BluetoothAdvertisement::UUIDList> service_uuids =
        data.service_uuids();
    ASSERT_TRUE(service_uuids);
    EXPECT_EQ(1u, service_uuids->size());
    EXPECT_EQ(kAdvertisingServiceUuid, service_uuids->at(0));

    // Then, verify that the service data is correct.
    std::optional<device::BluetoothAdvertisement::ServiceData> service_data =
        data.service_data();
    ASSERT_TRUE(service_data);
    EXPECT_EQ(1u, service_data->size());

    ErrorTolerantBleAdvertisementImpl* derived_type =
        static_cast<ErrorTolerantBleAdvertisementImpl*>(advertisement_.get());
    std::vector<uint8_t> service_data_from_args =
        service_data->at(kAdvertisingServiceUuid);
    EXPECT_EQ(service_data_from_args.size(),
              derived_type->advertisement_data().data.size() + 1);
    EXPECT_FALSE(memcmp(derived_type->advertisement_data().data.data(),
                        service_data_from_args.data(), service_data->size()));
    EXPECT_EQ(kInvertedConnectionFlag, service_data_from_args.back());
  }

  void InvokeRegisterCallback(bool success, size_t command_index) {
    if (success) {
      fake_advertisement_ = new device::MockBluetoothAdvertisement();
      fake_synchronizer_->GetRegisterCallback(command_index)
          .Run(base::WrapRefCounted(fake_advertisement_.get()));
      return;
    }

    fake_synchronizer_->GetRegisterErrorCallback(command_index)
        .Run(device::BluetoothAdvertisement::ErrorCode::
                 INVALID_ADVERTISEMENT_ERROR_CODE);
  }

  void ReleaseAdvertisement() {
    ErrorTolerantBleAdvertisementImpl* derived_type =
        static_cast<ErrorTolerantBleAdvertisementImpl*>(advertisement_.get());
    derived_type->AdvertisementReleased(fake_advertisement_);
  }

  void CallStop() {
    advertisement_->Stop(base::BindOnce(
        &SecureChannelErrorTolerantBleAdvertisementImplTest::OnStopped,
        base::Unretained(this)));
  }

  void OnStopped() { stopped_callback_called_ = true; }

  void InvokeUnregisterCallback(bool success, size_t command_index) {
    if (success) {
      fake_synchronizer_->GetUnregisterCallback(command_index).Run();
      return;
    }

    fake_synchronizer_->GetUnregisterErrorCallback(command_index)
        .Run(device::BluetoothAdvertisement::ErrorCode::
                 INVALID_ADVERTISEMENT_ERROR_CODE);
  }

  const std::unique_ptr<DataWithTimestamp> fake_advertisement_data_;

  std::unique_ptr<FakeBleSynchronizer> fake_synchronizer_;

  raw_ptr<device::MockBluetoothAdvertisement, DanglingUntriaged>
      fake_advertisement_;

  bool stopped_callback_called_;

  std::unique_ptr<ErrorTolerantBleAdvertisement> advertisement_;
};

TEST_F(SecureChannelErrorTolerantBleAdvertisementImplTest,
       TestRegisterAndStop_Success) {
  InvokeRegisterCallback(true /* success */, 0u /* command_index */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  InvokeUnregisterCallback(true /* success */, 1u /* command_index */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_TRUE(stopped_callback_called_);
}

TEST_F(SecureChannelErrorTolerantBleAdvertisementImplTest,
       TestStoppedBeforeStarted) {
  // Before the advertisement has been started successfully.
  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Now, finish registering the advertisement.
  InvokeRegisterCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Successfully unregister.
  InvokeUnregisterCallback(true /* success */, 1u /* command_index */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_TRUE(stopped_callback_called_);
}

TEST_F(SecureChannelErrorTolerantBleAdvertisementImplTest,
       TestRegisterAndStop_Released) {
  InvokeRegisterCallback(true /* success */, 0u /* command_index */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Simulate that advertisement being released. A new one should be created.
  ReleaseAdvertisement();
  VerifyServiceDataMatches(1u /* command_index */);

  InvokeRegisterCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  InvokeUnregisterCallback(true /* success */, 2u /* command_index */);
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_TRUE(stopped_callback_called_);
}

TEST_F(SecureChannelErrorTolerantBleAdvertisementImplTest,
       TestRegisterAndStop_Errors) {
  // Fail to register.
  InvokeRegisterCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Fail to register again.
  InvokeRegisterCallback(false /* success */, 1u /* command_index */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // This time, succeed.
  InvokeRegisterCallback(true /* success */, 2u /* command_index */);
  EXPECT_FALSE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  CallStop();
  EXPECT_TRUE(advertisement_->HasBeenStopped());
  EXPECT_FALSE(stopped_callback_called_);

  // Fail to unregister.
  InvokeUnregisterCallback(false /* success */, 3u /* command_index */);
  EXPECT_FALSE(stopped_callback_called_);

  // Fail to unregister again.
  InvokeUnregisterCallback(false /* success */, 4u /* command_index */);
  EXPECT_FALSE(stopped_callback_called_);

  // This time, succeed.
  InvokeUnregisterCallback(true /* success */, 5u /* command_index */);
  EXPECT_TRUE(stopped_callback_called_);
}

}  // namespace ash::secure_channel
