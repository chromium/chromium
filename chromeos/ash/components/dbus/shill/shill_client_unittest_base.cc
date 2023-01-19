// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_client_unittest_base.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {

namespace {

// Pops a string-to-string dictionary from the reader.
std::unique_ptr<base::Value> PopStringToStringDictionary(
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader))
    return nullptr;
  auto result = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  while (array_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(nullptr);
    std::string key;
    std::string value;
    if (!array_reader.PopDictEntry(&entry_reader) ||
        !entry_reader.PopString(&key) || !entry_reader.PopString(&value))
      return nullptr;
    result->SetKey(key, base::Value(value));
  }
  return result;
}

}  // namespace

ValueMatcher::ValueMatcher(const base::Value& value)
    : expected_value_(base::Value::ToUniquePtrValue(value.Clone())) {}
ValueMatcher::~ValueMatcher() = default;

bool ValueMatcher::MatchAndExplain(const base::Value& value,
                                   MatchResultListener* listener) const {
  return *expected_value_ == value;
}

void ValueMatcher::DescribeTo(::std::ostream* os) const {
  std::string expected_value_str;
  base::JSONWriter::WriteWithOptions(*expected_value_,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &expected_value_str);
  *os << "value equals " << expected_value_str;
}

void ValueMatcher::DescribeNegationTo(::std::ostream* os) const {
  std::string expected_value_str;
  base::JSONWriter::WriteWithOptions(*expected_value_,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &expected_value_str);
  *os << "value does not equal " << expected_value_str;
}

ShillClientUnittestBase::MockPropertyChangeObserver::
    MockPropertyChangeObserver() = default;

ShillClientUnittestBase::MockPropertyChangeObserver::
    ~MockPropertyChangeObserver() = default;

ShillClientUnittestBase::ShillClientUnittestBase(
    const std::string& interface_name,
    const dbus::ObjectPath& object_path)
    : interface_name_(interface_name),
      object_path_(object_path),
      response_(nullptr) {}

ShillClientUnittestBase::~ShillClientUnittestBase() = default;

void ShillClientUnittestBase::SetUp() {
  // Create a mock bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  mock_bus_ = new dbus::MockBus(options);

  // Create a mock proxy.
  mock_proxy_ = new dbus::MockObjectProxy(
      mock_bus_.get(), shill::kFlimflamServiceName, object_path_);

  // Set expectations so that mock_proxy's Call methods will return responses.
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillRepeatedly(Invoke(this, &ShillClientUnittestBase::OnCallMethod));
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorResponse(_, _, _))
      .WillRepeatedly(Invoke(
          this, &ShillClientUnittestBase::OnCallMethodWithErrorResponse));
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethodWithErrorCallback(_, _, _, _))
      .WillRepeatedly(Invoke(
          this, &ShillClientUnittestBase::OnCallMethodWithErrorCallback));

  // Set an expectation so mock_proxy's ConnectToSignal() will use
  // OnConnectToPropertyChanged() to run the callback.
  EXPECT_CALL(
      *mock_proxy_.get(),
      DoConnectToSignal(interface_name_, shill::kMonitorPropertyChanged, _, _))
      .WillRepeatedly(
          Invoke(this, &ShillClientUnittestBase::OnConnectToPropertyChanged));

  EXPECT_CALL(*mock_proxy_.get(),
              DoConnectToSignal(interface_name_,
                                shill::kOnPlatformMessageFunction, _, _))
      .WillRepeatedly(
          Invoke(this, &ShillClientUnittestBase::OnConnectToPlatformMessage));

  EXPECT_CALL(*mock_proxy_.get(),
              DoConnectToSignal(interface_name_,
                                shill::kOnPacketReceivedFunction, _, _))
      .WillRepeatedly(
          Invoke(this, &ShillClientUnittestBase::OnConnectToPacketReceived));

  EXPECT_CALL(*mock_bus_.get(),
              GetObjectProxy(shill::kFlimflamServiceName, object_path_))
      .WillRepeatedly(Return(mock_proxy_.get()));

  // Set an expectation so mock_bus's GetDBusTaskRunner will return the current
  // task runner.
  EXPECT_CALL(*mock_bus_.get(), GetDBusTaskRunner())
      .WillRepeatedly(
          Return(task_environment_.GetMainThreadTaskRunner().get()));

  // ShutdownAndBlock() will be called in TearDown().
  EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());
}

