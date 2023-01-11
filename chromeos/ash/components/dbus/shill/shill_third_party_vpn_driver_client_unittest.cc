// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/dbus/shill/shill_client_unittest_base.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;

namespace ash {

namespace {

const char kExampleIPConfigPath[] = "/foo/bar";

class MockShillThirdPartyVpnObserver : public ShillThirdPartyVpnObserver {
 public:
  MockShillThirdPartyVpnObserver() = default;
  ~MockShillThirdPartyVpnObserver() override = default;
  MOCK_METHOD1(OnPacketReceived, void(const std::vector<char>& data));
  MOCK_METHOD1(OnPlatformMessage, void(uint32_t message));
};

}  // namespace

class ShillThirdPartyVpnDriverClientTest : public ShillClientUnittestBase {
 public:
  ShillThirdPartyVpnDriverClientTest()
      : ShillClientUnittestBase(shill::kFlimflamThirdPartyVpnInterface,
                                dbus::ObjectPath(kExampleIPConfigPath)) {}

  void SetUp() override {
    ShillClientUnittestBase::SetUp();

    // Create a client with the mock bus.
    ShillThirdPartyVpnDriverClient::Initialize(mock_bus_.get());
    client_ = ShillThirdPartyVpnDriverClient::Get();
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ShillThirdPartyVpnDriverClient::Shutdown();
    ShillClientUnittestBase::TearDown();
  }

  MOCK_METHOD0(MockSuccess, void());
  MOCK_METHOD1(MockSuccessWithWarning, void(const std::string& warning));
  static void Failure(const std::string& error_name,
                      const std::string& error_message) {
    ADD_FAILURE() << error_name << ": " << error_message;
  }

 protected:
  ShillThirdPartyVpnDriverClient* client_ = nullptr;  // Unowned
};

TEST_F(ShillThirdPartyVpnDriverClientTest, PlatformSignal) {
  uint32_t connected_state = 123456;
  const size_t kPacketSize = 5;
  std::vector<char> data_packet(kPacketSize, 1);
  dbus::Signal pmessage_signal(shill::kFlimflamThirdPartyVpnInterface,
                               shill::kOnPlatformMessageFunction);
  {
    dbus::MessageWriter writer(&pmessage_signal);
    writer.AppendUint32(connected_state);
  }

  dbus::Signal preceived_signal(shill::kFlimflamThirdPartyVpnInterface,
                                shill::kOnPacketReceivedFunction);
  {
    dbus::MessageWriter writer(&preceived_signal);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(data_packet.data()),
        data_packet.size());
  }

  // Expect each signal to be triggered once.
  MockShillThirdPartyVpnObserver observer;
  EXPECT_CALL(observer, OnPlatformMessage(connected_state)).Times(1);
  EXPECT_CALL(observer, OnPacketReceived(data_packet)).Times(1);

  client_->AddShillThirdPartyVpnObserver(kExampleIPConfigPath, &observer);

  // Run the signal callback.
  SendPlatformMessageSignal(&pmessage_signal);
  SendPacketReceievedSignal(&preceived_signal);

  testing::Mock::VerifyAndClearExpectations(&observer);

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  uint32_t connection_state = 2;

  PrepareForMethodCall(
      shill::kUpdateConnectionStateFunction,
      base::BindRepeating(&ExpectUint32Argument, connection_state),
      response.get());

  EXPECT_CALL(*this, MockSuccess()).Times(0);
  client_->UpdateConnectionState(
      kExampleIPConfigPath, connection_state,
      base::BindOnce(&ShillThirdPartyVpnDriverClientTest::MockSuccess,
                     base::Unretained(this)),
      base::BindOnce(&Failure));

  client_->RemoveShillThirdPartyVpnObserver(kExampleIPConfigPath);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, MockSuccess()).Times(1);

  // Check after removing the observer that there is no further signals.
  EXPECT_CALL(observer, OnPlatformMessage(connected_state)).Times(0);
  EXPECT_CALL(observer, OnPacketReceived(data_packet)).Times(0);

  // Run the signal callback.
  SendPlatformMessageSignal(&pmessage_signal);
  SendPacketReceievedSignal(&preceived_signal);

  testing::Mock::VerifyAndClearExpectations(&observer);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillThirdPartyVpnDriverClientTest, SetParameters) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(std::string("deadbeef"));

  base::Value parameters(base::Value::Type::DICTIONARY);
  const std::string kAddress("1.1.1.1");
  parameters.SetKey(shill::kAddressParameterThirdPartyVpn,
                    base::Value(kAddress));

  EXPECT_CALL(*this, MockSuccessWithWarning(std::string("deadbeef"))).Times(1);

  PrepareForMethodCall(
      shill::kSetParametersFunction,
      base::BindRepeating(&ExpectValueDictionaryArgument, &parameters, true),
      response.get());

  client_->SetParameters(
      kExampleIPConfigPath, parameters,
      base::BindOnce(
          &ShillThirdPartyVpnDriverClientTest::MockSuccessWithWarning,
          base::Unretained(this)),
      base::BindOnce(&Failure));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillThirdPartyVpnDriverClientTest, UpdateConnectionState) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  uint32_t connection_state = 2;

  EXPECT_CALL(*this, MockSuccess()).Times(1);

  PrepareForMethodCall(
      shill::kUpdateConnectionStateFunction,
      base::BindRepeating(&ExpectUint32Argument, connection_state),
      response.get());

  client_->UpdateConnectionState(
      kExampleIPConfigPath, connection_state,
      base::BindOnce(&ShillThirdPartyVpnDriverClientTest::MockSuccess,
                     base::Unretained(this)),
      base::BindOnce(&Failure));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillThirdPartyVpnDriverClientTest, SendPacket) {
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  const size_t kPacketSize = 5;
  const std::vector<char> data(kPacketSize, 0);

  EXPECT_CALL(*this, MockSuccess()).Times(1);

  PrepareForMethodCall(
      shill::kSendPacketFunction,
      base::BindRepeating(&ExpectArrayOfBytesArgument,
                          std::string(data.begin(), data.end())),
      response.get());

  client_->SendPacket(
      kExampleIPConfigPath, data,
      base::BindOnce(&ShillThirdPartyVpnDriverClientTest::MockSuccess,
                     base::Unretained(this)),
      base::BindOnce(&Failure));

  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
