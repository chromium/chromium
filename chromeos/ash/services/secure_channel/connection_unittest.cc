// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connection.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/connection_observer.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

class MockConnection : public Connection {
 public:
  MockConnection() : Connection(multidevice::CreateRemoteDeviceRefForTest()) {}

  MockConnection(const MockConnection&) = delete;
  MockConnection& operator=(const MockConnection&) = delete;

  ~MockConnection() override {}

  MOCK_METHOD1(SetPaused, void(bool paused));
  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD0(GetDeviceAddress, std::string());
  MOCK_METHOD0(CancelConnectionAttempt, void());
  MOCK_METHOD1(SendMessageImplProxy, void(WireMessage* message));
  MOCK_METHOD4(
      RegisterPayloadFileImpl,
      void(int64_t payload_id,
           mojom::PayloadFilesPtr payload_files,
           FileTransferUpdateCallback file_transfer_update_callback,
           base::OnceCallback<void(bool)> registration_result_callback));
  MOCK_METHOD1(DeserializeWireMessageProxy,
               WireMessage*(bool* is_incomplete_message));

  // Gmock only supports copyable types, so create simple wrapper methods for
  // ease of mocking.
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override {
    SendMessageImplProxy(message.get());
  }

  std::unique_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message) override {
    return base::WrapUnique(DeserializeWireMessageProxy(is_incomplete_message));
  }

  using Connection::OnBytesReceived;
  using Connection::OnDidSendMessage;
  using Connection::SetStatus;
  using Connection::status;
};

class MockConnectionObserver : public ConnectionObserver {
 public:
  MockConnectionObserver() {}

  MockConnectionObserver(const MockConnectionObserver&) = delete;
  MockConnectionObserver& operator=(const MockConnectionObserver&) = delete;

  ~MockConnectionObserver() override {}

  MOCK_METHOD3(OnConnectionStatusChanged,
               void(Connection* connection,
                    Connection::Status old_status,
                    Connection::Status new_status));
  MOCK_METHOD2(OnMessageReceived,
               void(const Connection& connection, const WireMessage& message));
  MOCK_METHOD3(OnSendCompleted,
               void(const Connection& connection,
                    const WireMessage& message,
                    bool success));
};

// Unlike WireMessage, offers a public constructor.
class TestWireMessage : public WireMessage {
 public:
  TestWireMessage() : WireMessage("payload", "feature") {}

  TestWireMessage(const TestWireMessage&) = delete;
  TestWireMessage& operator=(const TestWireMessage&) = delete;

  ~TestWireMessage() override = default;
};

}  // namespace

class CryptAuthConnectionTest : public testing::Test {
 public:
  CryptAuthConnectionTest(const CryptAuthConnectionTest&) = delete;
  CryptAuthConnectionTest& operator=(const CryptAuthConnectionTest&) = delete;

 protected:
  CryptAuthConnectionTest() = default;
  ~CryptAuthConnectionTest() override = default;

  std::optional<int32_t> GetRssi(Connection* connection) {
    connection->GetConnectionRssi(base::BindOnce(
        &CryptAuthConnectionTest::OnConnectionRssi, base::Unretained(this)));

    std::optional<int32_t> rssi = rssi_;
    rssi_.reset();

    return rssi;
  }

 private:
  void OnConnectionRssi(std::optional<int32_t> rssi) { rssi_ = rssi; }

  std::optional<int32_t> rssi_;
};

TEST_F(CryptAuthConnectionTest, IsConnected) {
  StrictMock<MockConnection> connection;
  EXPECT_FALSE(connection.IsConnected());

  connection.SetStatus(Connection::Status::CONNECTED);
  EXPECT_TRUE(connection.IsConnected());

  connection.SetStatus(Connection::Status::DISCONNECTED);
  EXPECT_FALSE(connection.IsConnected());

  connection.SetStatus(Connection::Status::IN_PROGRESS);
  EXPECT_FALSE(connection.IsConnected());
}

TEST_F(CryptAuthConnectionTest, SendMessage_FailsWhenNotConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::IN_PROGRESS);

  EXPECT_CALL(connection, GetDeviceAddress()).Times(1);
  EXPECT_CALL(connection, SendMessageImplProxy(_)).Times(0);
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST_F(CryptAuthConnectionTest,
       SendMessage_FailsWhenAnotherMessageSendIsInProgress) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);
  connection.SendMessage(std::unique_ptr<WireMessage>());

  EXPECT_CALL(connection, SendMessageImplProxy(_)).Times(0);
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST_F(CryptAuthConnectionTest, SendMessage_SucceedsWhenConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);

  EXPECT_CALL(connection, SendMessageImplProxy(_));
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST_F(CryptAuthConnectionTest,
       SendMessage_SucceedsAfterPreviousMessageSendCompletes) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);
  connection.SendMessage(std::unique_ptr<WireMessage>());
  connection.OnDidSendMessage(TestWireMessage(), true /* success */);

  EXPECT_CALL(connection, SendMessageImplProxy(_));
  connection.SendMessage(std::unique_ptr<WireMessage>());
}

