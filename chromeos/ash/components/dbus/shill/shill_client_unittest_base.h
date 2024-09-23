// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_CLIENT_UNITTEST_BASE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_CLIENT_UNITTEST_BASE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_client_helper.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;

namespace base {
class Value;
}

namespace dbus {

class MessageReader;

}  // namespace dbus

namespace ash {

// A gmock matcher for base::Value types, so we can match them in expectations.
class ValueMatcher : public MatcherInterface<const base::Value&> {
 public:
  explicit ValueMatcher(const base::Value& value);
  ~ValueMatcher();

  // MatcherInterface overrides.
  bool MatchAndExplain(const base::Value& value,
                       MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  std::unique_ptr<base::Value> expected_value_;
};

inline Matcher<const base::Value&> ValueEq(const base::Value& expected_value) {
  return MakeMatcher(new ValueMatcher(expected_value));
}

// A class to provide functionalities needed for testing Shill D-Bus clients.
class ShillClientUnittestBase : public testing::Test {
 public:
  // A mock PropertyChangedObserver that can be used to check expected values.
  class MockPropertyChangeObserver : public ShillPropertyChangedObserver {
   public:
    MockPropertyChangeObserver();
    ~MockPropertyChangeObserver() override;
    MOCK_METHOD2(OnPropertyChanged,
                 void(const std::string& name, const base::Value& value));
  };

  explicit ShillClientUnittestBase(const std::string& interface_name,
                                   const dbus::ObjectPath& object_path);
  ~ShillClientUnittestBase() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // A callback to intercept and check the method call arguments.
  using ArgumentCheckCallback =
      base::RepeatingCallback<void(dbus::MessageReader* reader)>;

  // Sets expectations for called method name and arguments, and sets response.
  void PrepareForMethodCall(const std::string& method_name,
                            const ArgumentCheckCallback& argument_checker,
                            dbus::Response* response);

  // Sends platform message signal to the tested client.
  void SendPlatformMessageSignal(dbus::Signal* signal);

  // Sends packet received signal to the tested client.
  void SendPacketReceivedSignal(dbus::Signal* signal);

  // Sends property changed signal to the tested client.
  void SendPropertyChangedSignal(dbus::Signal* signal);

  // Checks the name and the value which are sent by PropertyChanged signal.
  static void ExpectPropertyChanged(const std::string& expected_name,
                                    const base::Value* expected_value,
                                    const std::string& name,
                                    const base::Value& value);

  // Expects the reader to be empty.
  static void ExpectNoArgument(dbus::MessageReader* reader);

  // Expects the reader to have a uint32_t
  static void ExpectUint32Argument(uint32_t expected_value,
                                   dbus::MessageReader* reader);

  // Expects the reader to have a int
  static void ExpectIntArgument(int expected_value,
                                   dbus::MessageReader* reader);

  // Expects the reader to have an array of bytes
  static void ExpectArrayOfBytesArgument(const std::string& expected_bytes,
                                         dbus::MessageReader* reader);

  // Expects the reader to have a string.
  static void ExpectStringArgument(const std::string& expected_string,
                                   dbus::MessageReader* reader);

  // Expects the reader to have a bool
  static void ExpectBoolArgument(bool expected_value,
                                 dbus::MessageReader* reader);

  static void ExpectArrayOfStringsArgument(
      const std::vector<std::string>& expected_strings,
      dbus::MessageReader* reader);

  // Expects the reader to have a string and a Value.
  static void ExpectStringAndValueArguments(const std::string& expected_string,
                                            const base::Value* expected_value,
                                            dbus::MessageReader* reader);

  // Expects the reader to have a string-to-variant dictionary.
  static void ExpectValueDictionaryArgument(
      const base::Value::Dict* expected_dictionary,
      bool string_valued,
      dbus::MessageReader* reader);

  // Creates a dictionary with example Service properties.
  static base::Value::Dict CreateExampleServiceProperties();

  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;

 private:
  // Checks the requested interface name and signal name.
  // Used to implement the mock proxy.
  void OnConnectToPlatformMessage(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback);

  // Checks the requested interface name and signal name.
  // Used to implement the mock proxy.
  void OnConnectToPacketReceived(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback);

  // Checks the requested interface name and signal name.
  // Used to implement the mock proxy.
  void OnConnectToPropertyChanged(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback);

  // These check the content of the method call and returns the response.
  // Used to implement the mock proxy.
  void OnCallMethod(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback* response_callback);
  void OnCallMethodWithErrorResponse(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseOrErrorCallback* response_callback);
  void OnCallMethodWithErrorCallback(
      dbus::MethodCall* method_call,
      int timeout_ms,
      dbus::ObjectProxy::ResponseCallback* response_callback,
      dbus::ObjectProxy::ErrorCallback* error_callback);

  // The interface name.
  const std::string interface_name_;
  // The object path.
  const dbus::ObjectPath object_path_;
  // The mock object proxy.
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // The PlatformMessage signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback platform_message_handler_;
  // The PacketReceived signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback packet_receieved__handler_;
  // The PropertyChanged signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback property_changed_handler_;
  // The name of the method which is expected to be called.
  std::string expected_method_name_;
  // The response which the mock object proxy returns.
  raw_ptr<dbus::Response, DanglingUntriaged> response_;
  // A callback to intercept and check the method call arguments.
  ArgumentCheckCallback argument_checker_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_CLIENT_UNITTEST_BASE_H_
