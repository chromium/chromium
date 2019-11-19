// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/oobe_configuration_client.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/oobe_config/oobe_config.pb.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

// A mock ConfigurationCallback
class MockConfigurationCallback {
 public:
  MOCK_METHOD2(Run, void(bool, const std::string&));
};

// Expect the reader to be empty.
void ExpectNoArgument(dbus::MessageReader* reader) {
  EXPECT_FALSE(reader->HasMoreData());
}

}  // namespace

class OobeConfigurationClientTest : public testing::Test {
 public:
  OobeConfigurationClientTest()
      : interface_name_(oobe_config::kOobeConfigRestoreInterface),
        response_(nullptr) {}

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock oobe config proxy.
    mock_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(), oobe_config::kOobeConfigRestoreServiceName,
        dbus::ObjectPath(oobe_config::kOobeConfigRestoreServicePath));

    // Set an expectation so mock_proxy's CallMethod() will use
    // OnCallMethod() to return responses.
    EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
        .WillRepeatedly(
            Invoke(this, &OobeConfigurationClientTest::OnCallMethod));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(oobe_config::kOobeConfigRestoreServiceName,
                               dbus::ObjectPath(
                                   oobe_config::kOobeConfigRestoreServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    client_ = OobeConfigurationClient::Create();
    client_->Init(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { mock_bus_->ShutdownAndBlock(); }

 protected:
  // A callback to intercept and check the method call arguments.
  typedef base::RepeatingCallback<void(dbus::MessageReader* reader)>
      ArgumentCheckCallback;

  // Sets expectations for called method name and arguments, and sets response.
  void PrepareForMethodCall(const std::string& method_name,
                            const ArgumentCheckCallback& argument_checker,
                            dbus::Response* response) {
    expected_method_name_ = method_name;
    argument_checker_ = argument_checker;
    response_ = response;
  }

  // The interface name.
  const std::string interface_name_;
  // The client to be tested.
  std::unique_ptr<OobeConfigurationClient> client_;
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock object proxy.
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;

  // The name of the method which is expected to be called.
  std::string expected_method_name_;
  // The response which the mock proxy returns.
  dbus::Response* response_;
  // A callback to intercept and check the method call arguments.
  ArgumentCheckCallback argument_checker_;

 private:
  // Checks the content of the method call and returns the response.
  // Used to implement the mock oobe config proxy.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* response) {
    EXPECT_EQ(interface_name_, method_call->GetInterface());
    EXPECT_EQ(expected_method_name_, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    argument_checker_.Run(&reader);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*response), response_));
  }
};

TEST_F(OobeConfigurationClientTest, CheckEmptyConfiguration) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  oobe_config::OobeRestoreData proto;
  proto.set_chrome_config_json(std::string());
  dbus::MessageWriter writer(response.get());
  writer.AppendInt32(0);
  writer.AppendProtoAsArrayOfBytes(proto);

  // Set expectations.
  PrepareForMethodCall(oobe_config::kProcessAndGetOobeAutoConfigMethod,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  MockConfigurationCallback callback;
  EXPECT_CALL(callback, Run(false, _)).Times(1);

  client_->CheckForOobeConfiguration(base::BindOnce(
      &MockConfigurationCallback::Run, base::Unretained(&callback)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(OobeConfigurationClientTest, CheckServiceError) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  oobe_config::OobeRestoreData proto;
  proto.set_chrome_config_json("{key:true}");
  dbus::MessageWriter writer(response.get());
  writer.AppendInt32(1);
  writer.AppendProtoAsArrayOfBytes(proto);

  // Set expectations.
  PrepareForMethodCall(oobe_config::kProcessAndGetOobeAutoConfigMethod,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  MockConfigurationCallback callback;
  EXPECT_CALL(callback, Run(false, _)).Times(1);

  client_->CheckForOobeConfiguration(base::BindOnce(
      &MockConfigurationCallback::Run, base::Unretained(&callback)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(OobeConfigurationClientTest, CheckConfigurationExists) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  oobe_config::OobeRestoreData proto;
  proto.set_chrome_config_json("{key:true}");
  dbus::MessageWriter writer(response.get());
  writer.AppendInt32(0);
  writer.AppendProtoAsArrayOfBytes(proto);

  // Set expectations.
  PrepareForMethodCall(oobe_config::kProcessAndGetOobeAutoConfigMethod,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  MockConfigurationCallback callback;
  EXPECT_CALL(callback, Run(true, "{key:true}")).Times(1);

  client_->CheckForOobeConfiguration(base::BindOnce(
      &MockConfigurationCallback::Run, base::Unretained(&callback)));

  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
