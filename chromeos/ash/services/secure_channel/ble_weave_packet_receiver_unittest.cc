// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_weave_packet_receiver.h"

#include <algorithm>
#include <memory>
#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel::weave {

namespace {

typedef BluetoothLowEnergyWeavePacketReceiver::ReceiverType ReceiverType;
typedef BluetoothLowEnergyWeavePacketReceiver::State State;
typedef BluetoothLowEnergyWeavePacketReceiver::ReceiverError ReceiverError;

const uint8_t kCloseWithoutError = 0;

// uWeave Header:
// 1--- ---- :  type = 1 (control packet)
// -000 ---- : counter = 0
// ---- 0000 : command = 0 (request)
// 1000 0000 = 0x80
const uint8_t kControlRequestHeader = 0x80;

// uWeave Header:
// 1--- ---- : type = 1 (control packet)
// -000 ---- : counter = 0
// ---- 0001 : command = 1 (response)
// 1000 0001 = 0x81
const uint8_t kControlResponseHeader = 0x81;

}  // namespace

class SecureChannelBluetoothLowEnergyWeavePacketReceiverTest
    : public testing::Test {
 public:
  SecureChannelBluetoothLowEnergyWeavePacketReceiverTest(
      const SecureChannelBluetoothLowEnergyWeavePacketReceiverTest&) = delete;
  SecureChannelBluetoothLowEnergyWeavePacketReceiverTest& operator=(
      const SecureChannelBluetoothLowEnergyWeavePacketReceiverTest&) = delete;

 protected:
  SecureChannelBluetoothLowEnergyWeavePacketReceiverTest() {}
};

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       WellBehavingServerPacketsNoControlDataTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  std::vector<uint8_t> p1(kByteDefaultMaxPacketSize, 'a');
  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0001 1000 = 0x18
  p1[0] = 0x18;
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::RECEIVING_DATA, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 0--- : first packet = false
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0010 0100 = 0x24
  std::vector<uint8_t> p2{0x24, 'c', 'd'};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("aaaaaaaaaaaaaaaaaaacd", receiver->GetDataMessage());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -011 ---- : counter = 3
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0011 1100 = 0x3C
  std::vector<uint8_t> p3{0x3C, 'g', 'o', 'o', 'g', 'l', 'e'};
  receiver->ReceivePacket(p3);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("google", receiver->GetDataMessage());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -100 ---- : counter = 4
  // ---- 0010 : command = 2 (close)
  // 1100 0010 = 0xC2
  // 0x80 is the hex value for APPLICATION_ERROR
  std::vector<uint8_t> p4{0xC2, kEmptyUpperByte, 0x80};
  receiver->ReceivePacket(p4);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::APPLICATION_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       WellBehavingServerPacketsWithFullControlDataTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteSelectMaxPacketSize,
                          'a',
                          'b',
                          'c',
                          'd',
                          'e',
                          'f',
                          'g',
                          'h',
                          'i',
                          'j',
                          'k',
                          'l',
                          'm'};

  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("abcdefghijklm", receiver->GetDataMessage());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0001 1000 = 0x18
  std::vector<uint8_t> p1(kByteDefaultMaxPacketSize, 'o');
  p1[0] = 0x18;
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::RECEIVING_DATA, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 0--- : first packet = false
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0010 0100 = 0x24
  std::vector<uint8_t> p2{0x24, 'p', 'q'};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("ooooooooooooooooooopq", receiver->GetDataMessage());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -011 ---- : counter = 3
  // ---- 0010 : command = 2 (close)
  // 1011 0010 = 0xB2
  std::vector<uint8_t> p3{0xB2, kEmptyUpperByte, kCloseWithoutError};
  receiver->ReceivePacket(p3);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::CLOSE_WITHOUT_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       WellBehavingServerPacketsWithSomeControlDataTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,    kEmptyUpperByte,
                          kByteWeaveVersion,        kEmptyUpperByte,
                          kByteWeaveVersion,        kEmptyUpperByte,
                          kByteSelectMaxPacketSize, 'a'};

  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("a", receiver->GetDataMessage());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0001 1000 = 0x18
  std::vector<uint8_t> p1(kByteDefaultMaxPacketSize, 'o');
  p1[0] = 0x18;
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::RECEIVING_DATA, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 0--- : first packet = false
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0010 0100 = 0x24
  std::vector<uint8_t> p2{0x24, 'p', 'q'};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("ooooooooooooooooooopq", receiver->GetDataMessage());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -011 ---- : counter = 3
  // ---- 0010 : command = 2 (close)
  // 1011 0010 = 0xB2
  std::vector<uint8_t> p3{0xB2, kEmptyUpperByte, kCloseWithoutError};
  receiver->ReceivePacket(p3);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::CLOSE_WITHOUT_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       WellBehavingClientPacketsNoControlDataTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  const uint8_t kSelectedPacketSize = 30;
  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kSelectedPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());
  EXPECT_EQ(kSelectedPacketSize, receiver->GetMaxPacketSize());

  std::vector<uint8_t> p1(kSelectedPacketSize, 'o');
  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 1100 = 0x1C
  p1[0] = 0x1C;
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("ooooooooooooooooooooooooooooo", receiver->GetDataMessage());

  const uint8_t kApplicationError = 0x80;
  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -010 ---- : counter = 2
  // ---- 0010 : command = 2 (close)
  // 1010 0010 = 0xA2
  std::vector<uint8_t> p2{0xA2, kEmptyUpperByte, kApplicationError};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::APPLICATION_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       WellBehavingClientPacketsWithFullControlDataTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteDefaultMaxPacketSize,
                          'a',
                          'b',
                          'c',
                          'd',
                          'e',
                          'f',
                          'g',
                          'h',
                          'i',
                          'j',
                          'k',
                          'l',
                          'm',
                          'n',
                          'o'};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("abcdefghijklmno", receiver->GetDataMessage());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 1100 = 0x1C
  std::vector<uint8_t> p1{0x1C, 'g', 'o', 'o', 'g', 'l', 'e'};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("google", receiver->GetDataMessage());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -010 ---- : counter = 2
  // ---- 0010 : command = 2 (close)
  // 1010 0010 = 0xA2
  std::vector<uint8_t> p2{0xA2, kEmptyUpperByte, kCloseWithoutError};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::CLOSE_WITHOUT_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       WellBehavingClientPacketsWithSomeControlDataTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteDefaultMaxPacketSize,
                          'a',
                          'b',
                          'c'};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("abc", receiver->GetDataMessage());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 1100 = 0x1C
  std::vector<uint8_t> p1{0x1C, 'g', 'o', 'o', 'g', 'l', 'e'};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("google", receiver->GetDataMessage());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -010 ---- : counter = 2
  // ---- 0010 : command = 2 (close)
  // 1010 0010 = 0xA2
  std::vector<uint8_t> p2{0xA2, kEmptyUpperByte, kCloseWithoutError};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::CLOSE_WITHOUT_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       LegacyCloseWithoutReasonTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -001 ---- : counter = 1
  // ---- 0010 : command = 2 (close)
  // 1001 0010 = 0x92
  std::vector<uint8_t> p1{0x92};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonForClose());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       OneBytePacketTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 1100 = 0x1C
  std::vector<uint8_t> p1{0x1C};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::DATA_READY, receiver->GetState());
  EXPECT_EQ("", receiver->GetDataMessage());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       EmptyPacketTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0;
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::EMPTY_PACKET, receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ServerReceivingConnectionResponseTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);
  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::CLIENT_RECEIVED_CONNECTION_REQUEST,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ClientReceivingConnectionRequestTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);
  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::SERVER_RECEIVED_CONNECTION_RESPONSE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ReceiveConnectionCloseInConnecting) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -000 ---- : counter = 0
  // ---- 0010 : command = 2 (close)
  // 1000 0010 = 0x82
  std::vector<uint8_t> p0{0x82, kEmptyUpperByte, kCloseWithoutError};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::RECEIVED_CONNECTION_CLOSE_IN_CONNECTING,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ReceiveDataInConnecting) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -000 ---- : counter = 0
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0000 1000 = 0x08
  std::vector<uint8_t> p3{0x08, 'a', 'b', 'c', 'd'};
  receiver->ReceivePacket(p3);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::RECEIVED_DATA_IN_CONNECTING,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ConnectionRequestTooSmallTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);
  std::vector<uint8_t> p0{kControlRequestHeader, kEmptyUpperByte,
                          kByteWeaveVersion,     kEmptyUpperByte,
                          kByteWeaveVersion,     kEmptyUpperByte};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_CONNECTION_REQUEST_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ConnectionRequestTooLargeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0(kByteDefaultMaxPacketSize + 1, 0);
  p0[0] = kControlRequestHeader;
  p0[2] = kByteWeaveVersion;
  p0[4] = kByteWeaveVersion;
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_CONNECTION_REQUEST_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ConnectionResponseTooSmallTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_CONNECTION_RESPONSE_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ConnectionResponseTooLargeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0(kByteDefaultMaxPacketSize + 1, 0);
  p0[0] = kControlResponseHeader;
  p0[2] = kByteWeaveVersion;
  p0[4] = kByteDefaultMaxPacketSize;
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_CONNECTION_RESPONSE_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ConnectionCloseTooLargeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -001 ---- : counter = 1
  // ---- 0010 : command = 2 (close)
  // 1001 0010 = 0x92
  std::vector<uint8_t> p1{0x92, kEmptyUpperByte, kCloseWithoutError, 'a'};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReceiverError::INVALID_CONNECTION_CLOSE_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       DataPacketTooLargeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 1100 = 0x1C
  std::vector<uint8_t> p1(kByteDefaultMaxPacketSize + 1, 'a');
  p1[0] = 0x1C;
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_DATA_PACKET_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       FirstPacketNoFirstNorLastBitTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 0--- : first packet = false
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0001 0000 = 0x10
  std::vector<uint8_t> p1{0x10};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
            receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INCORRECT_DATA_FIRST_BIT,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       FirstPacketNoFirstYesLastBitTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 0--- : first packet = false
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0001 0100 = 0x14
  std::vector<uint8_t> p1{0x14};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
            receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INCORRECT_DATA_FIRST_BIT,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       NonFirstPacketYesFirstBitTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0001 1000 = 0x18
  std::vector<uint8_t> p1{0x18};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::RECEIVING_DATA, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 1--- : first packet = true
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0010 1000 = 0x28
  std::vector<uint8_t> p2{0x28};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
            receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INCORRECT_DATA_FIRST_BIT,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       OutOfOrderPacketTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 0--- : first packet = false
  // ---- -0-- : last packet = false
  // ---- --00 : defined by uWeave to be 0
  // 0010 0000 = 0x20
  std::vector<uint8_t> p1{0x20};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::RECEIVED_PACKET_OUT_OF_SEQUENCE,
            receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::PACKET_OUT_OF_SEQUENCE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidVersionInConnectionRequestTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  const uint8_t kWrongVersion = 2;
  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kWrongVersion,           kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::NO_COMMON_VERSION_SUPPORTED,
            receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::NOT_SUPPORTED_REQUESTED_VERSION,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidMaxPacketSizeInConnectionRequestTest) {
  const uint8_t kSmallMaxPacketSize = 19;

  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader, kEmptyUpperByte,
                          kByteWeaveVersion,     kEmptyUpperByte,
                          kByteWeaveVersion,     kEmptyUpperByte,
                          kSmallMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_REQUESTED_MAX_PACKET_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidSelectedVersionInConnectionResponseTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader, kByteWeaveVersion,
                          kEmptyUpperByte, kEmptyUpperByte,
                          kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::NO_COMMON_VERSION_SUPPORTED,
            receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::NOT_SUPPORTED_SELECTED_VERSION,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidSelectedMaxPacketSizeInConnectionResponseTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  const uint8_t kSmallMaxPacketSize = 19;
  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kSmallMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_SELECTED_MAX_PACKET_SIZE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       UnrecognizedReasonForCloseInConnectionCloseTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  const uint8_t kInvalidReasonForClose = 5;
  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -001 ---- : counter = 1
  // ---- 0010 : command = 2 (close)
  // 1001 0010 = 0x92
  std::vector<uint8_t> p1{0x92, kEmptyUpperByte, kInvalidReasonForClose};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::UNRECOGNIZED_REASON_FOR_CLOSE,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       UnrecognizedControlCommandBitTwoTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  // uWeave Header:
  // 1--- ---- :  type = 1 (control packet)
  // -000 ---- : counter = 0
  // ---- 0100 : command = 4 (INVALID)
  // 1000 0100 = 0x84
  std::vector<uint8_t> p0{0x84,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::UNRECOGNIZED_CONTROL_COMMAND,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidControlCommandBitThreeTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  // uWeave Header:
  // 1--- ---- :  type = 1 (control packet)
  // -000 ---- : counter = 0
  // ---- 1000 : command = 8 (INVALID)
  // 1000 1000 = 0x88
  std::vector<uint8_t> p0{0x88, kEmptyUpperByte, kByteWeaveVersion,
                          kEmptyUpperByte, kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::UNRECOGNIZED_CONTROL_COMMAND,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidBitOneInDataPacketHeaderTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --10 : defined by uWeave to be 0, but bit 1 is not
  // 0001 1110 = 0x1E
  std::vector<uint8_t> p1{0x1E, 'a'};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::DATA_HEADER_LOW_BITS_NOT_CLEARED,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       InvalidBitZeroInDataPacketHeaderTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kByteDefaultMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -001 ---- : counter = 1
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --01 : defined by uWeave to be 0, but bit 0 is not
  // 0001 1101 = 0x1D
  std::vector<uint8_t> p1{0x1D, 'a'};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::DATA_HEADER_LOW_BITS_NOT_CLEARED,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ReceivedPacketInErrorState) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::CLIENT);

  std::vector<uint8_t> p0;
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());

  std::vector<uint8_t> p1{kControlResponseHeader, kEmptyUpperByte,
                          kByteWeaveVersion, kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::EMPTY_PACKET, receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       ReceivedPacketInConnectionClosedStateTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 1--- ---- : type = 1 (control packet)
  // -001 ---- : counter = 1
  // ---- 0010 : command = 2 (close)
  // 1001 0010 = 0x92
  std::vector<uint8_t> p1{0x92, kEmptyUpperByte, kCloseWithoutError};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::CONNECTION_CLOSED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::CLOSE_WITHOUT_ERROR, receiver->GetReasonForClose());

  // uWeave Header:
  // 0--- ---- : type = 0 (data packet)
  // -010 ---- : counter = 2
  // ---- 1--- : first packet = true
  // ---- -1-- : last packet = true
  // ---- --00 : defined by uWeave to be 0
  // 0010 1100 = 0x2C
  std::vector<uint8_t> p2{0x2C, 'a'};
  receiver->ReceivePacket(p2);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::RECEIVED_PACKET_IN_CONNECTION_CLOSED,
            receiver->GetReceiverError());
}

TEST_F(SecureChannelBluetoothLowEnergyWeavePacketReceiverTest,
       MultipleControlPacketTest) {
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> receiver =
      std::make_unique<BluetoothLowEnergyWeavePacketReceiver>(
          ReceiverType::SERVER);

  std::vector<uint8_t> p0{kControlRequestHeader,   kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteWeaveVersion,       kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p0);
  EXPECT_EQ(State::WAITING, receiver->GetState());

  // uWeave Header:
  // 1--- ---- :  type = 1 (control packet)
  // -001 ---- : counter = 1
  // ---- 0000 : command = 0 (request)
  // 1001 0000 = 0x90
  std::vector<uint8_t> p1{0x90,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteWeaveVersion,
                          kEmptyUpperByte,
                          kByteSelectMaxPacketSize};
  receiver->ReceivePacket(p1);
  EXPECT_EQ(State::ERROR_DETECTED, receiver->GetState());
  EXPECT_EQ(ReasonForClose::UNKNOWN_ERROR, receiver->GetReasonToClose());
  EXPECT_EQ(ReceiverError::INVALID_CONTROL_COMMAND_IN_DATA_TRANSACTION,
            receiver->GetReceiverError());
}

}  // namespace ash::secure_channel::weave
