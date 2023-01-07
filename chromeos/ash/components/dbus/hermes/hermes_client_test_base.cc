// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hermes/hermes_client_test_base.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"

namespace ash {

namespace {

void RunResponseOrErrorCallback(
    dbus::ObjectProxy::ResponseOrErrorCallback callback,
    std::unique_ptr<dbus::Response> response,
    std::unique_ptr<dbus::ErrorResponse> error_response) {
  std::move(callback).Run(response.get(), error_response.get());
}

}  // namespace

HermesClientTestBase::HermesClientTestBase() = default;
HermesClientTestBase::~HermesClientTestBase() = default;

void HermesClientTestBase::OnMethodCalled(
    dbus::MethodCall* method_call,
    int timeout_ms,
    dbus::ObjectProxy::ResponseOrErrorCallback* callback) {
  ASSERT_FALSE(pending_method_call_results_.empty());
  MethodCallResult result = std::move(pending_method_call_results_.front());
  pending_method_call_results_.pop_front();
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunResponseOrErrorCallback, std::move(*callback),
                     std::move(result.first), std::move(result.second)));
}

void HermesClientTestBase::AddPendingMethodCallResult(
    std::unique_ptr<dbus::Response> response,
    std::unique_ptr<dbus::ErrorResponse> error_response) {
  pending_method_call_results_.emplace_back(std::move(response),
                                            std::move(error_response));
}

dbus::MockBus* HermesClientTestBase::GetMockBus() {
  return bus_.get();
}

void HermesClientTestBase::InitMockBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::MockBus(options);
}

}  // namespace ash
