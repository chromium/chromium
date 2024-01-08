// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_client_unittest_base.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "dbus/message.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::ByRef;

namespace ash {

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
  raw_ptr<ShillIPConfigClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
};

TEST_F(ShillIPConfigClientTest, PropertyChanged) {
  // Create a signal.
  const base::Value kMtu(100);
  dbus::Signal signal(shill::kFlimflamIPConfigInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kMtuProperty);
  dbus::AppendBasicTypeValueDataAsVariant(&writer, kMtu);

  // Set expectations.
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kMtuProperty, ValueEq(ByRef(kMtu))))
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
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(nullptr);
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
  base::Value::Dict expected_value;
  expected_value.Set(shill::kAddressProperty, kAddress);
  expected_value.Set(shill::kMtuProperty, kMtu);

  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  base::test::TestFuture<std::optional<base::Value::Dict>>
      get_properties_result;
  // Call GetProperties.
  client_->GetProperties(dbus::ObjectPath(kExampleIPConfigPath),
                         get_properties_result.GetCallback());
  std::optional<base::Value::Dict> result = get_properties_result.Take();
  EXPECT_TRUE(result.has_value());
  const base::Value::Dict& result_value = result.value();
  EXPECT_EQ(expected_value, result_value);
}

TEST_F(ShillIPConfigClientTest, SetProperty) {
  const char kAddress[] = "address";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::Value value(kAddress);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::BindRepeating(&ExpectStringAndValueArguments,
                                           shill::kAddressProperty, &value),
                       response.get());
  // Call SetProperty.
  base::test::TestFuture<bool> set_property_result;
  client_->SetProperty(dbus::ObjectPath(kExampleIPConfigPath),
                       shill::kAddressProperty, value,
                       set_property_result.GetCallback());
  EXPECT_TRUE(set_property_result.Get());
}

TEST_F(ShillIPConfigClientTest, ClearProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kClearPropertyFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kAddressProperty),
      response.get());
  // Call ClearProperty.
  base::test::TestFuture<bool> clear_property_result;
  client_->ClearProperty(dbus::ObjectPath(kExampleIPConfigPath),
                         shill::kAddressProperty,
                         clear_property_result.GetCallback());
  EXPECT_TRUE(clear_property_result.Get());
}

TEST_F(ShillIPConfigClientTest, Remove) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRemoveConfigFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  base::test::TestFuture<bool> remove_result;
  // Call Remove.
  client_->Remove(dbus::ObjectPath(kExampleIPConfigPath),
                  remove_result.GetCallback());
  EXPECT_TRUE(remove_result.Get());
}

}  // namespace ash
