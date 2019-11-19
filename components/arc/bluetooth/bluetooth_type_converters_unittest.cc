// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/bluetooth_type_converters.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAddressStr[] = "1A:2B:3C:4D:5E:6F";
constexpr char kInvalidAddressStr[] = "00:00:00:00:00:00";
constexpr uint8_t kAddressArray[] = {0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f};
constexpr size_t kAddressSize = 6;
constexpr uint8_t kFillerByte = 0x79;

arc::mojom::BluetoothSdpAttributePtr CreateDeepMojoSequenceAttribute(
    size_t depth) {
  auto value = arc::mojom::BluetoothSdpAttribute::New();

  if (depth > 0u) {
    value->type = bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE;
    value->value = base::Value();
    value->sequence.push_back(CreateDeepMojoSequenceAttribute(depth - 1));
    value->type_size = static_cast<uint32_t>(value->sequence.size());
  } else {
    uint16_t data = 3;
    value->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
    value->type_size = static_cast<uint32_t>(sizeof(data));
    value->value = base::Value(data);
  }
  return value;
}

size_t GetDepthOfMojoAttribute(
    const arc::mojom::BluetoothSdpAttributePtr& attribute) {
  size_t depth = 1;
  if (attribute->type == bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE) {
    for (const auto& value : attribute->sequence)
      depth = std::max(depth, GetDepthOfMojoAttribute(value) + 1);
  }
  return depth;
}

bluez::BluetoothServiceAttributeValueBlueZ CreateDeepBlueZSequenceAttribute(
    size_t depth) {
  if (depth > 0u) {
    auto sequence = std::make_unique<
        bluez::BluetoothServiceAttributeValueBlueZ::Sequence>();
    sequence->push_back(CreateDeepBlueZSequenceAttribute(depth - 1));

    return bluez::BluetoothServiceAttributeValueBlueZ(std::move(sequence));
  } else {
    return bluez::BluetoothServiceAttributeValueBlueZ(
        bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(uint16_t),
        std::make_unique<base::Value>(3));
  }
}

size_t GetDepthOfBlueZAttribute(
    const bluez::BluetoothServiceAttributeValueBlueZ& attribute) {
  size_t depth = 1;
  if (attribute.type() ==
      bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE) {
    for (const auto& value : attribute.sequence())
      depth = std::max(depth, GetDepthOfBlueZAttribute(value) + 1);
  }
  return depth;
}

}  // namespace

namespace mojo {

TEST(BluetoothTypeConverterTest, ConvertMojoBluetoothAddressFromString) {
  arc::mojom::BluetoothAddressPtr address_mojo =
      arc::mojom::BluetoothAddress::From(std::string(kAddressStr));
  EXPECT_EQ(kAddressSize, address_mojo->address.size());
  for (size_t i = 0; i < kAddressSize; i++)
    EXPECT_EQ(kAddressArray[i], address_mojo->address[i]);
}

TEST(BluetoothTypeConverterTest, ConvertMojoBluetoothAddressToString) {
  arc::mojom::BluetoothAddressPtr address_mojo =
      arc::mojom::BluetoothAddress::New();
  // Test address is shorter than expected (invalid address).
  for (size_t i = 0; i < kAddressSize - 1; i++)
    address_mojo->address.push_back(kAddressArray[i]);
  EXPECT_EQ(kInvalidAddressStr, address_mojo->To<std::string>());

  // Test success case.
  address_mojo->address.push_back(kAddressArray[kAddressSize - 1]);
  EXPECT_EQ(kAddressStr, address_mojo->To<std::string>());

  // Test address is longer than expected (invalid address).
  address_mojo->address.push_back(kFillerByte);
  EXPECT_EQ(kInvalidAddressStr, address_mojo->To<std::string>());
}

TEST(BluetoothTypeConverterTest,
     ConvertMojoValueAttributeToBlueZAttribute_NullType) {
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE;
  mojo->type_size = 0;

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            blue_z.type());
  EXPECT_EQ(0u, blue_z.size());
  EXPECT_EQ(base::Value::Type::NONE, blue_z.value().type());
}

TEST(BluetoothTypeConverterTest,
     ConvertMojoValueAttributeToBlueZAttribute_BoolType) {
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::BOOL;
  mojo->type_size = static_cast<uint32_t>(sizeof(bool));
  mojo->value = base::Value(true);

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::BOOL, blue_z.type());
  EXPECT_EQ(sizeof(bool), blue_z.size());
  ASSERT_EQ(base::Value::Type::BOOLEAN, blue_z.value().type());
  EXPECT_TRUE(blue_z.value().GetBool());
}