TEST_F(CryptAuthConnectionTest, RegisterPayloadFile_FailsWhenNotConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::IN_PROGRESS);

  EXPECT_CALL(connection, GetDeviceAddress()).Times(1);
  EXPECT_CALL(connection, RegisterPayloadFileImpl(_, _, _, _)).Times(0);
  bool result;
  connection.RegisterPayloadFile(
      /*payload_id=*/1234, mojom::PayloadFiles::New(),
      FileTransferUpdateCallback(),
      base::BindLambdaForTesting([&](bool success) { result = success; }));
  EXPECT_FALSE(result);
}

TEST_F(CryptAuthConnectionTest, RegisterPayloadFile_SucceedsWhenConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);

  EXPECT_CALL(connection,
              RegisterPayloadFileImpl(/*payload_id=*/1234, _, _, _));
  connection.RegisterPayloadFile(
      /*payload_id=*/1234, mojom::PayloadFiles::New(),
      FileTransferUpdateCallback(), base::OnceCallback<void(bool)>());
}

TEST_F(CryptAuthConnectionTest, SetStatus_NotifiesObserversOfStatusChange) {
  StrictMock<MockConnection> connection;
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection.status());

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnConnectionStatusChanged(
                            &connection, Connection::Status::DISCONNECTED,
                            Connection::Status::CONNECTED));
  connection.SetStatus(Connection::Status::CONNECTED);
}

TEST_F(CryptAuthConnectionTest,
       SetStatus_DoesntNotifyObserversIfStatusUnchanged) {
  StrictMock<MockConnection> connection;
  EXPECT_EQ(Connection::Status::DISCONNECTED, connection.status());

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnConnectionStatusChanged(_, _, _)).Times(0);
  connection.SetStatus(Connection::Status::DISCONNECTED);
}

TEST_F(CryptAuthConnectionTest,
       OnDidSendMessage_NotifiesObserversIfMessageSendInProgress) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);
  connection.SendMessage(std::unique_ptr<WireMessage>());

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnSendCompleted(Ref(connection), _, true));
  connection.OnDidSendMessage(TestWireMessage(), true /* success */);
}

TEST_F(CryptAuthConnectionTest,
       OnDidSendMessage_DoesntNotifyObserversIfNoMessageSendInProgress) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(observer, OnSendCompleted(_, _, _)).Times(0);
  connection.OnDidSendMessage(TestWireMessage(), true /* success */);
}

TEST_F(CryptAuthConnectionTest,
       OnBytesReceived_NotifiesObserversOnValidMessage) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  ON_CALL(connection, DeserializeWireMessageProxy(_))
      .WillByDefault(
          DoAll(SetArgPointee<0>(false), Return(new TestWireMessage)));
  EXPECT_CALL(observer, OnMessageReceived(Ref(connection), _));
  connection.OnBytesReceived(std::string());
}

TEST_F(CryptAuthConnectionTest,
       OnBytesReceived_DoesntNotifyObserversIfNotConnected) {
  StrictMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::IN_PROGRESS);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  EXPECT_CALL(connection, GetDeviceAddress()).Times(1);
  EXPECT_CALL(observer, OnMessageReceived(_, _)).Times(0);
  connection.OnBytesReceived(std::string());
}

TEST_F(CryptAuthConnectionTest,
       OnBytesReceived_DoesntNotifyObserversIfMessageIsIncomplete) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  ON_CALL(connection, DeserializeWireMessageProxy(_))
      .WillByDefault(DoAll(SetArgPointee<0>(true), Return(nullptr)));
  EXPECT_CALL(observer, OnMessageReceived(_, _)).Times(0);
  connection.OnBytesReceived(std::string());
}

TEST_F(CryptAuthConnectionTest,
       OnBytesReceived_DoesntNotifyObserversIfMessageIsInvalid) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);

  StrictMock<MockConnectionObserver> observer;
  connection.AddObserver(&observer);

  ON_CALL(connection, DeserializeWireMessageProxy(_))
      .WillByDefault(DoAll(SetArgPointee<0>(false), Return(nullptr)));
  EXPECT_CALL(observer, OnMessageReceived(_, _)).Times(0);
  connection.OnBytesReceived(std::string());
}

TEST_F(CryptAuthConnectionTest, GetConnectionRssi) {
  NiceMock<MockConnection> connection;
  connection.SetStatus(Connection::Status::CONNECTED);
  EXPECT_EQ(std::nullopt, GetRssi(&connection));
}

}  // namespace ash::secure_channel
