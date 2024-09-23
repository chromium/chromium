// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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

void ExpectThrottlingArguments(bool throttling_enabled_expected,
                               uint32_t upload_rate_kbits_expected,
                               uint32_t download_rate_kbits_expected,
                               dbus::MessageReader* reader) {
  bool throttling_enabled_actual;
  uint32_t upload_rate_kbits_actual;
  uint32_t download_rate_kbits_actual;
  ASSERT_TRUE(reader->PopBool(&throttling_enabled_actual));
  EXPECT_EQ(throttling_enabled_actual, throttling_enabled_expected);
  ASSERT_TRUE(reader->PopUint32(&upload_rate_kbits_actual));
  EXPECT_EQ(upload_rate_kbits_expected, upload_rate_kbits_actual);
  ASSERT_TRUE(reader->PopUint32(&download_rate_kbits_actual));
  EXPECT_EQ(download_rate_kbits_expected, download_rate_kbits_actual);
  EXPECT_FALSE(reader->HasMoreData());
}

}  // namespace

class ShillManagerClientTest : public ShillClientUnittestBase {
 public:
  ShillManagerClientTest()
      : ShillClientUnittestBase(shill::kFlimflamManagerInterface,
                                dbus::ObjectPath(shill::kFlimflamServicePath)) {
  }

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    ShillManagerClient::Initialize(mock_bus_.get());
    client_ = ShillManagerClient::Get();
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ShillManagerClient::Shutdown();
    ShillClientUnittestBase::TearDown();
  }

 protected:
  raw_ptr<ShillManagerClient, DanglingUntriaged> client_ =
      nullptr;  // Unowned convenience pointer.
};

TEST_F(ShillManagerClientTest, PropertyChanged) {
  // Create a signal.
  base::Value kArpGateway(true);
  dbus::Signal signal(shill::kFlimflamManagerInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kArpGatewayProperty);
  dbus::AppendBasicTypeValueData(&writer, kArpGateway);

  // Set expectations.
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer, OnPropertyChanged(shill::kArpGatewayProperty,
                                          ValueEq(ByRef(kArpGateway))))
      .Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(&observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(&observer);

  // Make sure it's not called anymore.
  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillManagerClientTest, GetProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(nullptr);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(nullptr);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kArpGatewayProperty);
  entry_writer.AppendVariantOfBool(true);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::Value::Dict expected_value;
  expected_value.Set(shill::kArpGatewayProperty, true);
  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  // Prepare result callback to get the properties.
  base::test::TestFuture<std::optional<base::Value::Dict>>
      get_properties_result;
  // Call method.
  client_->GetProperties(get_properties_result.GetCallback());
  std::optional<base::Value::Dict> result = get_properties_result.Take();
  EXPECT_TRUE(result.has_value());
  const base::Value::Dict& result_value = result.value();
  EXPECT_EQ(expected_value, result_value);
}

TEST_F(ShillManagerClientTest, GetNetworksForGeolocation) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter type_dict_writer(nullptr);
  writer.OpenArray("{sv}", &type_dict_writer);
  dbus::MessageWriter type_entry_writer(nullptr);
  type_dict_writer.OpenDictEntry(&type_entry_writer);
  type_entry_writer.AppendString(shill::kTypeWifi);
  dbus::MessageWriter variant_writer(nullptr);
  type_entry_writer.OpenVariant("aa{ss}", &variant_writer);
  dbus::MessageWriter wap_list_writer(nullptr);
  variant_writer.OpenArray("a{ss}", &wap_list_writer);
  dbus::MessageWriter property_dict_writer(nullptr);
  wap_list_writer.OpenArray("{ss}", &property_dict_writer);
  dbus::MessageWriter property_entry_writer(nullptr);
  property_dict_writer.OpenDictEntry(&property_entry_writer);
  property_entry_writer.AppendString(shill::kGeoMacAddressProperty);
  property_entry_writer.AppendString("01:23:45:67:89:AB");
  property_dict_writer.CloseContainer(&property_entry_writer);
  wap_list_writer.CloseContainer(&property_dict_writer);
  variant_writer.CloseContainer(&wap_list_writer);
  type_entry_writer.CloseContainer(&variant_writer);
  type_dict_writer.CloseContainer(&type_entry_writer);
  writer.CloseContainer(&type_dict_writer);

  // Create the expected value.
  base::Value::Dict property_dict;
  property_dict.Set(shill::kGeoMacAddressProperty, "01:23:45:67:89:AB");
  base::Value::List type_entry_list;
  type_entry_list.Append(std::move(property_dict));
  base::Value::Dict type_dict;
  type_dict.Set("wifi", std::move(type_entry_list));

  // Set expectations.
  PrepareForMethodCall(shill::kGetNetworksForGeolocation,
                       base::BindRepeating(&ExpectNoArgument), response.get());

  // Prepare result callback to get the networks dictionary.
  base::test::TestFuture<std::optional<base::Value::Dict>> get_networks_result;
  // Call method.
  client_->GetNetworksForGeolocation(get_networks_result.GetCallback());
  // Check if result is as expected.
  std::optional<base::Value::Dict> result = get_networks_result.Take();
  EXPECT_TRUE(result.has_value());
  const base::Value::Dict& result_value = result.value();
  EXPECT_EQ(type_dict, result_value);
}

