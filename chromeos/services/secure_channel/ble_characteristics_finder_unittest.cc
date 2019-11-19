// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_characteristics_finder.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/background_eid_generator.h"
#include "chromeos/services/secure_channel/fake_background_eid_generator.h"
#include "chromeos/services/secure_channel/remote_attribute.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace chromeos {

namespace secure_channel {
namespace {

const char kDeviceName[] = "Device name";
const char kBluetoothAddress[] = "11:22:33:44:55:66";

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kToPeripheralCharUUID[] = "FBAE09F2-0482-11E5-8418-1697F925EC7B";
const char kFromPeripheralCharUUID[] = "5539ED10-0483-11E5-8418-1697F925EC7B";
const char kEidCharacteristicUUID[] = "f21843b0-9411-434b-b85f-a9b92bd69f77";

const char kToPeripheralCharID[] = "to peripheral id";
const char kFromPeripheralCharID[] = "from peripheral id";
const char kEidCharID[] = "eid id";

const char kServiceID[] = "service id";

const device::BluetoothRemoteGattCharacteristic::Properties
    kCharacteristicProperties =
        device::BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        device::BluetoothRemoteGattCharacteristic::
            PROPERTY_WRITE_WITHOUT_RESPONSE |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

const char kOtherCharUUID[] = "09731422-048A-11E5-8418-1697F925EC7B";
const char kOtherCharID[] = "other id";

const std::vector<uint8_t>& GetCorrectEidValue() {
  static const std::vector<uint8_t> kCorrectEidValue({0xAB, 0xCD});
  return kCorrectEidValue;
}

const std::vector<uint8_t>& GetIncorrectEidValue() {
  static const std::vector<uint8_t> kIncorrectEidValue({0xEF, 0xAB});
  return kIncorrectEidValue;
}

std::string EidToString(const std::vector<uint8_t>& eid_value_read) {
  std::string output;
  char* string_contents_ptr =
      base::WriteInto(&output, eid_value_read.size() + 1);
  memcpy(string_contents_ptr, eid_value_read.data(), eid_value_read.size());
  return output;
}

}  //  namespace

class SecureChannelBluetoothLowEnergyCharacteristicFinderTest
    : public testing::Test {
 protected:
  SecureChannelBluetoothLowEnergyCharacteristicFinderTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        success_callback_(base::Bind(
            &SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                OnCharacteristicsFound,
            base::Unretained(this))),
        error_callback_(base::Bind(
            &SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                OnCharacteristicsFinderError,
            base::Unretained(this))),
        device_(
            new NiceMock<device::MockBluetoothDevice>(adapter_.get(),
                                                      0,
                                                      kDeviceName,
                                                      kBluetoothAddress,
                                                      /* paired */ false,
                                                      /* connected */ false)),
        remote_service_({device::BluetoothUUID(kServiceUUID), ""}),
        to_peripheral_char_({device::BluetoothUUID(kToPeripheralCharUUID), ""}),
        from_peripheral_char_(
            {device::BluetoothUUID(kFromPeripheralCharUUID), ""}),
        remote_device_(multidevice::CreateRemoteDeviceRefForTest()) {
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    // The default behavior for |device_| is to have no services discovered. Can
    // be overrided later.
    ON_CALL(*device_, GetGattServices())
        .WillByDefault(
            Return(std::vector<device::BluetoothRemoteGattService*>()));
  }

  void SetUp() {
    EXPECT_CALL(*adapter_, AddObserver(_)).Times(AtLeast(1));
    EXPECT_CALL(*adapter_, RemoveObserver(_)).Times(AtLeast(1));
    characteristic_finder_ =
        std::make_unique<BluetoothLowEnergyCharacteristicsFinder>(
            adapter_, device_.get(), remote_service_, to_peripheral_char_,
            from_peripheral_char_, success_callback_, error_callback_,
            remote_device_, CreateBackgroundEidGenerator());
  }

  MOCK_METHOD3(OnCharacteristicsFound,
               void(const RemoteAttribute&,
                    const RemoteAttribute&,
                    const RemoteAttribute&));
  MOCK_METHOD0(OnCharacteristicsFinderError, void());

  std::unique_ptr<device::MockBluetoothGattCharacteristic>
  ExpectToFindCharacteristic(const device::BluetoothUUID& uuid,
                             const std::string& id,
                             bool valid = true) {
    std::unique_ptr<device::MockBluetoothGattCharacteristic> characteristic(
        new NiceMock<device::MockBluetoothGattCharacteristic>(
            /* service */ nullptr, id, uuid, /* is_local */ false,
            kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE));

    ON_CALL(*characteristic.get(), GetUUID()).WillByDefault(Return(uuid));
    if (valid)
      ON_CALL(*characteristic.get(), GetIdentifier()).WillByDefault(Return(id));
    return characteristic;
  }

  std::unique_ptr<device::MockBluetoothGattCharacteristic>
  ExpectEidCharacteristic(const std::string& id,
                          bool read_success,
                          bool correct_eid) {
    std::unique_ptr<device::MockBluetoothGattCharacteristic> characteristic =
        ExpectToFindCharacteristic(
            device::BluetoothUUID(kEidCharacteristicUUID), id);

    // Posting to a task to allow the read to be asynchronous, although still
    // running only on one thread. Calls to
    // |task_environment_.RunUntilIdle()| in tests will process any
    // pending callbacks.
    ON_CALL(*characteristic.get(), ReadRemoteCharacteristic_(_, _))
        .WillByDefault(
            Invoke([read_success, correct_eid](
                       device::BluetoothRemoteGattCharacteristic::ValueCallback&
                           callback,
                       device::BluetoothRemoteGattCharacteristic::ErrorCallback&
                           error_callback) {
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE,
                  read_success
                      ? base::BindOnce(std::move(callback),
                                       correct_eid ? GetCorrectEidValue()
                                                   : GetIncorrectEidValue())
                      : base::BindOnce(
                            std::move(error_callback),
                            device::BluetoothGattService::GATT_ERROR_FAILED));
            }));
    return characteristic;
  }

