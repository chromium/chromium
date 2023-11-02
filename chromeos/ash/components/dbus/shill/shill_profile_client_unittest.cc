// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_client_unittest_base.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;

namespace ash {

namespace {

const char kDefaultProfilePath[] = "/profile/default";
const char kExampleEntryPath[] = "example_entry_path";

void AppendVariantOfArrayOfStrings(dbus::MessageWriter* writer,
                                   const std::vector<std::string>& strings) {
  dbus::MessageWriter variant_writer(NULL);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(strings);
  writer->CloseContainer(&variant_writer);
}

}  // namespace

class ShillProfileClientTest : public ShillClientUnittestBase {
 public:
  ShillProfileClientTest()
      : ShillClientUnittestBase(shill::kFlimflamProfileInterface,
                                dbus::ObjectPath(kDefaultProfilePath)) {}

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    ShillProfileClient::Initialize(mock_bus_.get());
    client_ = ShillProfileClient::Get();
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ShillProfileClient::Shutdown();
    ShillClientUnittestBase::TearDown();
  }

 protected:
  ShillProfileClient* client_ = nullptr;  // Unowned convenience pointer.
};

TEST_F(ShillProfileClientTest, PropertyChanged) {
  // Create a signal.
  dbus::Signal signal(shill::kFlimflamProfileInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&writer,
                                std::vector<std::string>(1, kExampleEntryPath));

  // Set expectations.
  base::ListValue value;
  value.Append(kExampleEntryPath);
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kEntriesProperty, ValueEq(value)))
      .Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(dbus::ObjectPath(kDefaultProfilePath),
                                      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(dbus::ObjectPath(kDefaultProfilePath),
                                         &observer);

  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillProfileClientTest, GetProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&entry_writer,
                                std::vector<std::string>(1, kExampleEntryPath));
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::Value entries(base::Value::Type::LIST);
  entries.Append(kExampleEntryPath);
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(shill::kEntriesProperty, std::move(entries));
  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::MockCallback<ShillProfileClient::ErrorCallback> error_callback;
  client_->GetProperties(
      dbus::ObjectPath(kDefaultProfilePath),
      base::BindOnce(&ExpectValueResultWithoutStatus, &value),
      error_callback.Get());
  EXPECT_CALL(error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillProfileClientTest, GetEntry) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kTypeProperty);
  entry_writer.AppendVariantOfString(shill::kTypeWifi);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  // Set expectations.
  PrepareForMethodCall(
      shill::kGetEntryFunction,
      base::BindRepeating(&ExpectStringArgument, kExampleEntryPath),
      response.get());
  // Call method.
  base::MockCallback<ShillProfileClient::ErrorCallback> error_callback;
  client_->GetEntry(dbus::ObjectPath(kDefaultProfilePath), kExampleEntryPath,
                    base::BindOnce(&ExpectValueResultWithoutStatus, &value),
                    error_callback.Get());
  EXPECT_CALL(error_callback, Run(_, _)).Times(0);
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillProfileClientTest, DeleteEntry) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Create the expected value.
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(shill::kArpGatewayProperty, base::Value(true));
  // Set expectations.
  PrepareForMethodCall(
      shill::kDeleteEntryFunction,
      base::BindRepeating(&ExpectStringArgument, kExampleEntryPath),
      response.get());
  // Call method.
  base::MockCallback<base::OnceClosure> mock_closure;
  base::MockCallback<ShillProfileClient::ErrorCallback> mock_error_callback;
  client_->DeleteEntry(dbus::ObjectPath(kDefaultProfilePath), kExampleEntryPath,
                       mock_closure.Get(), mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
