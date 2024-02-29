// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_device_client.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_client_unittest_base.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::ByRef;

namespace ash {

namespace {

const char kExampleDevicePath[] = "/foo/bar";

// Expects the reader to have a string and a bool.
void ExpectStringAndBoolArguments(const std::string& expected_string,
                                  bool expected_bool,
                                  dbus::MessageReader* reader) {
  std::string arg1;
  ASSERT_TRUE(reader->PopString(&arg1));
  EXPECT_EQ(expected_string, arg1);
  bool arg2 = false;
  ASSERT_TRUE(reader->PopBool(&arg2));
  EXPECT_EQ(expected_bool, arg2);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expects the reader to have two strings.
void ExpectTwoStringArguments(const std::string& expected_string1,
                              const std::string& expected_string2,
                              dbus::MessageReader* reader) {
  std::string arg1;
  ASSERT_TRUE(reader->PopString(&arg1));
  EXPECT_EQ(expected_string1, arg1);
  std::string arg2;
  ASSERT_TRUE(reader->PopString(&arg2));
  EXPECT_EQ(expected_string2, arg2);
  EXPECT_FALSE(reader->HasMoreData());
}

}  // namespace

class ShillDeviceClientTest : public ShillClientUnittestBase {
 public:
  ShillDeviceClientTest()
      : ShillClientUnittestBase(shill::kFlimflamDeviceInterface,
                                dbus::ObjectPath(kExampleDevicePath)) {}

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    ShillDeviceClient::Initialize(mock_bus_.get());
    client_ = ShillDeviceClient::Get();
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ShillDeviceClient::Shutdown();
    ShillClientUnittestBase::TearDown();
  }

 protected:
  raw_ptr<ShillDeviceClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
};

TEST_F(ShillDeviceClientTest, PropertyChanged) {
  const bool kValue = true;
  // Create a signal.
  dbus::Signal signal(shill::kFlimflamDeviceInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kCellularPolicyAllowRoamingProperty);
  writer.AppendVariantOfBool(kValue);

  // Set expectations.
  const base::Value value(kValue);
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kCellularPolicyAllowRoamingProperty,
                                ValueEq(ByRef(value))))
      .Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(dbus::ObjectPath(kExampleDevicePath),
                                      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(dbus::ObjectPath(kExampleDevicePath),
                                         &observer);

  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kCellularPolicyAllowRoamingProperty,
                                ValueEq(ByRef(value))))
      .Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillDeviceClientTest, GetProperties) {
  const bool kValue = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kCellularPolicyAllowRoamingProperty);
  entry_writer.AppendVariantOfBool(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::Value::Dict expected_value;
  expected_value.Set(shill::kCellularPolicyAllowRoamingProperty, kValue);
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call GetProperties.
  base::test::TestFuture<std::optional<base::Value::Dict>>
      properties_result_future;
  client_->GetProperties(dbus::ObjectPath(kExampleDevicePath),
                         properties_result_future.GetCallback());
  std::optional<base::Value::Dict> result = properties_result_future.Take();
  EXPECT_TRUE(result.has_value());
  const base::Value::Dict& result_value = result.value();
  EXPECT_EQ(expected_value, result_value);
}

TEST_F(ShillDeviceClientTest, SetProperty) {
  const bool kValue = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::Value value(kValue);
  PrepareForMethodCall(
      shill::kSetPropertyFunction,
      base::BindRepeating(&ExpectStringAndValueArguments,
                          shill::kCellularPolicyAllowRoamingProperty, &value),
      response.get());
  // Call SetProperty.
  base::test::TestFuture<void> set_property_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->SetProperty(
      dbus::ObjectPath(kExampleDevicePath),
      shill::kCellularPolicyAllowRoamingProperty, value,
      set_property_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(set_property_result.Wait());
  // The SetProperty() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillDeviceClientTest, ClearProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kClearPropertyFunction,
      base::BindRepeating(&ExpectStringArgument,
                          shill::kCellularPolicyAllowRoamingProperty),
      response.get());
  // Call ClearProperty.
  base::test::TestFuture<bool> clear_property_result;
  client_->ClearProperty(dbus::ObjectPath(kExampleDevicePath),
                         shill::kCellularPolicyAllowRoamingProperty,
                         clear_property_result.GetCallback());
  EXPECT_TRUE(clear_property_result.Get());
}

TEST_F(ShillDeviceClientTest, RequirePin) {
  const char kPin[] = "123456";
  const bool kRequired = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kRequirePinFunction,
      base::BindRepeating(&ExpectStringAndBoolArguments, kPin, kRequired),
      response.get());

  base::test::TestFuture<void> require_pin_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call RequirePin.
  client_->RequirePin(
      dbus::ObjectPath(kExampleDevicePath), kPin, kRequired,
      require_pin_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(require_pin_result.Wait());
  // The RequirePin() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillDeviceClientTest, EnterPin) {
  const char kPin[] = "123456";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::test::TestFuture<void> enter_pin_result;
  base::test::TestFuture<std::string, std::string> error_result;
  PrepareForMethodCall(shill::kEnterPinFunction,
                       base::BindRepeating(&ExpectStringArgument, kPin),
                       response.get());

  // Call EnterPin.
  client_->EnterPin(
      dbus::ObjectPath(kExampleDevicePath), kPin,
      enter_pin_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(enter_pin_result.Wait());
  // The EnterPin() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillDeviceClientTest, UnblockPin) {
  const char kPuk[] = "987654";
  const char kPin[] = "123456";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::test::TestFuture<void> unblock_pin_result;
  base::test::TestFuture<std::string, std::string> error_result;
  PrepareForMethodCall(
      shill::kUnblockPinFunction,
      base::BindRepeating(&ExpectTwoStringArguments, kPuk, kPin),
      response.get());

  // Call UnblockPin.
  client_->UnblockPin(
      dbus::ObjectPath(kExampleDevicePath), kPuk, kPin,
      unblock_pin_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(unblock_pin_result.Wait());
  // The UnblockPin() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillDeviceClientTest, ChangePin) {
  const char kOldPin[] = "123456";
  const char kNewPin[] = "234567";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kChangePinFunction,
      base::BindRepeating(&ExpectTwoStringArguments, kOldPin, kNewPin),
      response.get());

  base::test::TestFuture<void> change_pin_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call ChangePin.
  client_->ChangePin(
      dbus::ObjectPath(kExampleDevicePath), kOldPin, kNewPin,
      change_pin_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(change_pin_result.Wait());
  // The ChangePin() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillDeviceClientTest, Register) {
  const char kNetworkId[] = "networkid";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRegisterFunction,
                       base::BindRepeating(&ExpectStringArgument, kNetworkId),
                       response.get());

  base::test::TestFuture<void> register_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call Register.
  client_->Register(
      dbus::ObjectPath(kExampleDevicePath), kNetworkId,
      register_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(register_result.Wait());
  // The Register() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillDeviceClientTest, Reset) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kResetFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  base::test::TestFuture<void> reset_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call Reset.
  client_->Reset(
      dbus::ObjectPath(kExampleDevicePath), reset_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(reset_result.Wait());
  // The Reset() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

}  // namespace ash
