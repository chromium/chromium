// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
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
  dbus::MessageWriter variant_writer(nullptr);
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
  raw_ptr<ShillProfileClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
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
  base::Value::List value;
  value.Append(kExampleEntryPath);
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kEntriesProperty,
                                ValueEq(base::Value(std::move(value)))))
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
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(nullptr);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&entry_writer,
                                std::vector<std::string>(1, kExampleEntryPath));
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::Value::List entries;
  entries.Append(kExampleEntryPath);
  base::Value::Dict expected_value;
  expected_value.Set(shill::kEntriesProperty, std::move(entries));
  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<base::Value::Dict> get_properties_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->GetProperties(
      dbus::ObjectPath(kDefaultProfilePath),
      get_properties_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const base::Value::Dict& result_value = get_properties_result.Get();
  EXPECT_EQ(expected_value, result_value);
  // The GetProperties() error callback should not be invoked after properties
  // result has been received successfully.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillProfileClientTest, GetEntry) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(nullptr);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kTypeProperty);
  entry_writer.AppendVariantOfString(shill::kTypeWifi);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::Value::Dict expected_value;
  expected_value.Set(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  // Set expectations.
  PrepareForMethodCall(
      shill::kGetEntryFunction,
      base::BindRepeating(&ExpectStringArgument, kExampleEntryPath),
      response.get());
  // Call method.
  base::test::TestFuture<base::Value::Dict> get_entry_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->GetEntry(
      dbus::ObjectPath(kDefaultProfilePath), kExampleEntryPath,
      get_entry_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const base::Value::Dict& result_value = get_entry_result.Get();
  EXPECT_EQ(expected_value, result_value);
  // The GetEntry() error callback should not be invoked after the entry result
  // has been received successfully.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillProfileClientTest, DeleteEntry) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kDeleteEntryFunction,
      base::BindRepeating(&ExpectStringArgument, kExampleEntryPath),
      response.get());
  // Call method.
  base::test::TestFuture<void> delete_entry_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->DeleteEntry(
      dbus::ObjectPath(kDefaultProfilePath), kExampleEntryPath,
      delete_entry_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(delete_entry_result.Wait());
  // The DeleteEntry() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

}  // namespace ash