  device::MockBluetoothGattService* SetUpServiceWithCharacteristics(
      const std::string& service_id,
      std::vector<device::MockBluetoothGattCharacteristic*> characteristics,
      bool is_discovery_complete) {
    auto service = std::make_unique<NiceMock<device::MockBluetoothGattService>>(
        device_.get(), service_id, device::BluetoothUUID(kServiceUUID),
        /* is_primary */ true, /* is_local */ false);
    device::MockBluetoothGattService* service_ptr = service.get();
    services_.push_back(std::move(service));
    ON_CALL(*device_, GetGattServices())
        .WillByDefault(Return(GetRawServiceList()));
    ON_CALL(*device_, IsGattServicesDiscoveryComplete())
        .WillByDefault(Return(is_discovery_complete));

    ON_CALL(*device_, GetGattService(service_id))
        .WillByDefault(Return(service_ptr));

    for (auto* characteristic : characteristics) {
      std::vector<device::BluetoothRemoteGattCharacteristic*> chars_for_uuid{
          characteristic};
      ON_CALL(*service_ptr, GetCharacteristicsByUUID(characteristic->GetUUID()))
          .WillByDefault(Return(chars_for_uuid));
      ON_CALL(*characteristic, GetService()).WillByDefault(Return(service_ptr));
    }

    return service_ptr;
  }

  device::MockBluetoothGattService* SetUpServiceWithIds(
      const std::string& service_id,
      const std::string& from_char_id,
      const std::string& to_char_id,
      const std::string& eid_char_id = std::string(),
      bool correct_eid_value = true) {
    std::unique_ptr<device::MockBluetoothGattCharacteristic> from_char =
        ExpectToFindCharacteristic(
            device::BluetoothUUID(kFromPeripheralCharUUID), from_char_id);
    std::unique_ptr<device::MockBluetoothGattCharacteristic> to_char =
        ExpectToFindCharacteristic(device::BluetoothUUID(kToPeripheralCharUUID),
                                   to_char_id);
    std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
        from_char.get(), to_char.get()};
    all_mock_characteristics_.push_back(std::move(from_char));
    all_mock_characteristics_.push_back(std::move(to_char));

