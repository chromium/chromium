// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_characteristics_finder.h"

#include <memory>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/background_eid_generator.h"
#include "chromeos/ash/services/secure_channel/fake_background_eid_generator.h"
#include "chromeos/ash/services/secure_channel/remote_attribute.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothAdapterFactory;
using ::device::BluetoothDevice;
using ::device::BluetoothGattService;
using ::device::BluetoothRemoteGattCharacteristic;
using ::device::BluetoothRemoteGattService;
using ::device::BluetoothUUID;
using ::device::MockBluetoothAdapter;
using ::device::MockBluetoothDevice;
using ::device::MockBluetoothGattCharacteristic;
using ::device::MockBluetoothGattService;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

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

const BluetoothRemoteGattCharacteristic::Properties kCharacteristicProperties =
    BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
    BluetoothRemoteGattCharacteristic::PROPERTY_READ |
    BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE |
    BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

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
 public:
  MOCK_METHOD3(OnCharacteristicsFound,
               void(const RemoteAttribute&,
                    const RemoteAttribute&,
                    const RemoteAttribute&));
  MOCK_METHOD0(OnCharacteristicsFinderError, void());

 protected:
  SecureChannelBluetoothLowEnergyCharacteristicFinderTest()
      : adapter_(new NiceMock<MockBluetoothAdapter>),
        device_(new NiceMock<MockBluetoothDevice>(adapter_.get(),
                                                  0,
                                                  kDeviceName,
                                                  kBluetoothAddress,
                                                  /* paired */ false,
                                                  /* connected */ false)),
        remote_service_({BluetoothUUID(kServiceUUID), ""}),
        to_peripheral_char_({BluetoothUUID(kToPeripheralCharUUID), ""}),
        from_peripheral_char_({BluetoothUUID(kFromPeripheralCharUUID), ""}),
        remote_device_(multidevice::CreateRemoteDeviceRefForTest()) {
    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    // The default behavior for |device_| is to have no services discovered. Can
    // be overrided later.
    ON_CALL(*device_, GetGattServices())
        .WillByDefault(Return(std::vector<BluetoothRemoteGattService*>()));
  }

  void SetUp() override {
    EXPECT_CALL(*adapter_, AddObserver(_)).Times(AtLeast(1));
    EXPECT_CALL(*adapter_, RemoveObserver(_)).Times(AtLeast(1));

    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

    characteristic_finder_ =
        std::make_unique<BluetoothLowEnergyCharacteristicsFinder>(
            adapter_, device_.get(), remote_service_, to_peripheral_char_,
            from_peripheral_char_,
            base::BindOnce(
                &SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                    OnCharacteristicsFound,
                base::Unretained(this)),
            base::BindOnce(
                &SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                    OnCharacteristicsFinderError,
                base::Unretained(this)),
            remote_device_, CreateBackgroundEidGenerator(), test_task_runner);

    test_task_runner->RunUntilIdle();
  }

  std::unique_ptr<MockBluetoothGattCharacteristic> ExpectToFindCharacteristic(
      const BluetoothUUID& uuid,
      const std::string& id,
      bool valid = true) {
    std::unique_ptr<MockBluetoothGattCharacteristic> characteristic(
        new NiceMock<MockBluetoothGattCharacteristic>(
            /*service=*/nullptr, id, uuid, kCharacteristicProperties,
            BluetoothRemoteGattCharacteristic::PERMISSION_NONE));

    ON_CALL(*characteristic.get(), GetUUID()).WillByDefault(Return(uuid));
    if (valid)
      ON_CALL(*characteristic.get(), GetIdentifier()).WillByDefault(Return(id));
    return characteristic;
  }

  std::unique_ptr<MockBluetoothGattCharacteristic> ExpectEidCharacteristic(
      const std::string& id,
      bool read_success,
      bool correct_eid) {
    std::unique_ptr<MockBluetoothGattCharacteristic> characteristic =
        ExpectToFindCharacteristic(BluetoothUUID(kEidCharacteristicUUID), id);

    // Posting to a task to allow the read to be asynchronous, although still
    // running only on one thread. Calls to
    // |task_environment_.RunUntilIdle()| in tests will process any
    // pending callbacks.
    ON_CALL(*characteristic.get(), ReadRemoteCharacteristic_(_))
        .WillByDefault(Invoke(
            [read_success, correct_eid](
                BluetoothRemoteGattCharacteristic::ValueCallback& callback) {
              std::optional<BluetoothGattService::GattErrorCode> error_code;
              std::vector<uint8_t> value;
              if (read_success) {
                error_code = std::nullopt;
                value =
                    correct_eid ? GetCorrectEidValue() : GetIncorrectEidValue();
              } else {
                error_code =
                    device::BluetoothGattService::GattErrorCode::kFailed;
              }
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), error_code, value));
            }));
    return characteristic;
  }

  MockBluetoothGattService* SetUpServiceWithCharacteristics(
      const std::string& service_id,
      std::vector<MockBluetoothGattCharacteristic*> characteristics,
      bool is_discovery_complete) {
    auto service = std::make_unique<NiceMock<MockBluetoothGattService>>(
        device_.get(), service_id, BluetoothUUID(kServiceUUID),
        /*is_primary=*/true);
    MockBluetoothGattService* service_ptr = service.get();
    services_.push_back(std::move(service));
    ON_CALL(*device_, GetGattServices())
        .WillByDefault(Return(GetRawServiceList()));
    ON_CALL(*device_, IsGattServicesDiscoveryComplete())
        .WillByDefault(Return(is_discovery_complete));

    ON_CALL(*device_, GetGattService(service_id))
        .WillByDefault(Return(service_ptr));

    for (auto* characteristic : characteristics) {
      std::vector<BluetoothRemoteGattCharacteristic*> chars_for_uuid{
          characteristic};
      ON_CALL(*service_ptr, GetCharacteristicsByUUID(characteristic->GetUUID()))
          .WillByDefault(Return(chars_for_uuid));
      ON_CALL(*characteristic, GetService()).WillByDefault(Return(service_ptr));
    }

    return service_ptr;
  }

  MockBluetoothGattService* SetUpServiceWithIds(
      const std::string& service_id,
      const std::string& from_char_id,
      const std::string& to_char_id,
      const std::string& eid_char_id = std::string(),
      bool correct_eid_value = true) {
    std::unique_ptr<MockBluetoothGattCharacteristic> from_char =
        ExpectToFindCharacteristic(BluetoothUUID(kFromPeripheralCharUUID),
                                   from_char_id);
    std::unique_ptr<MockBluetoothGattCharacteristic> to_char =
        ExpectToFindCharacteristic(BluetoothUUID(kToPeripheralCharUUID),
                                   to_char_id);
    std::vector<MockBluetoothGattCharacteristic*> characteristics{
        from_char.get(), to_char.get()};
    all_mock_characteristics_.push_back(std::move(from_char));
    all_mock_characteristics_.push_back(std::move(to_char));

    if (!eid_char_id.empty()) {
      std::unique_ptr<MockBluetoothGattCharacteristic> eid_char =
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

  std::vector<BluetoothRemoteGattService*> GetRawServiceList() {
    return base::ToVector(services_,
                          &std::unique_ptr<BluetoothRemoteGattService>::get);
  }

  void CallGattServicesDiscovered() {
    characteristic_finder_->GattServicesDiscovered(adapter_.get(),
                                                   device_.get());
  }

  std::unique_ptr<BluetoothLowEnergyCharacteristicsFinder>
      characteristic_finder_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockBluetoothAdapter> adapter_;
  std::unique_ptr<MockBluetoothDevice> device_;
  std::vector<std::unique_ptr<BluetoothRemoteGattService>> services_;
  std::vector<std::unique_ptr<MockBluetoothGattCharacteristic>>
      all_mock_characteristics_;
  raw_ptr<FakeBackgroundEidGenerator, DanglingUntriaged>
      fake_background_eid_generator_;
  RemoteAttribute remote_service_;
  RemoteAttribute to_peripheral_char_;
  RemoteAttribute from_peripheral_char_;
  multidevice::RemoteDeviceRef remote_device_;
};

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       ConstructAndDestroyDontCrash) {
  std::make_unique<BluetoothLowEnergyCharacteristicsFinder>(
      adapter_, device_.get(), remote_service_, to_peripheral_char_,
      from_peripheral_char_,
      base::BindOnce(&SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                         OnCharacteristicsFound,
                     base::Unretained(this)),
      base::BindOnce(&SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                         OnCharacteristicsFinderError,
                     base::Unretained(this)),
      remote_device_, CreateBackgroundEidGenerator());
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
  std::unique_ptr<BluetoothDevice> device(new NiceMock<MockBluetoothDevice>(
      adapter_.get(), 0, kDeviceName, kBluetoothAddress,
      /* connected */ false, /* paired */ false));
  BluetoothLowEnergyCharacteristicsFinder characteristic_finder(
      adapter_, device.get(), remote_service_, to_peripheral_char_,
      from_peripheral_char_,
      base::BindOnce(&SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                         OnCharacteristicsFound,
                     base::Unretained(this)),
      base::BindOnce(&SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                         OnCharacteristicsFinderError,
                     base::Unretained(this)),
      remote_device_, CreateBackgroundEidGenerator());
  BluetoothAdapter::Observer* observer =
      static_cast<BluetoothAdapter::Observer*>(&characteristic_finder);

  RemoteAttribute found_to_char, found_from_char;
  // These shouldn't be called at all since the GATT events below are for other
  // devices.
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError()).Times(0);

  std::unique_ptr<MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::unique_ptr<MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);

  std::vector<MockBluetoothGattCharacteristic*> characteristics{from_char.get(),
                                                                to_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ false);

  observer->GattServicesDiscovered(adapter_.get(), device_.get());
}

