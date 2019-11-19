// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/bluetooth_mojom_traits.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUuidStr[] = "12345678-1234-5678-9abc-def123456789";
constexpr uint8_t kUuidArray[] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34,
                                  0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1,
                                  0x23, 0x45, 0x67, 0x89};
constexpr size_t kUuidSize = 16;

constexpr char kUuid16Str[] = "1234";
constexpr uint16_t kUuid16 = 0x1234;
constexpr uint8_t kServiceData[] = {0x11, 0x22, 0x33, 0x44, 0x55};
constexpr uint8_t kManufacturerData[] = {0x00, 0xe0};

template <typename MojoType, typename UserType>
mojo::StructPtr<MojoType> ConvertToMojo(UserType* input) {
  std::vector<uint8_t> data = MojoType::Serialize(input);
  mojo::StructPtr<MojoType> output;
  MojoType::Deserialize(std::move(data), &output);
  return output;
}

template <typename MojoType, typename UserType>
bool ConvertFromMojo(mojo::StructPtr<MojoType> input, UserType* output) {
  std::vector<uint8_t> data = MojoType::Serialize(&input);
  return MojoType::Deserialize(std::move(data), output);
}

}  // namespace

namespace mojo {

TEST(BluetoothStructTraitsTest, SerializeBluetoothUUID) {
  device::BluetoothUUID uuid_device(kUuidStr);
  arc::mojom::BluetoothUUIDPtr uuid_mojo =
      ConvertToMojo<arc::mojom::BluetoothUUID>(&uuid_device);
  EXPECT_EQ(kUuidSize, uuid_mojo->uuid.size());
  for (size_t i = 0; i < kUuidSize; i++) {
    EXPECT_EQ(kUuidArray[i], uuid_mojo->uuid[i]);
  }
}

TEST(BluetoothStructTraitsTest, DeserializeBluetoothUUID) {
  arc::mojom::BluetoothUUIDPtr uuid_mojo = arc::mojom::BluetoothUUID::New();
  for (size_t i = 0; i < kUuidSize; i++) {
    uuid_mojo->uuid.push_back(kUuidArray[i]);
  }
  uuid_mojo->uuid =
      std::vector<uint8_t>(std::begin(kUuidArray), std::end(kUuidArray));
  device::BluetoothUUID uuid_device;
  EXPECT_TRUE(ConvertFromMojo(std::move(uuid_mojo), &uuid_device));
  EXPECT_EQ(std::string(kUuidStr), uuid_device.canonical_value());

  // Size checks are performed in serialization code. UUIDs with a length
  // other than 16 bytes will not make it through the mojo deserialization
  // code since arc::mojom::BluetoothUUID::uuid is defined as
  // array<uint8, 16>.
}

TEST(BluetoothStructTraitsTest, DeserializeBluetoothAdvertisement) {
  arc::mojom::BluetoothAdvertisementPtr advertisement_mojo =
      arc::mojom::BluetoothAdvertisement::New();
  std::vector<arc::mojom::BluetoothAdvertisingDataPtr> adv_data;

  // Create 16bit service UUIDs.
  arc::mojom::BluetoothAdvertisingDataPtr data =
      arc::mojom::BluetoothAdvertisingData::New();
  data->set_service_uuids_16({kUuid16});
  adv_data.push_back(std::move(data));

  // Create service UUIDs.
  data = arc::mojom::BluetoothAdvertisingData::New();
  std::vector<device::BluetoothUUID> service_uuids;
  service_uuids.push_back((device::BluetoothUUID(kUuidStr)));
  data->set_service_uuids(service_uuids);
  adv_data.push_back(std::move(data));

  // Create service data.
  data = arc::mojom::BluetoothAdvertisingData::New();
  arc::mojom::BluetoothServiceDataPtr service_data =
      arc::mojom::BluetoothServiceData::New();
  service_data->uuid_16bit = kUuid16;
  service_data->data =
      std::vector<uint8_t>(std::begin(kServiceData), std::end(kServiceData));
  data->set_service_data(std::move(service_data));
  adv_data.push_back(std::move(data));

  // Create manufacturer data.
  data = arc::mojom::BluetoothAdvertisingData::New();
  data->set_manufacturer_data(std::vector<uint8_t>(
      std::begin(kManufacturerData), std::end(kManufacturerData)));
  adv_data.push_back(std::move(data));

  advertisement_mojo->type =
      arc::mojom::BluetoothAdvertisementType::ADV_TYPE_CONNECTABLE;
  advertisement_mojo->data = std::move(adv_data);

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement;
  EXPECT_TRUE(ConvertFromMojo(std::move(advertisement_mojo), &advertisement));

  EXPECT_EQ(advertisement->type(),
            device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_PERIPHERAL);

  std::unique_ptr<device::BluetoothAdvertisement::UUIDList> converted_uuids =
      advertisement->service_uuids();
  EXPECT_EQ(converted_uuids->size(), 2U);
  EXPECT_EQ(converted_uuids->at(0), kUuid16Str);
  EXPECT_EQ(converted_uuids->at(1), kUuidStr);

  std::unique_ptr<device::BluetoothAdvertisement::ServiceData>
      converted_service = advertisement->service_data();
  EXPECT_EQ(converted_service->size(), 1U);
  EXPECT_EQ(converted_service->begin()->first, kUuid16Str);
  for (size_t i = 0; i < base::size(kServiceData); i++) {
    EXPECT_EQ(kServiceData[i], converted_service->begin()->second[i]);
  }

  std::unique_ptr<device::BluetoothAdvertisement::ManufacturerData>
      converted_manufacturer = advertisement->manufacturer_data();
  EXPECT_EQ(converted_manufacturer->size(), 1U);
  uint16_t cic = converted_manufacturer->begin()->first;
  EXPECT_EQ(cic & 0xff, kManufacturerData[0]);
  EXPECT_EQ((cic >> 8) & 0xff, kManufacturerData[1]);
  EXPECT_EQ(converted_manufacturer->begin()->second.size(),
            base::size(kManufacturerData) - sizeof(uint16_t));
}

TEST(BluetoothStructTraitsTest, DeserializeBluetoothAdvertisementFailure) {
  arc::mojom::BluetoothAdvertisementPtr advertisement_mojo =
      arc::mojom::BluetoothAdvertisement::New();
  std::vector<arc::mojom::BluetoothAdvertisingDataPtr> adv_data;

  // Create empty manufacturer data. Manufacturer data must include the CIC
  // which is 2 bytes long.
  arc::mojom::BluetoothAdvertisingDataPtr data =
      arc::mojom::BluetoothAdvertisingData::New();
  data->set_manufacturer_data(std::vector<uint8_t>());
  adv_data.push_back(std::move(data));

  advertisement_mojo->type =
      arc::mojom::BluetoothAdvertisementType::ADV_TYPE_CONNECTABLE;
  advertisement_mojo->data = std::move(adv_data);

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement;
  EXPECT_FALSE(ConvertFromMojo(std::move(advertisement_mojo), &advertisement));
}

}  // namespace mojo