TEST(BluetoothTypeConverterTest,
     ConvertMojoValueAttributeToBlueZAttribute_UintType) {
  constexpr uint16_t kValue = 10;
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
  mojo->type_size = static_cast<uint32_t>(sizeof(kValue));
  mojo->value = base::Value(static_cast<int>(kValue));

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT, blue_z.type());
  EXPECT_EQ(sizeof(kValue), blue_z.size());
  ASSERT_EQ(base::Value::Type::INTEGER, blue_z.value().type());
  EXPECT_EQ(kValue, static_cast<uint16_t>(blue_z.value().GetInt()));
}

TEST(BluetoothTypeConverterTest,
     ConvertMojoValueAttributeToBlueZAttribute_IntType) {
  constexpr int16_t kValue = 20;
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::INT;
  mojo->type_size = static_cast<uint32_t>(sizeof(kValue));
  mojo->value = base::Value(static_cast<int>(kValue));

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::INT, blue_z.type());
  EXPECT_EQ(sizeof(kValue), blue_z.size());
  ASSERT_EQ(base::Value::Type::INTEGER, blue_z.value().type());
  EXPECT_EQ(kValue, static_cast<int16_t>(blue_z.value().GetInt()));
}

TEST(BluetoothTypeConverterTest,
     ConvertMojoValueAttributeToBlueZAttribute_UuidType) {
  // Construct Mojo attribute with TYPE_UUID.
  constexpr char kValue[] = "00000000-0000-1000-8000-00805f9b34fb";
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::UUID;
  // UUIDs are all stored in string form, but it can be converted to one of
  // UUID16, UUID32 and UUID128.
  mojo->type_size = static_cast<uint32_t>(sizeof(uint16_t));
  mojo->value = base::Value(kValue);

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID, blue_z.type());
  EXPECT_EQ(sizeof(uint16_t), blue_z.size());
  ASSERT_EQ(base::Value::Type::STRING, blue_z.value().type());
  EXPECT_EQ(kValue, blue_z.value().GetString());
}

TEST(BluetoothTypeConverterTest,
     ConvertMojoValueAttributeToBlueZAttribute_StringType) {
  // Construct Mojo attribute with TYPE_STRING. TYPE_URL is the same case as
  // TYPE_STRING.
  constexpr char kValue[] = "Some SDP service";
  constexpr size_t kValueSize = sizeof(kValue) - 1;  // Subtract '\0' size.
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::STRING;
  // Subtract '\0'-terminate size.
  mojo->type_size = static_cast<uint32_t>(kValueSize);
  mojo->value = base::Value(kValue);

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::STRING, blue_z.type());
  EXPECT_EQ(kValueSize, blue_z.size());
  ASSERT_EQ(base::Value::Type::STRING, blue_z.value().type());
  EXPECT_EQ(kValue, blue_z.value().GetString());
}

TEST(BluetoothTypeConverterTest, ConvertMojoSequenceAttributeToBlueZAttribute) {
  constexpr char kL2capUuid[] = "00000100-0000-1000-8000-00805f9b34fb";
  constexpr uint16_t kL2capChannel = 3;

  // Create a sequence with the above two values.
  auto sequence_mojo = arc::mojom::BluetoothSdpAttribute::New();
  sequence_mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE;
  {
    // Create an UUID value.
    auto value_uuid = arc::mojom::BluetoothSdpAttribute::New();
    value_uuid->type = bluez::BluetoothServiceAttributeValueBlueZ::UUID;
    value_uuid->type_size = static_cast<uint32_t>(sizeof(uint16_t));
    value_uuid->value = base::Value(kL2capUuid);
    sequence_mojo->sequence.push_back(std::move(value_uuid));
  }
  {
    // Create an UINT value.
    auto value_channel = arc::mojom::BluetoothSdpAttribute::New();
    value_channel->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
    value_channel->type_size = static_cast<uint32_t>(sizeof(kL2capChannel));
    value_channel->value = base::Value(static_cast<int>(kL2capChannel));
    sequence_mojo->sequence.push_back(std::move(value_channel));
  }
  sequence_mojo->type_size = sequence_mojo->sequence.size();
  sequence_mojo->value = base::nullopt;

  auto sequence_blue_z =
      sequence_mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            sequence_blue_z.type());
  EXPECT_EQ(sequence_mojo->sequence.size(), sequence_blue_z.sequence().size());

  const auto& sequence = sequence_blue_z.sequence();
  ASSERT_EQ(2u, sequence.size());
  {
    const auto& blue_z = sequence[0];
    EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID, blue_z.type());
    EXPECT_EQ(sizeof(uint16_t), blue_z.size());
    ASSERT_EQ(base::Value::Type::STRING, blue_z.value().type());
    EXPECT_EQ(kL2capUuid, blue_z.value().GetString());
  }

  {
    const auto& blue_z = sequence[1];
    EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT, blue_z.type());
    EXPECT_EQ(sizeof(kL2capChannel), blue_z.size());
    ASSERT_EQ(base::Value::Type::INTEGER, blue_z.value().type());
    EXPECT_EQ(kL2capChannel, static_cast<uint16_t>(blue_z.value().GetInt()));
  }
}