TEST_F(SecureChannelBluetoothLowEnergyCharacteristicFinderTest,
       DidntFindRightCharacteristics) {
  EXPECT_CALL(*this, OnCharacteristicsFound(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnCharacteristicsFinderError());

  std::unique_ptr<MockBluetoothGattCharacteristic> other_char =
      ExpectToFindCharacteristic(BluetoothUUID(kOtherCharUUID), kOtherCharID,
                                 /* valid */ false);
  std::vector<MockBluetoothGattCharacteristic*> characteristics{
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

  std::unique_ptr<MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::vector<MockBluetoothGattCharacteristic*> characteristics{
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

  std::unique_ptr<MockBluetoothGattCharacteristic> other_char =
      ExpectToFindCharacteristic(BluetoothUUID(kOtherCharUUID), kOtherCharID,
                                 /* valid */ false);
  std::unique_ptr<MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::unique_ptr<MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);
  std::vector<MockBluetoothGattCharacteristic*> characteristics{
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

  std::unique_ptr<MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);

  std::unique_ptr<MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);

  std::vector<MockBluetoothGattCharacteristic*> characteristics{from_char.get(),
                                                                to_char.get()};
  SetUpServiceWithCharacteristics(kServiceID, characteristics,
                                  /* is_discovery_complete */ true);

  auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  auto finder = std::make_unique<BluetoothLowEnergyCharacteristicsFinder>(
      adapter_, device_.get(), remote_service_, to_peripheral_char_,
      from_peripheral_char_,
      base::BindOnce(&SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                         OnCharacteristicsFound,
                     base::Unretained(this)),
      base::BindOnce(&SecureChannelBluetoothLowEnergyCharacteristicFinderTest::
                         OnCharacteristicsFinderError,
                     base::Unretained(this)),
      remote_device_, CreateBackgroundEidGenerator(), test_task_runner);

  test_task_runner->RunUntilIdle();

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

  std::unique_ptr<MockBluetoothGattCharacteristic> from_char =
      ExpectToFindCharacteristic(BluetoothUUID(kFromPeripheralCharUUID),
                                 kFromPeripheralCharID);
  std::unique_ptr<MockBluetoothGattCharacteristic> to_char =
      ExpectToFindCharacteristic(BluetoothUUID(kToPeripheralCharUUID),
                                 kToPeripheralCharID);
  // NOTE: Explicitly passing false for read_success so that the GATT read
  // fails.
  std::unique_ptr<MockBluetoothGattCharacteristic> eid_char =
      ExpectEidCharacteristic(kEidCharID, /* read_success */ false,
                              /* correct_eid */ true);

  std::vector<MockBluetoothGattCharacteristic*> characteristics{
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

}  // namespace ash::secure_channel