    if (!eid_char_id.empty()) {
      std::unique_ptr<device::MockBluetoothGattCharacteristic> eid_char =
          ExpectEidCharacteristic(eid_char_id, /* read_success */ true,
                                  correct_eid_value);
      characteristics.push_back(eid_char.get());
      all_mock_characteristics_.push_back(std::move(eid_char));
    }

    return SetUpServiceWithCharacteristics(service_id, characteristics, false);
  }

  std::unique_ptr<BackgroundEidGenerator> CreateBackgroundEidGenerator() {
    auto fake_background_eid_generator =
        std::make_unique<FakeBackgroundEidGenerator>();
    fake_background_eid_generator_ = fake_background_eid_generator.get();
    fake_background_eid_generator_->set_identified_device_id(
        remote_device_.GetDeviceId());
    fake_background_eid_generator_->set_matching_service_data(
        EidToString(GetCorrectEidValue()));
    return fake_background_eid_generator;
  }

  std::vector<device::BluetoothRemoteGattService*> GetRawServiceList() {
    std::vector<device::BluetoothRemoteGattService*> service_list_raw;
    std::transform(services_.begin(), services_.end(),
                   std::back_inserter(service_list_raw),
                   [](auto& service) { return service.get(); });
    return service_list_raw;
  }

  void CallGattServicesDiscovered() {
    characteristic_finder_->GattServicesDiscovered(adapter_.get(),
                                                   device_.get());
  }

  std::unique_ptr<BluetoothLowEnergyCharacteristicsFinder>
      characteristic_finder_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  BluetoothLowEnergyCharacteristicsFinder::SuccessCallback success_callback_;
  BluetoothLowEnergyCharacteristicsFinder::ErrorCallback error_callback_;
  std::unique_ptr<device::MockBluetoothDevice> device_;
  std::vector<std::unique_ptr<device::BluetoothRemoteGattService>> services_;
  std::vector<std::unique_ptr<device::MockBluetoothGattCharacteristic>>
      all_mock_characteristics_;
  FakeBackgroundEidGenerator* fake_background_eid_generator_;
  RemoteAttribute remote_service_;
  RemoteAttribute to_peripheral_char_;
  RemoteAttribute from_peripheral_char_;
  multidevice::RemoteDeviceRef remote_device_;
};

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       ConstructAndDestroyDontCrash) {
  std::make_unique<BluetoothLowEnergyCharacteristicsFinder>(
      adapter_, device_.get(), remote_service_, to_peripheral_char_,
      from_peripheral_char_, success_callback_, error_callback_, remote_device_,
      CreateBackgroundEidGenerator());
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       FindRightCharacteristics) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID);

  CallGattServicesDiscovered();

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

