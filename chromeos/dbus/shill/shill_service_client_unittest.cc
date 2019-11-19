// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_client_unittest_base.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::ByRef;

namespace chromeos {

namespace {

const char kExampleServicePath[] = "/foo/bar";

}  // namespace

class ShillServiceClientTest : public ShillClientUnittestBase {
 public:
  ShillServiceClientTest()
      : ShillClientUnittestBase(shill::kFlimflamServiceInterface,
                                dbus::ObjectPath(kExampleServicePath)) {}

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    ShillServiceClient::Initialize(mock_bus_.get());
    client_ = ShillServiceClient::Get();
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ShillServiceClient::Shutdown();
    ShillClientUnittestBase::TearDown();
  }

 protected:
  ShillServiceClient* client_ = nullptr;  // Unowned convenience pointer.
};

TEST_F(ShillServiceClientTest, PropertyChanged) {
  const int kValue = 42;
  // Create a signal.
  dbus::Signal signal(shill::kFlimflamServiceInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kSignalStrengthProperty);
  writer.AppendVariantOfByte(kValue);

  // Set expectations.
  const base::Value value(kValue);
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer, OnPropertyChanged(shill::kSignalStrengthProperty,
                                          ValueEq(ByRef(value))))
      .Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(dbus::ObjectPath(kExampleServicePath),
                                      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(dbus::ObjectPath(kExampleServicePath),
                                         &observer);

  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillServiceClientTest, GetProperties) {
  const int kValue = 42;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kSignalStrengthProperty);
  entry_writer.AppendVariantOfByte(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::DictionaryValue value;
  value.SetKey(shill::kSignalStrengthProperty, base::Value(kValue));
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument), response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleServicePath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, SetProperty) {
  const char kValue[] = "passphrase";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::Value value(kValue);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  shill::kPassphraseProperty, &value),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->SetProperty(dbus::ObjectPath(kExampleServicePath),
                       shill::kPassphraseProperty, value, mock_closure.Get(),
                       mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, SetProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  std::unique_ptr<base::DictionaryValue> arg(CreateExampleServiceProperties());
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  PrepareForMethodCall(
      shill::kSetPropertiesFunction,
      base::Bind(&ExpectDictionaryValueArgument, arg.get(), string_valued),
      response.get());

  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->SetProperties(dbus::ObjectPath(kExampleServicePath), *arg,
                         mock_closure.Get(), mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, ClearProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kClearPropertyFunction,
      base::Bind(&ExpectStringArgument, shill::kPassphraseProperty),
      response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->ClearProperty(dbus::ObjectPath(kExampleServicePath),
                         shill::kPassphraseProperty, mock_closure.Get(),
                         mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, ClearProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("b", &array_writer);
  array_writer.AppendBool(true);
  array_writer.AppendBool(true);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  std::vector<std::string> keys;
  keys.push_back(shill::kPassphraseProperty);
  keys.push_back(shill::kSignalStrengthProperty);
  PrepareForMethodCall(shill::kClearPropertiesFunction,
                       base::Bind(&ExpectArrayOfStringsArgument, keys),
                       response.get());
  // Call method.
  base::MockCallback<ShillServiceClient::ListValueCallback>
      mock_list_value_callback;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->ClearProperties(dbus::ObjectPath(kExampleServicePath), keys,
                           mock_list_value_callback.Get(),
                           mock_error_callback.Get());
  EXPECT_CALL(mock_list_value_callback, Run(_)).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, Connect) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kConnectFunction, base::Bind(&ExpectNoArgument),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  // Call method.
  client_->Connect(dbus::ObjectPath(kExampleServicePath), mock_closure.Get(),
                   mock_error_callback.Get());

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, Disconnect) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kDisconnectFunction,
                       base::Bind(&ExpectNoArgument), response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->Disconnect(dbus::ObjectPath(kExampleServicePath), mock_closure.Get(),
                      mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, Remove) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRemoveServiceFunction,
                       base::Bind(&ExpectNoArgument), response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->Remove(dbus::ObjectPath(kExampleServicePath), mock_closure.Get(),
                  mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillServiceClientTest, ActivateCellularModem) {
  const char kCarrier[] = "carrier";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kActivateCellularModemFunction,
                       base::Bind(&ExpectStringArgument, kCarrier),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillServiceClient::ErrorCallback> mock_error_callback;
  client_->ActivateCellularModem(dbus::ObjectPath(kExampleServicePath),
                                 kCarrier, mock_closure.Get(),
                                 mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