TEST(BluetoothTypeConverterTest,
     ConvertInvalidMojoValueAttributeToBlueZAttribute) {
  // Create a Mojo attribute without value defined.
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::UINT;
  mojo->type_size = static_cast<uint32_t>(sizeof(uint32_t));
  mojo->value = base::nullopt;

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            blue_z.type());
  EXPECT_EQ(0u, blue_z.size());
  EXPECT_EQ(base::Value::Type::NONE, blue_z.value().type());
}

TEST(BluetoothTypeConverterTest,
     ConvertInvalidMojoSequenceAttributeToBlueZAttribute_NoData) {
  // Create a Mojo attribute with an empty sequence.
  auto mojo = arc::mojom::BluetoothSdpAttribute::New();
  mojo->type = bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE;
  mojo->type_size = 0;
  mojo->value = base::nullopt;

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE,
            blue_z.type());
  EXPECT_EQ(0u, blue_z.size());
  EXPECT_EQ(base::Value::Type::NONE, blue_z.value().type());
}

TEST(BluetoothTypeConverterTest,
     ConvertInvalidMojoSequenceAttributeToBlueZAttribute_TooDeep) {
  // Create a Mojo attribute with the depth = arc::kBluetoothSDPMaxDepth + 3.
  auto mojo = CreateDeepMojoSequenceAttribute(arc::kBluetoothSDPMaxDepth + 3);

  auto blue_z = mojo.To<bluez::BluetoothServiceAttributeValueBlueZ>();

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            blue_z.type());
  EXPECT_EQ(1u, blue_z.size());
  EXPECT_EQ(arc::kBluetoothSDPMaxDepth, GetDepthOfBlueZAttribute(blue_z));
}

TEST(BluetoothTypeConverterTest,
     ConvertBlueZValueAttributeToMojoAttribute_NullType) {
  // Check NULL type.
  auto blue_z = bluez::BluetoothServiceAttributeValueBlueZ();

  auto mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::NULLTYPE, mojo->type);
  EXPECT_EQ(0u, mojo->type_size);

  ASSERT_TRUE(mojo->value.has_value());
  EXPECT_TRUE(mojo->value->is_none());
}

TEST(BluetoothTypeConverterTest,
     ConvertBlueZValueAttributeToMojoAttribute_UintType) {
  // Check integer types (INT, UINT).
  constexpr uint16_t kValue = 10;
  auto blue_z = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(kValue),
      std::make_unique<base::Value>(static_cast<int>(kValue)));

  auto mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT, mojo->type);
  EXPECT_EQ(sizeof(kValue), mojo->type_size);

  ASSERT_TRUE(mojo->value.has_value());
  ASSERT_TRUE(mojo->value->is_int());
  EXPECT_EQ(kValue, static_cast<uint16_t>(mojo->value->GetInt()));
}

TEST(BluetoothTypeConverterTest,
     ConvertBlueZValueAttributeToMojoAttribute_BoolType) {
  // Check bool type.
  auto blue_z = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::BOOL, sizeof(bool),
      std::make_unique<base::Value>(false));

  auto mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::BOOL, mojo->type);
  EXPECT_EQ(static_cast<uint32_t>(sizeof(bool)), mojo->type_size);

  ASSERT_TRUE(mojo->value.has_value());
  ASSERT_TRUE(mojo->value->is_bool());
  EXPECT_FALSE(mojo->value->GetBool());
}

