// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_weave_packet_generator.h"

#include <algorithm>
#include <memory>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel::weave {

class SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest
    : public testing::Test {
 public:
  SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest(
      const SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest&) = delete;
  SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest& operator=(
      const SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest&) = delete;

 protected:
  SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest() {}

  void TestConnectionCloseWithReason(ReasonForClose reason_for_close,
                                     uint8_t expected_reason_for_close) {
    std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
        std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

    Packet packet = generator->CreateConnectionClose(reason_for_close);

    const uint16_t kCloseSize = 3;
    Packet expected(kCloseSize, 0);
    // uWeave Header:
    // 1--- ---- : type = 1 (control packet)
    // -000 ---- : counter = 0
    // ---- 0010 : command = 2 (close)
    // 1000 0010 = 0x82
    expected = {0x82, kEmptyUpperByte, expected_reason_for_close};

    EXPECT_EQ(expected, packet);
  }

  uint8_t GetCounterFromHeader(uint8_t header) { return (header >> 4) & 7; }

  uint8_t GetPacketType(uint8_t header) { return (header >> 7) & 1; }
};

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       CreateConnectionRequestTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  Packet packet = generator->CreateConnectionRequest();

  const uint16_t kRequestSize = 7;
  Packet expected(kRequestSize, 0);
  // uWeave Header:
  // 1--- ---- :  type = 1 (control packet)
  // -000 ---- : counter = 0
  // ---- 0000 : command = 0 (request)
  // 1000 0000 = 0x80
  expected = {0x80,
              kEmptyUpperByte,
              kByteWeaveVersion,
              kEmptyUpperByte,
              kByteWeaveVersion,
              kEmptyUpperByte,
              kByteSelectMaxPacketSize};

  EXPECT_EQ(expected, packet);
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       CreateConnectionResponseWithDefaultPacketSizeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  Packet packet = generator->CreateConnectionResponse();

  const uint16_t kResponseSize = 5;
  Packet expected_default(kResponseSize, 0);
  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -000 ---- : counter = 0
  // ---- 0001 : command = 1 (response)
  // 1000 0001 = 0x81
  expected_default = {0x81, kEmptyUpperByte, kByteWeaveVersion, kEmptyUpperByte,
                      kByteDefaultMaxPacketSize};

  EXPECT_EQ(expected_default, packet);
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       CreateConnectionResponseWithSelectedPacketSizeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  const uint8_t kSelectedPacketSize = 30;
  const uint16_t kResponseSize = 5;

  generator->SetMaxPacketSize(kSelectedPacketSize);

  Packet packet = generator->CreateConnectionResponse();

  Packet expected_selected(kResponseSize, 0);
  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -000 ---- : counter = 0
  // ---- 0001 : command = 1 (response)
  // 1000 0001 = 0x81
  expected_selected = {0x81, kEmptyUpperByte, kByteWeaveVersion,
                       kEmptyUpperByte, kSelectedPacketSize};
  EXPECT_EQ(expected_selected, packet);
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       CreateConnectionCloseTest) {
  // Reason for close spec of uWeave.
  // 0x00: Close without error
  // 0x01: Unknown error
  // 0x02: No common version supported
  // 0x03: Received packet out of sequence
  // 0x80: Application error

  TestConnectionCloseWithReason(ReasonForClose::CLOSE_WITHOUT_ERROR, 0x00);
  TestConnectionCloseWithReason(ReasonForClose::UNKNOWN_ERROR, 0x01);
  TestConnectionCloseWithReason(ReasonForClose::NO_COMMON_VERSION_SUPPORTED,
                                0x02);
  TestConnectionCloseWithReason(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
                                0x03);
  TestConnectionCloseWithReason(ReasonForClose::APPLICATION_ERROR, 0x80);
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       EncodeDataMessageWithDefaultPacketSizeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  std::string data = "abcdefghijklmnopqrstuvwxyz";

  std::vector<Packet> packets = generator->EncodeDataMessage(data);

  std::vector<Packet> expected(2);

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -000 ---- : counter = 0
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0000 1000 = 0x08
  expected[0] = {0x08, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
                 'j',  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's'};

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 0--- : first packet = false
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 0100 = 0x14
  expected[1] = {0x14, 't', 'u', 'v', 'w', 'x', 'y', 'z'};

  EXPECT_EQ(expected, packets);
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       EncodeDataMessageWithSelectedPacketSizeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  const uint32_t packet_size = 30;
  const uint32_t residual_packet_size = 2;
  std::string a(packet_size - 1, 'a');
  std::string b(packet_size - 1, 'b');
  std::string c(residual_packet_size - 1, 'c');

  std::string data = a + b + c;

  generator->SetMaxPacketSize(packet_size);

  std::vector<Packet> packets = generator->EncodeDataMessage(data);

  std::vector<Packet> expected(3);

  expected[0].assign(packet_size, 'a');
  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -000 ---- : counter = 0
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0000 1000 = 0x08
  expected[0][0] = 0x08;

  expected[1].assign(packet_size, 'b');
  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 0--- : first packet = false
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0001 0000 = 0x10
  expected[1][0] = 0x10;

  expected[2].assign(residual_packet_size, 'c');
  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 0--- : first packet = false
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0010 0100 = 0x24
  expected[2][0] = 0x24;

  EXPECT_EQ(expected, packets);
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       PacketCounterForMixedPacketTypesTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  Packet packet = generator->CreateConnectionRequest();

  EXPECT_EQ(0, GetCounterFromHeader(packet[0]));

  std::string data = "a";
  std::vector<Packet> packets = generator->EncodeDataMessage(data);

  EXPECT_EQ(1, GetCounterFromHeader(packets[0][0]));

  packet = generator->CreateConnectionClose(ReasonForClose::UNKNOWN_ERROR);

  EXPECT_EQ(2, GetCounterFromHeader(packet[0]));
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketGeneratorTest,
       PacketCounterWrappedAroundTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> generator =
      std::make_unique<BluetoothLowEnergyWeavePacketGenerator>();

  const uint8_t kNumPackets = 100;
  std::string data(kNumPackets * kByteDefaultMaxPacketSize, 'a');

  std::vector<Packet> packets = generator->EncodeDataMessage(data);

  std::vector<Packet> expected(kNumPackets);

  const uint8_t kDataType = 0;

  for (uint8_t i = 0; i < kNumPackets; ++i) {
    uint8_t header = packets[i][0];
    EXPECT_EQ(i % kMaxPacketCounter, GetCounterFromHeader(header));
    EXPECT_EQ(kDataType, GetPacketType(header));
  }
}

}  // namespace ash::secure_channel::weave
