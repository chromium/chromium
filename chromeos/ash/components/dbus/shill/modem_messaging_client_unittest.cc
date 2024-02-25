// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/modem_messaging_client.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
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

// A mock SmsReceivedHandler.
class MockSmsReceivedHandler {
 public:
  MOCK_METHOD2(Run, void(const dbus::ObjectPath& sms, bool complete));
};

// D-Bus service name used by test.
const char kServiceName[] = "service.name";
// D-Bus object path used by test.
const char kObjectPath[] = "/object/path";

}  // namespace

class ModemMessagingClientTest : public testing::Test {
 public:
  ModemMessagingClientTest() = default;

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
    EXPECT_CALL(
        *mock_proxy_.get(),
        DoConnectToSignal(modemmanager::kModemManager1MessagingInterface,
                          modemmanager::kSMSAddedSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &ModemMessagingClientTest::OnConnectToSignal));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kServiceName, dbus::ObjectPath(kObjectPath)))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    ModemMessagingClient::Initialize(mock_bus_.get());
    client_ = ModemMessagingClient::Get();
  }

  void TearDown() override {
    mock_bus_->ShutdownAndBlock();
    ModemMessagingClient::Shutdown();
  }

  // Handles Delete method call.
  void OnDelete(dbus::MethodCall* method_call,
                int timeout_ms,
                dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(modemmanager::kModemManager1MessagingInterface,
              method_call->GetInterface());
    EXPECT_EQ(modemmanager::kSMSDeleteFunction, method_call->GetMember());
    dbus::ObjectPath sms_path;
    dbus::MessageReader reader(method_call);
    EXPECT_TRUE(reader.PopObjectPath(&sms_path));
    EXPECT_EQ(expected_sms_path_, sms_path);
    EXPECT_FALSE(reader.HasMoreData());

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  // Handles List method call.
  void OnList(dbus::MethodCall* method_call,
              int timeout_ms,
              dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(modemmanager::kModemManager1MessagingInterface,
              method_call->GetInterface());
    EXPECT_EQ(modemmanager::kSMSListFunction, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    EXPECT_FALSE(reader.HasMoreData());

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

 protected:
  raw_ptr<ModemMessagingClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock object proxy.
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // The SmsReceived signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback sms_received_callback_;
  // Expected argument for Delete method.
  dbus::ObjectPath expected_sms_path_;
  // Response returned by mock methods.
  raw_ptr<dbus::Response, DanglingUntriaged> response_ = nullptr;

 private:
  // Used to implement the mock proxy.
  void OnConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    sms_received_callback_ = signal_callback;
    const bool success = true;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }
};

TEST_F(ModemMessagingClientTest, SmsReceived) {
  // Set expectations.
  const dbus::ObjectPath kSmsPath("/SMS/0");
  const bool kComplete = true;
  MockSmsReceivedHandler handler;
  EXPECT_CALL(handler, Run(kSmsPath, kComplete)).Times(1);
  // Set handler.
  client_->SetSmsReceivedHandler(
      kServiceName, dbus::ObjectPath(kObjectPath),
      base::BindRepeating(&MockSmsReceivedHandler::Run,
                          base::Unretained(&handler)));

  // Run the message loop to run the signal connection result callback.
  base::RunLoop().RunUntilIdle();

  // Send signal.
  dbus::Signal signal(modemmanager::kModemManager1MessagingInterface,
                      modemmanager::kSMSAddedSignal);
  dbus::MessageWriter writer(&signal);
  writer.AppendObjectPath(kSmsPath);
  writer.AppendBool(kComplete);
  ASSERT_FALSE(sms_received_callback_.is_null());
  sms_received_callback_.Run(&signal);
  // Reset handler.
  client_->ResetSmsReceivedHandler(kServiceName, dbus::ObjectPath(kObjectPath));
  // Send signal again.
  sms_received_callback_.Run(&signal);
}

TEST_F(ModemMessagingClientTest, Delete) {
  // Set expectations.
  const dbus::ObjectPath kSmsPath("/SMS/0");
  expected_sms_path_ = kSmsPath;
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillOnce(Invoke(this, &ModemMessagingClientTest::OnDelete));

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();
  // Call Delete.
  base::test::TestFuture<bool> delete_result_future;
  client_->Delete(kServiceName, dbus::ObjectPath(kObjectPath), kSmsPath,
                  delete_result_future.GetCallback());

  EXPECT_TRUE(delete_result_future.Get());
}

TEST_F(ModemMessagingClientTest, List) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillOnce(Invoke(this, &ModemMessagingClientTest::OnList));

  const std::vector<dbus::ObjectPath> kExpectedResult{
      dbus::ObjectPath("/SMS/1"), dbus::ObjectPath("/SMS/2")};

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfObjectPaths(kExpectedResult);
  response_ = response.get();

  // Call List.
  base::test::TestFuture<std::optional<std::vector<dbus::ObjectPath>>>
      list_result_future;
  client_->List(kServiceName, dbus::ObjectPath(kObjectPath),
                list_result_future.GetCallback());

  std::optional<std::vector<dbus::ObjectPath>> result =
      list_result_future.Take();
  EXPECT_TRUE(result);
  EXPECT_EQ(kExpectedResult, result.value());
}

}  // namespace ash