TEST_F(ShillManagerClientTest, SetProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  base::Value value("portal list");
  PrepareForMethodCall(
      shill::kSetPropertyFunction,
      base::BindRepeating(ExpectStringAndValueArguments,
                          shill::kCheckPortalListProperty, &value),
      response.get());
  // Prepare callbacks for properties result and error.
  base::test::TestFuture<void> set_property_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call method.
  client_->SetProperty(
      shill::kCheckPortalListProperty, value, set_property_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(set_property_result.Wait());
  // The SetProperty() error callback should not be invoked after successful
  // completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, RequestScan) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(
      shill::kRequestScanFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kTypeWifi),
      response.get());
  // Prepare callbacks for scan result and error.
  base::test::TestFuture<void> scan_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call method.
  client_->RequestScan(
      shill::kTypeWifi, scan_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(scan_result.Wait());
  // The RequestScan() error callback should not be invoked after successful
  // completion and scan result has been received.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, EnableTechnology) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(
      shill::kEnableTechnologyFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kTypeWifi),
      response.get());
  // Prepare callbacks for successful enable technology call or error.
  base::test::TestFuture<void> enable_technology_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call method.
  client_->EnableTechnology(
      shill::kTypeWifi, enable_technology_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(enable_technology_result.Wait());
  // The EnableTechnology() error callback should not be invoked after result
  // has arrived on successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, NetworkThrottling) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  const bool enabled = true;
  const uint32_t upload_rate = 1200;
  const uint32_t download_rate = 2000;
  PrepareForMethodCall(shill::kSetNetworkThrottlingFunction,
                       base::BindRepeating(&ExpectThrottlingArguments, enabled,
                                           upload_rate, download_rate),
                       response.get());
  // Prepare callbacks for successful set network throttling status call and
  // error.
  base::test::TestFuture<void> set_network_throttling_status_result;
  base::test::TestFuture<std::string, std::string> error_result;
  // Call method.
  client_->SetNetworkThrottlingStatus(
      ShillManagerClient::NetworkThrottlingStatus{enabled, upload_rate,
                                                  download_rate},
      set_network_throttling_status_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(set_network_throttling_status_result.Wait());
  // The SetNetworkThrottlingStatus() error callback should not be invoked after
  // successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, DisableTechnology) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(
      shill::kDisableTechnologyFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kTypeWifi),
      response.get());
  // Call method.
  base::test::TestFuture<void> disable_technology_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->DisableTechnology(
      shill::kTypeWifi, disable_technology_result.GetCallback(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_TRUE(disable_technology_result.Wait());
  // The DisableTechnology() error callback should not be invoked after the
  // disable technology result has been received successfully.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, ConfigureService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  base::Value::Dict arg = CreateExampleServiceProperties();
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  // Set expectations.
  PrepareForMethodCall(
      shill::kConfigureServiceFunction,
      base::BindRepeating(&ExpectValueDictionaryArgument, &arg, string_valued),
      response.get());
  // Call method.
  base::test::TestFuture<dbus::ObjectPath> configure_service_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->ConfigureService(
      arg, configure_service_result.GetCallback<const dbus::ObjectPath&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const dbus::ObjectPath& result = configure_service_result.Get();
  EXPECT_EQ(result, object_path);
  // The ConfigureService() error callback should not be invoked after
  // successful completion and object path result has been received.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, GetService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  base::Value::Dict arg = CreateExampleServiceProperties();
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  // Set expectations.
  PrepareForMethodCall(
      shill::kGetServiceFunction,
      base::BindRepeating(&ExpectValueDictionaryArgument, &arg, string_valued),
      response.get());
  // Call method.
  base::test::TestFuture<dbus::ObjectPath> get_service_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->GetService(
      arg, get_service_result.GetCallback<const dbus::ObjectPath&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const dbus::ObjectPath& result = get_service_result.Get();
  EXPECT_EQ(result, object_path);
  // The GetService() error callback should not be invoked after successful
  // completion and object path result has been received.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, SetTetheringEnabled) {
  const char kEnabledResult[] = "success";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kEnabledResult);

  // Set expectation.
  PrepareForMethodCall(shill::kSetTetheringEnabledFunction,
                       base::BindRepeating(&ExpectBoolArgument, true),
                       response.get());
  // Call method.
  base::test::TestFuture<std::string> set_tethering_enabled_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->SetTetheringEnabled(
      /*enabled=*/true,
      set_tethering_enabled_result.GetCallback<const std::string&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const std::string& enabled_result = set_tethering_enabled_result.Get();
  EXPECT_EQ(kEnabledResult, enabled_result);
  // The SetTetheringEnabled() error callback should not be invoked after
  // successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, EnableTethering) {
  const char kEnabledResult[] = "success";
  const shill::WiFiInterfacePriority kPriority =
      shill::WiFiInterfacePriority::OS_REQUEST;

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kEnabledResult);

  // Set expectation.
  PrepareForMethodCall(shill::kEnableTetheringFunction,
                       base::BindRepeating(&ExpectUint32Argument,
                                           static_cast<uint32_t>(kPriority)),
                       response.get());
  // Call method.
  base::test::TestFuture<std::string> enable_tethering_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->EnableTethering(
      kPriority, enable_tethering_result.GetCallback<const std::string&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const std::string& enabled_result = enable_tethering_result.Get();
  EXPECT_EQ(kEnabledResult, enabled_result);
  // The EnableTethering() error callback should not be invoked after
  // successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, DisableTethering) {
  const char kDisabledResult[] = "success";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kDisabledResult);

  // Set expectation.
  PrepareForMethodCall(shill::kDisableTetheringFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<std::string> disable_tethering_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->DisableTethering(
      disable_tethering_result.GetCallback<const std::string&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const std::string& disabled_result = disable_tethering_result.Get();
  EXPECT_EQ(kDisabledResult, disabled_result);
  // The DisableTethering() error callback should not be invoked after
  // successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, CheckTetheringReadiness) {
  const char kReadinessResult[] = "not_ready";

  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendString(kReadinessResult);

  // Set expectation.
  PrepareForMethodCall(shill::kCheckTetheringReadinessFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  base::test::TestFuture<std::string> check_tethering_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->CheckTetheringReadiness(
      check_tethering_result.GetCallback<const std::string&>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  const std::string& readiness_status = check_tethering_result.Get();
  EXPECT_EQ(kReadinessResult, readiness_status);
  // The CheckTetheringReadiness() error callback should not be invoked after
  // successful completion.
  EXPECT_FALSE(error_result.IsReady());
}

TEST_F(ShillManagerClientTest, CreateP2PGroup) {
  const char kShillId[] = "sample_shill_id";
  const char kCreateGroupResult[] = "success";

  const char kSSID[] = "test_ssid";
  const char kPassphrase[] = "test_password";
  const int kFrequency = 3;
  const shill::WiFiInterfacePriority kPriority =
      shill::WiFiInterfacePriority::FOREGROUND_WITHOUT_FALLBACK;

  // Create response.
  base::Value::Dict result_dictionary;
  result_dictionary.Set("shill_id", kShillId);
  result_dictionary.Set("result", kCreateGroupResult);

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, result_dictionary);

  // Create input dictionary
  base::Value::Dict input_dictionary;
  input_dictionary.Set(shill::kP2PDeviceSSID, kSSID);
  input_dictionary.Set(shill::kP2PDevicePassphrase, kPassphrase);
  input_dictionary.Set(shill::kP2PDeviceFrequency, kFrequency);
  input_dictionary.Set(shill::kP2PDevicePriority, static_cast<int>(kPriority));

  // Set expectation.
  const bool string_valued = false;
  PrepareForMethodCall(shill::kCreateP2PGroupFunction,
                       base::BindRepeating(&ExpectValueDictionaryArgument,
                                           &input_dictionary, string_valued),
                       response.get());

  base::test::TestFuture<base::Value::Dict> create_p2p_group_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->CreateP2PGroup(
      ShillManagerClient::CreateP2PGroupParameter(kSSID, kPassphrase,
                                                  kFrequency, kPriority),
      create_p2p_group_result.GetCallback<base::Value::Dict>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_EQ(create_p2p_group_result.Get(), result_dictionary);
}

TEST_F(ShillManagerClientTest, ConnectToP2PGroup) {
  const char kShillId[] = "sample_shill_id";
  const char kConnectToGroupResult[] = "success";

  const char kSSID[] = "test_ssid";
  const char kPassphrase[] = "test_passphrase";
  const int kFrequency = 3;

  // Create response.
  base::Value::Dict result_dictionary;
  result_dictionary.Set("shill_id", kShillId);
  result_dictionary.Set("result", kConnectToGroupResult);

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, result_dictionary);

  // Create input dictionary
  base::Value::Dict input_dictionary;
  input_dictionary.Set(shill::kP2PDeviceSSID, kSSID);
  input_dictionary.Set(shill::kP2PDevicePassphrase, kPassphrase);
  input_dictionary.Set(shill::kP2PDeviceFrequency, kFrequency);
  input_dictionary.Set(shill::kP2PDevicePriority, 2);

  // Set expectation.
  const bool string_valued = false;
  PrepareForMethodCall(shill::kConnectToP2PGroupFunction,
                       base::BindRepeating(&ExpectValueDictionaryArgument,
                                           &input_dictionary, string_valued),
                       response.get());

  base::test::TestFuture<base::Value::Dict> connect_to_p2p_group_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->ConnectToP2PGroup(
      ShillManagerClient::ConnectP2PGroupParameter(kSSID, kPassphrase,
                                                   kFrequency, std::nullopt),
      connect_to_p2p_group_result.GetCallback<base::Value::Dict>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_EQ(connect_to_p2p_group_result.Get(), result_dictionary);
}

TEST_F(ShillManagerClientTest, DestroyP2PGroup) {
  const int kShillId = 57;
  const char kDestroyGroupResult[] = "success";

  // Create response.
  base::Value::Dict result_dictionary;
  result_dictionary.Set("shill_id", kShillId);
  result_dictionary.Set("result", kDestroyGroupResult);

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, result_dictionary);

  // Set expectation.
  PrepareForMethodCall(shill::kDestroyP2PGroupFunction,
                       base::BindRepeating(&ExpectIntArgument, kShillId),
                       response.get());

  base::test::TestFuture<base::Value::Dict> destroy_p2p_group_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->DestroyP2PGroup(
      kShillId, destroy_p2p_group_result.GetCallback<base::Value::Dict>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_EQ(destroy_p2p_group_result.Get(), result_dictionary);
}

TEST_F(ShillManagerClientTest, DisconnectFromP2PGroup) {
  const int kShillId = 57;
  const char kDisconnectFromGroupResult[] = "success";

  // Create response.
  base::Value::Dict result_dictionary;
  result_dictionary.Set("shill_id", kShillId);
  result_dictionary.Set("result", kDisconnectFromGroupResult);

  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  AppendValueDataAsVariant(&writer, result_dictionary);

  // Set expectation.
  PrepareForMethodCall(shill::kDisconnectFromP2PGroupFunction,
                       base::BindRepeating(&ExpectIntArgument, kShillId),
                       response.get());

  base::test::TestFuture<base::Value::Dict> disconnect_from_p2p_group_result;
  base::test::TestFuture<std::string, std::string> error_result;
  client_->DisconnectFromP2PGroup(
      kShillId,
      disconnect_from_p2p_group_result.GetCallback<base::Value::Dict>(),
      error_result.GetCallback<const std::string&, const std::string&>());
  EXPECT_EQ(disconnect_from_p2p_group_result.Get(), result_dictionary);
}

}  // namespace ash
