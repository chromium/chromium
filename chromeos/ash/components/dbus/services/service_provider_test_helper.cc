// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/object_path.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::Unused;

namespace ash {

ServiceProviderTestHelper::ServiceProviderTestHelper() = default;

ServiceProviderTestHelper::~ServiceProviderTestHelper() = default;

void ServiceProviderTestHelper::SetUp(
    const std::string& service_name,
    const dbus::ObjectPath& service_path,
    const std::string& interface_name,
    const std::string& exported_method_name,
    CrosDBusService::ServiceProviderInterface* service_provider) {
  exported_method_name_ = exported_method_name;
  // Create a mock bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  mock_bus_ = new dbus::MockBus(options);

  // ShutdownAndBlock() will be called in TearDown().
  EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

  // Create a mock exported object that behaves as the service.
  mock_exported_object_ =
      new dbus::MockExportedObject(mock_bus_.get(), service_path);

  // |mock_exported_object_|'s ExportMethod() will use
  // |MockExportedObject().
  EXPECT_CALL(*mock_exported_object_.get(),
              ExportMethod(interface_name, _, _, _))
      .WillRepeatedly(
          Invoke(this, &ServiceProviderTestHelper::MockExportMethod));

  // Create a mock object proxy, with which we call a method of
  // |mock_exported_object_|.
  mock_object_proxy_ =
      new dbus::MockObjectProxy(mock_bus_.get(), service_name, service_path);
  // |mock_object_proxy_|'s CallMethodAndBlock() will use CallMethodAndBlock()
  // to return responses.
  EXPECT_CALL(*mock_object_proxy_.get(),
              CallMethodAndBlock(
                  AllOf(ResultOf(std::mem_fn(&dbus::MethodCall::GetInterface),
                                 interface_name),
                        ResultOf(std::mem_fn(&dbus::MethodCall::GetMember),
                                 exported_method_name)),
                  _))
      .WillOnce(Invoke(this, &ServiceProviderTestHelper::CallMethodAndBlock));

  service_provider->Start(mock_exported_object_.get());
}

void ServiceProviderTestHelper::TearDown() {
  mock_bus_->ShutdownAndBlock();
  mock_exported_object_.reset();
  mock_object_proxy_.reset();
  mock_bus_.reset();
}

void ServiceProviderTestHelper::SetUpReturnSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
  // |mock_exported_object_|'s SendSignal() will use
  // MockSendSignal().
  EXPECT_CALL(*mock_exported_object_.get(), SendSignal(_))
      .WillOnce(Invoke(this, &ServiceProviderTestHelper::MockSendSignal));

  // |mock_object_proxy_|'s ConnectToSignal will use
  // MockConnectToSignal().
  EXPECT_CALL(*mock_object_proxy_.get(),
              DoConnectToSignal(interface_name, signal_name, _, _))
      .WillOnce(Invoke(this, &ServiceProviderTestHelper::MockConnectToSignal));

  mock_object_proxy_->ConnectToSignal(interface_name, signal_name,
                                      signal_callback,
                                      std::move(on_connected_callback));
}

std::unique_ptr<dbus::Response> ServiceProviderTestHelper::CallMethod(
    dbus::MethodCall* method_call) {
  return mock_object_proxy_
      ->CallMethodAndBlock(method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
      .value_or(nullptr);
}

void ServiceProviderTestHelper::MockExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    dbus::ExportedObject::MethodCallCallback method_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback) {
  // Tell the call back that the method is exported successfully.
  std::move(on_exported_callback).Run(interface_name, method_name, true);
  // Capture the callback, so we can run this at a later time.
  if (method_name == exported_method_name_) {
    method_callback_ = method_callback;
  }
}

std::unique_ptr<dbus::Response> ServiceProviderTestHelper::CallMethodAndBlock(
    dbus::MethodCall* method_call,
    Unused) {
  // Set the serial number to non-zero, so
  // dbus_message_new_method_return() won't emit a warning.
  method_call->SetSerial(1);
  // Run the callback captured in MockExportMethod(). In addition to returning
  // a response that the caller will ignore, this will send a signal, which
  // will be received by |on_signal_callback_|.
  std::unique_ptr<dbus::Response> response;
  method_callback_.Run(method_call,
                       base::BindOnce(&ServiceProviderTestHelper::OnResponse,
                                      base::Unretained(this), &response));
  // Check for a response.
  if (!response)
    loop_.Run();
  // Return response.
  return response;
}

void ServiceProviderTestHelper::MockConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback* connected_callback) {
  // Tell the callback that the object proxy is connected to the signal.
  std::move(*connected_callback).Run(interface_name, signal_name, true);
  // Capture the callback, so we can run this at a later time.
  on_signal_callback_ = signal_callback;
}

void ServiceProviderTestHelper::MockSendSignal(dbus::Signal* signal) {
  // Run the callback captured in MockConnectToSignal(). This will call
  // OnSignalReceived().
  on_signal_callback_.Run(signal);
}

void ServiceProviderTestHelper::OnResponse(
    std::unique_ptr<dbus::Response>* out_response,
    std::unique_ptr<dbus::Response> response) {
  *out_response = std::move(response);
  loop_.QuitWhenIdle();
}

}  // namespace ash
