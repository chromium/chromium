// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/shill/modem_3gpp_client.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

namespace {

// D-Bus service name used by test.
const char kServiceName[] = "service.name";

// D-Bus object path used by test.
const char kObjectPath[] = "/object/path";

// Lock configuration used by test.
constexpr char kCarrierLockConfig[] = "carrier.lock.configuration";

}  // namespace

class Modem3gppClientTest : public testing::Test {
 public:
  Modem3gppClientTest() {}

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock proxy.
    mock_proxy_ = new dbus::MockObjectProxy(mock_bus_.get(), kServiceName,
                                            dbus::ObjectPath(kObjectPath));

    // Set an expectation so mock_proxy's ConnectToSignal() will use
    // OnConnectToSignal() to run the callback.
    EXPECT_CALL(*mock_proxy_.get(),
                DoConnectToSignal(modemmanager::kModemManager13gppInterface,
                                  modemmanager::kModem3gppSetCarrierLock, _, _))
        .WillRepeatedly(Invoke(this, &Modem3gppClientTest::OnConnectToSignal));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kServiceName, dbus::ObjectPath(kObjectPath)))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    Modem3gppClient::Initialize(mock_bus_.get());
    client_ = Modem3gppClient::Get();
  }

  void TearDown() override {
    client_ = nullptr;
    mock_bus_->ShutdownAndBlock();
    Modem3gppClient::Shutdown();
  }

  // Handles SetCarrierLock method call.
  void OnSetCarrierLock(dbus::MethodCall* method_call,
                        int timeout_ms,
                        dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
    const uint8_t* configuration;
    size_t conf_len;

    EXPECT_EQ(modemmanager::kModemManager13gppInterface,
              method_call->GetInterface());
    EXPECT_EQ(modemmanager::kModem3gppSetCarrierLock, method_call->GetMember());

    dbus::MessageReader reader(method_call);
    EXPECT_TRUE(reader.PopArrayOfBytes(&configuration, &conf_len));
    EXPECT_EQ(conf_len, expected_configuration_.size());
    for (size_t i = 0; i < conf_len; i++) {
      EXPECT_EQ(expected_configuration_.c_str()[i], (char)configuration[i]);
    }
    EXPECT_FALSE(reader.HasMoreData());

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_.get(),
                                  error_response_.get()));
  }

 protected:
  raw_ptr<Modem3gppClient> client_ = nullptr;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;

  // The 3gppReceived signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback received_callback_;

  // Expected argument for SetCarrierLock method.
  std::string expected_configuration_;

  // Response returned by mock methods.
  std::unique_ptr<dbus::Response> response_;
  std::unique_ptr<dbus::ErrorResponse> error_response_;

 private:
  // Used to implement the mock proxy.
  void OnConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    received_callback_ = signal_callback;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*on_connected_callback), interface_name,
                       signal_name, /*success=*/true));
  }
};

TEST_F(Modem3gppClientTest, SetCarrierLockSuccess) {
  // Set expectations.
  expected_configuration_ = kCarrierLockConfig;
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(Invoke(this, &Modem3gppClientTest::OnSetCarrierLock));

  // Create response.
  response_ = dbus::Response::CreateEmpty();

  // Call SetCarrierLock.
  base::test::TestFuture<CarrierLockResult> set_carrier_lock_future;
  client_->SetCarrierLock(kServiceName, dbus::ObjectPath(kObjectPath),
                          expected_configuration_,
                          set_carrier_lock_future.GetCallback());

  EXPECT_EQ(CarrierLockResult::kSuccess, set_carrier_lock_future.Get());
}

TEST_F(Modem3gppClientTest, SetCarrierLockFailure) {
  const std::vector<std::pair<CarrierLockResult, std::string>> all_errors = {
      {CarrierLockResult::kUnknownError, "org.error"},
      {CarrierLockResult::kInvalidSignature, "org.InvalidSignature"},
      {CarrierLockResult::kInvalidImei, "org.InvalidImei"},
      {CarrierLockResult::kInvalidTimeStamp, "org.InvalidTimestamp"},
      {CarrierLockResult::kNetworkListTooLarge, "org.NetworkListTooLarge"},
      {CarrierLockResult::kAlgorithmNotSupported, "org.AlgorithmNotSupported"},
      {CarrierLockResult::kFeatureNotSupported, "org.FeatureNotSupported"},
      {CarrierLockResult::kDecodeOrParsingError, "org.DecodeOrParsingError"},
      {CarrierLockResult::kOperationNotSupported, "org.Unsupported"},
  };

  // Set expectations.
  expected_configuration_ = kCarrierLockConfig;
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(this, &Modem3gppClientTest::OnSetCarrierLock));

  // Create response.
  error_response_ = dbus::ErrorResponse::FromRawMessage(
      dbus_message_new(DBUS_MESSAGE_TYPE_ERROR));

  for (std::pair<CarrierLockResult, std::string> error : all_errors) {
    error_response_->SetErrorName(error.second);

    // Call SetCarrierLock.
    base::test::TestFuture<CarrierLockResult> set_carrier_lock_future;
    client_->SetCarrierLock(kServiceName, dbus::ObjectPath(kObjectPath),
                            expected_configuration_,
                            set_carrier_lock_future.GetCallback());

    EXPECT_EQ(error.first, set_carrier_lock_future.Get());
  }
}

TEST_F(Modem3gppClientTest, CallSetCarrierLockTwice) {
  // Set expectations.
  expected_configuration_ = kCarrierLockConfig;
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillOnce(Invoke(this, &Modem3gppClientTest::OnSetCarrierLock))
      .WillOnce(Invoke(this, &Modem3gppClientTest::OnSetCarrierLock));

  // Create response.
  response_ = dbus::Response::CreateEmpty();

  // Call SetCarrierLock.
  base::test::TestFuture<CarrierLockResult> set_carrier_lock_future1;
  client_->SetCarrierLock(kServiceName, dbus::ObjectPath(kObjectPath),
                          expected_configuration_,
                          set_carrier_lock_future1.GetCallback());

  // Call SetCarrierLock again.
  base::test::TestFuture<CarrierLockResult> set_carrier_lock_future2;
  client_->SetCarrierLock(kServiceName, dbus::ObjectPath(kObjectPath),
                          expected_configuration_,
                          set_carrier_lock_future2.GetCallback());

  EXPECT_EQ(CarrierLockResult::kSuccess, set_carrier_lock_future1.Get());
  EXPECT_EQ(CarrierLockResult::kSuccess, set_carrier_lock_future2.Get());
}

}  // namespace ash