void ShillClientUnittestBase::TearDown() {
  mock_bus_->ShutdownAndBlock();
}

void ShillClientUnittestBase::PrepareForMethodCall(
    const std::string& method_name,
    const ArgumentCheckCallback& argument_checker,
    dbus::Response* response) {
  expected_method_name_ = method_name;
  argument_checker_ = argument_checker;
  response_ = response;
}

void ShillClientUnittestBase::SendPlatformMessageSignal(dbus::Signal* signal) {
  ASSERT_FALSE(platform_message_handler_.is_null());
  platform_message_handler_.Run(signal);
}

void ShillClientUnittestBase::SendPacketReceievedSignal(dbus::Signal* signal) {
  ASSERT_FALSE(packet_receieved__handler_.is_null());
  packet_receieved__handler_.Run(signal);
}

void ShillClientUnittestBase::SendPropertyChangedSignal(dbus::Signal* signal) {
  ASSERT_FALSE(property_changed_handler_.is_null());
  property_changed_handler_.Run(signal);
}

// static
void ShillClientUnittestBase::ExpectPropertyChanged(
    const std::string& expected_name,
    const base::Value* expected_value,
    const std::string& name,
    const base::Value& value) {
  EXPECT_EQ(expected_name, name);
  EXPECT_EQ(*expected_value, value);
}

