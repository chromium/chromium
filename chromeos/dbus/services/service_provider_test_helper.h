// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICES_SERVICE_PROVIDER_TEST_HELPER_H_
#define CHROMEOS_DBUS_SERVICES_SERVICE_PROVIDER_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/mock_exported_object.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

class MockBus;
class ObjectPath;

}  // namespace dbus

namespace chromeos {

// Helps to implement |CrosDBusService::ServiceProviderInterface| unittests.
// Setups mocking of dbus classes.
// Class can test only one method call in time. SetUp() must be called before
// testing new call to the same method or different method.
//
// Sample usage:
//   ServiceProviderTestHelper helper;
//   helper.Setup(...);
//   helper.SetUpReturnSignal(...); // optional.
//   helper.CallMethod(...);
//   helper.TearDown();
class ServiceProviderTestHelper {
 public:
  ServiceProviderTestHelper();
  ~ServiceProviderTestHelper();

  // Sets up helper. Should be called before |CallMethod()|.
  void SetUp(const std::string& service_name,
             const dbus::ObjectPath& service_path,
             const std::string& interface_name,
             const std::string& exported_method_name,
             CrosDBusService::ServiceProviderInterface* service_provider);

  // Setups return signal callback. It's optional and don't need to be called
  // if tested method doesn't use signal to return results.
  void SetUpReturnSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback);

  // Calls tested dbus method.
  std::unique_ptr<dbus::Response> CallMethod(dbus::MethodCall* method_call);

  // Cleanups helper. Should be called after |CallMethod()|.
  void TearDown();

 private:
  // Behaves as |mock_exported_object_|'s ExportMethod().
  void MockExportMethod(
      const std::string& interface_name,
      const std::string& method_name,
      dbus::ExportedObject::MethodCallCallback method_callback,
      dbus::ExportedObject::OnExportedCallback on_exported_callback);

  // Calls exported method and waits for a response for |mock_object_proxy_|.
  std::unique_ptr<dbus::Response> CallMethodAndBlock(
      dbus::MethodCall* method_call,
      ::testing::Unused);

  // Behaves as |mock_object_proxy_|'s ConnectToSignal().
  void MockConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* connected_callback);

  // Behaves as |mock_exported_object_|'s SendSignal().
  void MockSendSignal(dbus::Signal* signal);

  // Receives a response and makes it available to MockCallMethodAndBlock().
  void OnResponse(std::unique_ptr<dbus::Response>* out_response,
                  std::unique_ptr<dbus::Response> response);

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy_;
  dbus::ExportedObject::MethodCallCallback method_callback_;
  dbus::ObjectProxy::SignalCallback on_signal_callback_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::string exported_method_name_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SERVICES_SERVICE_PROVIDER_TEST_HELPER_H_