TEST(BluetoothTypeConverterTest,
     ConvertBlueZValueAttributeToMojoAttribute_UuidType) {
  // Check UUID type.
  constexpr char kValue[] = "00000100-0000-1000-8000-00805f9b34fb";
  auto blue_z = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UUID, sizeof(uint16_t),
      std::make_unique<base::Value>(kValue));

  auto mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID, mojo->type);
  EXPECT_EQ(static_cast<uint32_t>(sizeof(uint16_t)), mojo->type_size);

  ASSERT_TRUE(mojo->value.has_value());
  ASSERT_TRUE(mojo->value->is_string());
  EXPECT_EQ(kValue, mojo->value->GetString());
}

TEST(BluetoothTypeConverterTest,
     ConvertBlueZValueAttributeToMojoAttribute_StringType) {
  // Check string types (STRING, URL).
  constexpr char kValue[] = "Some Service Name";
  constexpr size_t kValueSize = sizeof(kValue) - 1;  // Subtract '\0' size.
  auto blue_z = bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::STRING, kValueSize,
      std::make_unique<base::Value>(kValue));

  auto mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::STRING, mojo->type);
  EXPECT_EQ(static_cast<uint32_t>(kValueSize), mojo->type_size);

  ASSERT_TRUE(mojo->value.has_value());
  ASSERT_TRUE(mojo->value->is_string());
  EXPECT_EQ(kValue, mojo->value->GetString());
}

TEST(BluetoothTypeConverterTest, ConvertBlueZSequenceAttributeToMojoAttribute) {
  constexpr char kL2capUuid[] = "00000100-0000-1000-8000-00805f9b34fb";
  constexpr uint16_t kL2capChannel = 3;

  auto sequence =
      std::make_unique<bluez::BluetoothServiceAttributeValueBlueZ::Sequence>();
  sequence->push_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UUID, sizeof(uint16_t),
      std::make_unique<base::Value>(kL2capUuid)));
  sequence->push_back(bluez::BluetoothServiceAttributeValueBlueZ(
      bluez::BluetoothServiceAttributeValueBlueZ::UINT, sizeof(uint16_t),
      std::make_unique<base::Value>(kL2capChannel)));

  auto blue_z = bluez::BluetoothServiceAttributeValueBlueZ(std::move(sequence));
  ASSERT_EQ(2u, blue_z.sequence().size());

  auto sequence_mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE,
            sequence_mojo->type);
  EXPECT_EQ(static_cast<uint32_t>(blue_z.size()), sequence_mojo->type_size);
  EXPECT_EQ(sequence_mojo->type_size, sequence_mojo->sequence.size());

  {
    const auto& mojo = sequence_mojo->sequence[0];
    EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UUID, mojo->type);
    EXPECT_EQ(static_cast<uint32_t>(sizeof(uint16_t)), mojo->type_size);

    ASSERT_TRUE(mojo->value.has_value());
    ASSERT_TRUE(mojo->value->is_string());
    EXPECT_EQ(kL2capUuid, mojo->value->GetString());
  }

  {
    const auto& mojo = sequence_mojo->sequence[1];
    EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::UINT, mojo->type);
    EXPECT_EQ(static_cast<uint32_t>(sizeof(uint16_t)), mojo->type_size);
    ASSERT_TRUE(mojo->value.has_value());
    ASSERT_TRUE(mojo->value->is_int());
    EXPECT_EQ(kL2capChannel, static_cast<uint16_t>(mojo->value->GetInt()));
  }
}

TEST(BluetoothTypeConverterTest,
     ConvertInvalidBlueZSequenceAttributeToBlueZAttribute_TooDeep) {
  bluez::BluetoothServiceAttributeValueBlueZ blue_z =
      CreateDeepBlueZSequenceAttribute(arc::kBluetoothSDPMaxDepth + 3);

  auto mojo = ConvertTo<arc::mojom::BluetoothSdpAttributePtr>(blue_z);

  EXPECT_EQ(bluez::BluetoothServiceAttributeValueBlueZ::SEQUENCE, mojo->type);
  EXPECT_EQ(1u, mojo->type_size);
  EXPECT_EQ(arc::kBluetoothSDPMaxDepth, GetDepthOfMojoAttribute(mojo));
}

}  // namespace mojo
