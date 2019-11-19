// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_client_unittest_base.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "dbus/message.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::ByRef;

namespace chromeos {

namespace {

const char kExampleIPConfigPath[] = "/foo/bar";

}  // namespace

class ShillIPConfigClientTest : public ShillClientUnittestBase {
 public:
  ShillIPConfigClientTest()
      : ShillClientUnittestBase(shill::kFlimflamIPConfigInterface,
                                dbus::ObjectPath(kExampleIPConfigPath)) {}

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    ShillIPConfigClient::Initialize(mock_bus_.get());
    client_ = ShillIPConfigClient::Get();
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ShillIPConfigClient::Shutdown();
    ShillClientUnittestBase::TearDown();
  }

 protected:
  ShillIPConfigClient* client_ = nullptr;  // Unowned convenience pointer.
};

TEST_F(ShillIPConfigClientTest, PropertyChanged) {
  // Create a signal.
  const base::Value kConnected(true);
  dbus::Signal signal(shill::kFlimflamIPConfigInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kConnectedProperty);
  dbus::AppendBasicTypeValueDataAsVariant(&writer, kConnected);

  // Set expectations.
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer, OnPropertyChanged(shill::kConnectedProperty,
                                          ValueEq(ByRef(kConnected))))
      .Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(dbus::ObjectPath(kExampleIPConfigPath),
                                      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(dbus::ObjectPath(kExampleIPConfigPath),
                                         &observer);

  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillIPConfigClientTest, GetProperties) {
  const char kAddress[] = "address";
  const int32_t kMtu = 68;

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  // Append address.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kAddressProperty);
  entry_writer.AppendVariantOfString(kAddress);
  array_writer.CloseContainer(&entry_writer);
  // Append MTU.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kMtuProperty);
  entry_writer.AppendVariantOfInt32(kMtu);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetKey(shill::kAddressProperty, base::Value(kAddress));
  value.SetKey(shill::kMtuProperty, base::Value(kMtu));

  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument), response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleIPConfigPath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillIPConfigClientTest, SetProperty) {
  const char kAddress[] = "address";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::Value value(kAddress);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  shill::kAddressProperty, &value),
                       response.get());
  // Call method.
  client_->SetProperty(dbus::ObjectPath(kExampleIPConfigPath),
                       shill::kAddressProperty, value,
                       base::BindOnce(&ExpectNoResultValue));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillIPConfigClientTest, ClearProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kClearPropertyFunction,
      base::Bind(&ExpectStringArgument, shill::kAddressProperty),
      response.get());
  // Call method.
  client_->ClearProperty(dbus::ObjectPath(kExampleIPConfigPath),
                         shill::kAddressProperty,
                         base::BindOnce(&ExpectNoResultValue));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillIPConfigClientTest, Remove) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRemoveConfigFunction,
                       base::Bind(&ExpectNoArgument), response.get());
  // Call method.
  client_->Remove(dbus::ObjectPath(kExampleIPConfigPath),
                  base::BindOnce(&ExpectNoResultValue));

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