// static
void ShillClientUnittestBase::ExpectNoArgument(dbus::MessageReader* reader) {
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectUint32Argument(
    uint32_t expected_value,
    dbus::MessageReader* reader) {
  uint32_t value;
  ASSERT_TRUE(reader->PopUint32(&value));
  EXPECT_EQ(expected_value, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectArrayOfBytesArgument(
    const std::string& expected_bytes,
    dbus::MessageReader* reader) {
  const uint8_t* bytes = nullptr;
  size_t size = 0;
  ASSERT_TRUE(reader->PopArrayOfBytes(&bytes, &size));
  EXPECT_EQ(expected_bytes.size(), size);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(expected_bytes[i], bytes[i]);
  }
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectStringArgument(
    const std::string& expected_string,
    dbus::MessageReader* reader) {
  std::string str;
  ASSERT_TRUE(reader->PopString(&str));
  EXPECT_EQ(expected_string, str);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectBoolArgument(bool expected_value,
                                                 dbus::MessageReader* reader) {
  bool value;
  ASSERT_TRUE(reader->PopBool(&value));
  EXPECT_EQ(expected_value, value);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectArrayOfStringsArgument(
    const std::vector<std::string>& expected_strings,
    dbus::MessageReader* reader) {
  std::vector<std::string> strs;
  ASSERT_TRUE(reader->PopArrayOfStrings(&strs));
  EXPECT_EQ(expected_strings, strs);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectStringAndValueArguments(
    const std::string& expected_string,
    const base::Value* expected_value,
    dbus::MessageReader* reader) {
  std::string str;
  ASSERT_TRUE(reader->PopString(&str));
  EXPECT_EQ(expected_string, str);
  base::Value value(dbus::PopDataAsValue(reader));
  ASSERT_TRUE(!value.is_none());
  EXPECT_EQ(value, *expected_value);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void ShillClientUnittestBase::ExpectValueDictionaryArgument(
    const base::Value* expected_dictionary,
    bool string_valued,
    dbus::MessageReader* reader) {
  ASSERT_TRUE(expected_dictionary->is_dict());
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader->PopArray(&array_reader));
  while (array_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(nullptr);
    ASSERT_TRUE(array_reader.PopDictEntry(&entry_reader));
    std::string key;
    ASSERT_TRUE(entry_reader.PopString(&key));
    if (string_valued) {
      std::string value;
      ASSERT_TRUE(entry_reader.PopString(&value));
      const std::string* expected_value =
          expected_dictionary->FindStringKey(key);
      ASSERT_TRUE(expected_value);
      EXPECT_EQ(*expected_value, value);
      continue;
    }
    dbus::MessageReader variant_reader(nullptr);
    ASSERT_TRUE(entry_reader.PopVariant(&variant_reader));
    std::unique_ptr<base::Value> value;
    // Variants in the dictionary can be basic types or string-to-string
    // dictinoary.
    switch (variant_reader.GetDataType()) {
      case dbus::Message::ARRAY:
        value = PopStringToStringDictionary(&variant_reader);
        break;
      case dbus::Message::BOOL:
      case dbus::Message::INT32:
      case dbus::Message::STRING:
        value = base::Value::ToUniquePtrValue(
            dbus::PopDataAsValue(&variant_reader));
        ASSERT_FALSE(value->is_none());
        break;
      default:
        NOTREACHED();
    }
    ASSERT_TRUE(value.get());
    const base::Value* expected_value = expected_dictionary->FindKey(key);
    ASSERT_TRUE(expected_value);
    EXPECT_EQ(*value, *expected_value);
  }
}

// static
base::Value ShillClientUnittestBase::CreateExampleServiceProperties() {
  base::Value properties(base::Value::Type::DICTIONARY);
  properties.SetKey(shill::kGuidProperty,
                    base::Value("00000000-0000-0000-0000-000000000000"));
  properties.SetKey(shill::kModeProperty, base::Value(shill::kModeManaged));
  properties.SetKey(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  const std::string ssid = "testssid";
  properties.SetKey(shill::kWifiHexSsid,
                    base::Value(base::HexEncode(ssid.c_str(), ssid.size())));
  properties.SetKey(shill::kSecurityClassProperty,
                    base::Value(shill::kSecurityClassPsk));
  return properties;
}

// static
void ShillClientUnittestBase::ExpectNoResultValue(bool result) {
  EXPECT_TRUE(result);
}

// static
void ShillClientUnittestBase::ExpectObjectPathResultWithoutStatus(
    const dbus::ObjectPath& expected_result,
    const dbus::ObjectPath& result) {
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectBoolResultWithoutStatus(
    bool expected_result,
    bool result) {
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectStringResultWithoutStatus(
    const std::string& expected_result,
    const std::string& result) {
  EXPECT_EQ(expected_result, result);
}

// static
void ShillClientUnittestBase::ExpectValueResultWithoutStatus(
    const base::Value* expected_result,
    base::Value result) {
  std::string expected_result_string;
  base::JSONWriter::Write(*expected_result, &expected_result_string);
  std::string result_string;
  base::JSONWriter::Write(result, &result_string);
  EXPECT_EQ(expected_result_string, result_string);
}

// static
void ShillClientUnittestBase::ExpectValueResult(
    const base::Value* expected_result,
    absl::optional<base::Value> result) {
  EXPECT_TRUE(result);
  ExpectValueResultWithoutStatus(expected_result,
                                 std::move(result).value_or(base::Value()));
}

void ShillClientUnittestBase::OnConnectToPlatformMessage(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
  platform_message_handler_ = signal_callback;
  const bool success = true;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                interface_name, signal_name, success));
}

void ShillClientUnittestBase::OnConnectToPacketReceived(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
  packet_receieved__handler_ = signal_callback;
  const bool success = true;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                interface_name, signal_name, success));
}

void ShillClientUnittestBase::OnConnectToPropertyChanged(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
  property_changed_handler_ = signal_callback;
  const bool success = true;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                interface_name, signal_name, success));
}

void ShillClientUnittestBase::OnCallMethod(
    dbus::MethodCall* method_call,
    int timeout_ms,
    dbus::ObjectProxy::ResponseCallback* response_callback) {
  EXPECT_EQ(interface_name_, method_call->GetInterface());
  EXPECT_EQ(expected_method_name_, method_call->GetMember());
  dbus::MessageReader reader(method_call);
  argument_checker_.Run(&reader);
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*response_callback), response_));
}

void ShillClientUnittestBase::OnCallMethodWithErrorResponse(
    dbus::MethodCall* method_call,
    int timeout_ms,
    dbus::ObjectProxy::ResponseOrErrorCallback* response_callback) {
  EXPECT_EQ(interface_name_, method_call->GetInterface());
  EXPECT_EQ(expected_method_name_, method_call->GetMember());
  dbus::MessageReader reader(method_call);
  argument_checker_.Run(&reader);
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*response_callback), response_, nullptr));
}

void ShillClientUnittestBase::OnCallMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    int timeout_ms,
    dbus::ObjectProxy::ResponseCallback* response_callback,
    dbus::ObjectProxy::ErrorCallback* error_callback) {
  OnCallMethod(method_call, timeout_ms, response_callback);
}

}  // namespace ash
