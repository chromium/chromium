// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_service_client.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
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

const char kExampleServicePath[] = "/service/1";

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
  raw_ptr<ShillServiceClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
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
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(nullptr);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kSignalStrengthProperty);
  entry_writer.AppendVariantOfByte(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::Value::Dict expected_value;
  expected_value.Set(shill::kSignalStrengthProperty, kValue);
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<std::optional<base::Value::Dict>>
      get_properties_result;
  client_->GetProperties(dbus::ObjectPath(kExampleServicePath),
                         get_properties_result.GetCallback());
  std::optional<base::Value::Dict> result = get_properties_result.Take();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(expected_value, result.value());
}

TEST_F(ShillServiceClientTest, SetProperty) {
  const char kValue[] = "passphrase";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::Value value(kValue);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::BindRepeating(&ExpectStringAndValueArguments,
                                           shill::kPassphraseProperty, &value),
                       response.get());
  // Call method.
  base::test::TestFuture<void> set_property_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->SetProperty(
      dbus::ObjectPath(kExampleServicePath), shill::kPassphraseProperty, value,
      set_property_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(set_property_result.Wait());
  // The SetProperty() error callback should not be invoked after the successful
  // compilation.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, SetProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::Value::Dict arg = CreateExampleServiceProperties();
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  PrepareForMethodCall(
      shill::kSetPropertiesFunction,
      base::BindRepeating(&ExpectValueDictionaryArgument, &arg, string_valued),
      response.get());

  // Call method.
  base::test::TestFuture<void> set_property_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->SetProperties(
      dbus::ObjectPath(kExampleServicePath), arg,
      set_property_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(set_property_result.Wait());
  // The SetProperties() error callback should not be invoked after the
  // properties result is received successfully.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, ClearProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(
      shill::kClearPropertyFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kPassphraseProperty),
      response.get());
  // Call method.
  base::test::TestFuture<void> clear_property_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->ClearProperty(
      dbus::ObjectPath(kExampleServicePath), shill::kPassphraseProperty,
      clear_property_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(clear_property_result.Wait());
  // The ClearProperty() error callback should not be invoked after the
  // successful result is received.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, ClearProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("b", &array_writer);
  array_writer.AppendBool(true);
  array_writer.AppendBool(true);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  std::vector<std::string> keys;
  keys.push_back(shill::kPassphraseProperty);
  keys.push_back(shill::kSignalStrengthProperty);
  PrepareForMethodCall(shill::kClearPropertiesFunction,
                       base::BindRepeating(&ExpectArrayOfStringsArgument, keys),
                       response.get());
  // Call method.
  // We can't use `base::test::TestFuture` for non-copyable objects and
  // base::Value::List is non-copyable. So this test will keep using
  // base::RunLoop unlike other tests in this suite until TestFuture can support
  // it.
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
  base::test::TestFuture<void> connect_result;
  base::test::TestFuture<std::string, std::string> error_result;
  PrepareForMethodCall(shill::kConnectFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  client_->Connect(
      dbus::ObjectPath(kExampleServicePath), connect_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(connect_result.Wait());
  // The Remove() error callback should not be invoked after the successful
  // connection result.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, Disconnect) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kDisconnectFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<void> disconnect_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->Disconnect(
      dbus::ObjectPath(kExampleServicePath), disconnect_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(disconnect_result.Wait());
  // The Disconnect() error callback should not be invoked after the successful
  // disconnect call.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, Remove) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRemoveServiceFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<void> remove_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->Remove(
      dbus::ObjectPath(kExampleServicePath), remove_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(remove_result.Wait());
  // The Remove() error callback should not be invoked after the successful
  // compilation.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, GetWiFiPassphrase) {
  const char kPassphrase[] = "passphrase";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kPassphrase);

  // Set expectations.
  PrepareForMethodCall(shill::kGetWiFiPassphraseFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<std::string> get_passphrase_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->GetWiFiPassphrase(
      dbus::ObjectPath(kExampleServicePath),
      get_passphrase_result.GetCallback<const std::string&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_EQ(get_passphrase_result.Get(), kPassphrase);
  // The GetWifiPassphrase() error callback should not be invoked after the
  // passphrase is received successfully.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, GetEapPassphrase) {
  const char kPassphrase[] = "passphrase";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kPassphrase);

  // Set expectations.
  PrepareForMethodCall(shill::kGetEapPassphraseFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<std::string> get_passphrase_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->GetEapPassphrase(
      dbus::ObjectPath(kExampleServicePath),
      get_passphrase_result.GetCallback<const std::string&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_EQ(get_passphrase_result.Get(), kPassphrase);
  // The GetEapPassphrase() error callback should not be invoked after the
  // passphrase is received successfully.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, RequestPortalDetection) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  PrepareForMethodCall(shill::kRequestPortalDetectionFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<bool> request_detection_result;
  client_->RequestPortalDetection(dbus::ObjectPath(kExampleServicePath),
                                  request_detection_result.GetCallback());
  EXPECT_TRUE(request_detection_result.Get());
}

TEST_F(ShillServiceClientTest, RequestTrafficCounters) {
  // Set up value of response.
  base::Value::List traffic_counters;

  base::Value::Dict chrome_dict;
  chrome_dict.Set("source", shill::kTrafficCounterSourceChrome);
  chrome_dict.Set("rx_bytes", 12);
  chrome_dict.Set("tx_bytes", 34);
  traffic_counters.Append(std::move(chrome_dict));

  base::Value::Dict user_dict;
  user_dict.Set("source", shill::kTrafficCounterSourceUser);
  user_dict.Set("rx_bytes", 90);
  user_dict.Set("tx_bytes", 87);
  traffic_counters.Append(std::move(user_dict));

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, base::Value(traffic_counters.Clone()));

  // Set expectations.
  PrepareForMethodCall(shill::kRequestTrafficCountersFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  // Call method.
  base::test::TestFuture<std::optional<base::Value>> request_result;
  client_->RequestTrafficCounters(dbus::ObjectPath(kExampleServicePath),
                                  request_result.GetCallback());
  std::optional<base::Value> result = request_result.Take();
  EXPECT_TRUE(result);
  const base::Value::List& result_list = result.value().GetList();
  EXPECT_EQ(result_list, traffic_counters);
}

TEST_F(ShillServiceClientTest, ResetTrafficCounters) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kResetTrafficCountersFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<void> reset_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->ResetTrafficCounters(
      dbus::ObjectPath(kExampleServicePath), reset_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(reset_result.Wait());
  // The ResetTrafficCounters() error callback should not be invoked after
  // successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillServiceClientTest, InvalidServicePath) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  base::test::TestFuture<void> connect_result;
  base::test::TestFuture<std::string, std::string> error_result;
  PrepareForMethodCall(shill::kConnectFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  // Call method.
  client_->Connect(
      dbus::ObjectPath("/invalid/path"), connect_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  // Error will be received after calling the method with invalid path.
  EXPECT_TRUE(error_result.Wait());
  // Connect result callback should not be invoked after an error is received.
  EXPECT_FALSE(connect_result.IsReady());
}

}  // namespace ash
