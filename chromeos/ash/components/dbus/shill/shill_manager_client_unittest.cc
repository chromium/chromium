// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
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
  ShillManagerClient* client_ = nullptr;  // Unowned convenience pointer.
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
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kArpGatewayProperty);
  entry_writer.AppendVariantOfBool(true);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(shill::kArpGatewayProperty, base::Value(true));
  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  client_->GetProperties(base::BindOnce(&ExpectValueResult, &value));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, GetNetworksForGeolocation) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter type_dict_writer(NULL);
  writer.OpenArray("{sv}", &type_dict_writer);
  dbus::MessageWriter type_entry_writer(NULL);
  type_dict_writer.OpenDictEntry(&type_entry_writer);
  type_entry_writer.AppendString(shill::kTypeWifi);
  dbus::MessageWriter variant_writer(NULL);
  type_entry_writer.OpenVariant("aa{ss}", &variant_writer);
  dbus::MessageWriter wap_list_writer(NULL);
  variant_writer.OpenArray("a{ss}", &wap_list_writer);
  dbus::MessageWriter property_dict_writer(NULL);
  wap_list_writer.OpenArray("{ss}", &property_dict_writer);
  dbus::MessageWriter property_entry_writer(NULL);
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
  base::Value property_dict_value(base::Value::Type::DICTIONARY);
  property_dict_value.SetKey(shill::kGeoMacAddressProperty,
                             base::Value("01:23:45:67:89:AB"));
  base::Value type_entry_value(base::Value::Type::LIST);
  type_entry_value.Append(std::move(property_dict_value));
  base::Value type_dict_value(base::Value::Type::DICTIONARY);
  type_dict_value.SetKey("wifi", std::move(type_entry_value));

  // Set expectations.
  PrepareForMethodCall(shill::kGetNetworksForGeolocation,
                       base::BindRepeating(&ExpectNoArgument), response.get());
  // Call method.
  client_->GetNetworksForGeolocation(
      base::BindOnce(&ExpectValueResult, &type_dict_value));

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
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
  // Call method.
  base::RunLoop run_loop;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->SetProperty(shill::kCheckPortalListProperty, value,
                       run_loop.QuitClosure(), mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  run_loop.RunUntilIdle();
}

TEST_F(ShillManagerClientTest, RequestScan) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(
      shill::kRequestScanFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kTypeWifi),
      response.get());
  // Call method.
  base::RunLoop run_loop;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->RequestScan(shill::kTypeWifi, run_loop.QuitClosure(),
                       mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  run_loop.RunUntilIdle();
}

TEST_F(ShillManagerClientTest, EnableTechnology) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(
      shill::kEnableTechnologyFunction,
      base::BindRepeating(&ExpectStringArgument, shill::kTypeWifi),
      response.get());
  // Call method.
  base::RunLoop run_loop;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->EnableTechnology(shill::kTypeWifi, run_loop.QuitClosure(),
                            mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  run_loop.RunUntilIdle();
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
  // Call method.
  base::RunLoop run_loop;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  client_->SetNetworkThrottlingStatus(
      ShillManagerClient::NetworkThrottlingStatus{enabled, upload_rate,
                                                  download_rate},
      run_loop.QuitClosure(), mock_error_callback.Get());
  run_loop.RunUntilIdle();
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
  base::MockCallback<base::OnceClosure> mock_closure;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->DisableTechnology(shill::kTypeWifi, mock_closure.Get(),
                             mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, ConfigureService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  base::Value arg = CreateExampleServiceProperties();
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  // Set expectations.
  PrepareForMethodCall(
      shill::kConfigureServiceFunction,
      base::BindRepeating(&ExpectValueDictionaryArgument, &arg, string_valued),
      response.get());
  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->ConfigureService(
      arg, base::BindOnce(&ExpectObjectPathResultWithoutStatus, object_path),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, GetService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  base::Value arg = CreateExampleServiceProperties();
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  // Set expectations.
  PrepareForMethodCall(
      shill::kGetServiceFunction,
      base::BindRepeating(&ExpectValueDictionaryArgument, &arg, string_valued),
      response.get());
  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->GetService(
      arg, base::BindOnce(&ExpectObjectPathResultWithoutStatus, object_path),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, SetTetheringEnabled) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  PrepareForMethodCall(shill::kSetTetheringEnabledFunction,
                       base::BindRepeating(&ExpectBoolArgument, true),
                       response.get());
  // Call method.
  base::RunLoop run_loop;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);
  client_->SetTetheringEnabled(
      /*enabled=*/true, run_loop.QuitClosure(), mock_error_callback.Get());

  // Run the message loop.
  run_loop.RunUntilIdle();
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
  base::RunLoop run_loop;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);
  client_->CheckTetheringReadiness(
      base::BindLambdaForTesting([&](const std::string& readiness_status) {
        EXPECT_EQ(kReadinessResult, readiness_status);
        run_loop.QuitClosure();
      }),
      mock_error_callback.Get());

  // Run the message loop.
  run_loop.RunUntilIdle();
}

}  // namespace ash