// Tests that CharacteristicFinder ignores events for other devices.
TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       FindRightCharacteristicsWrongDevice) {
  // Make CharacteristicFinder which is supposed to listen for other device.
  std::unique_ptr<device::BluetoothDevice> device(
      new NiceMock<device::MockBluetoothDevice>(
          adapter_.get(), 0, kDeviceName, kBluetoothAddress,
          /* connected */ false, /* paired */ false));
  BluetoothLowEnergyCharacteristicsFinder characteristic_finder(
      adapter_, device.get(), remote_service_, to_peripheral_char_,
      from_peripheral_char_, success_callback_, error_callback_, remote_device_,
      CreateBackgroundEidGenerator());
  device::BluetoothAdapter::Observer* observer =
      static_cast<device::BluetoothAdapter::Observer*>(&characteristic_finder);

  RemoteAttribute found_to_char, found_from_char;
  // These shouldn't be called at all since the GATT events below are for other
  // devices.
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  std::unique_ptr<device::MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::unique_ptr<device::MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);

  std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
      from_char.get(), to_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ false);

  observer->GattServicesDiscovered(adapter_.get(), device_.get());
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       DidntFindRightCharacteristics) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  std::unique_ptr<device::MockBluetoothGattCharacteristic> other_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kOtherCharUUID),
                                 kOtherCharID, /* valid */ false);
  std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
      other_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ false);

  CallGattServicesDiscovered();
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       DidntFindRightCharacteristicsNorService) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  CallGattServicesDiscovered();
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       FindOnlyOneRightCharacteristic) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  std::unique_ptr<device::MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
      from_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ true);

  CallGattServicesDiscovered();
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       FindWrongCharacteristic_FindRightCharacteristics) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  std::unique_ptr<device::MockBluetoothGattCharacteristic> other_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kOtherCharUUID),
                                 kOtherCharID, /* valid */ false);
  std::unique_ptr<device::MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::unique_ptr<device::MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);
  std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
      other_char.get(), from_char.get(), to_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ false);

  CallGattServicesDiscovered();

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       RightCharacteristicsAlreadyPresent) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  std::unique_ptr<device::MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);

  std::unique_ptr<device::MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);

  std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
      from_char.get(), to_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ true);

  std::make_unique<BluetoothLowEnergyCharacteristicsFinder>(
      adapter_, device_.get(), remote_service_, to_peripheral_char_,
      from_peripheral_char_, success_callback_, error_callback_, remote_device_,
      CreateBackgroundEidGenerator());

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       OneServiceWithRightEidCharacteristic) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID,
                      kEidCharID);

  CallGattServicesDiscovered();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       OneServiceWithFailedEidCharacteristicRead) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  std::unique_ptr<device::MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::unique_ptr<device::MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(device::BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);
  // NOTE: Explicitly passing false for read_success so that the GATT read
  // fails.
  std::unique_ptr<device::MockBluetoothGattCharacteristic> eid_char =
      ExpectEidCharacteristic(kEidCharID, /* read_success */ false,
                              /* correct_eid */ true);

  std::vector<device::MockBluetoothGattCharacteristic*> characteristics{
      from_char.get(), to_char.get(), eid_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ false);

  CallGattServicesDiscovered();
  task_environment_.RunUntilIdle();
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       OneServiceWithWrongEidCharacteristic) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID,
                      kEidCharID, /* correct_eid_value */ false);

  CallGattServicesDiscovered();
  task_environment_.RunUntilIdle();
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       TwoServicesWithWrongEidCharacteristics) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID,
                      kEidCharID, /* correct_eid_value */ false);
  SetUpServiceWithIds("service_id_2", "from_id_2", "to_id_2", "eid_id_2",
                      /* correct_eid_value */ false);

  CallGattServicesDiscovered();
  task_environment_.RunUntilIdle();
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       TwoServicesAndOneHasRightEidCharacteristic) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID,
                      kEidCharID);
  SetUpServiceWithIds("service_id_2", "from_id_2", "to_id_2", "eid_id_2",
                      /* correct_eid_value */ false);

  CallGattServicesDiscovered();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       TwoServicesWithNoEidCharacteristics) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID);
  SetUpServiceWithIds("service_id_2", "from_id_2", "to_id_2");

  CallGattServicesDiscovered();

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       FourServicesAndOneHasRightEidCharacteristic) {
  RemoteAttribute found_to_char, found_from_char;
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&found_to_char), SaveArg<2>(&found_from_char)));
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  SetUpServiceWithIds("service_id_1", "from_id_1", "to_id_1", "eid_id_1",
                      /* correct_eid_value */ false);
  SetUpServiceWithIds("service_id_2", "from_id_2", "to_id_2", "eid_id_2",
                      /* correct_eid_value */ false);
  SetUpServiceWithIds(kServiceID, kFromPeripheralCharID, kToPeripheralCharID,
                      kEidCharID, /* correct_eid_value */ true);
  SetUpServiceWithIds("service_id_3", "from_id_3", "to_id_3", "eid_id_3",
                      /* correct_eid_value */ false);

  CallGattServicesDiscovered();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(kToPeripheralCharID, found_to_char.id);
  EXPECT_EQ(kFromPeripheralCharID, found_from_char.id);
}

}  // namespace secure_channel

}  // namespace chromeos
