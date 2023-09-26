// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_CLIENT_TEST_BASE_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_CLIENT_TEST_BASE_H_

#include "base/test/task_environment.h"
#include "dbus/object_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {
class MockBus;
}  // namespace dbus

namespace ash {

// Base class for Hermes client unittests.
class HermesClientTestBase : public testing::Test {
 public:
  // Pops the top pending MethodCallResult and passes the response and error
  // messages to callback. This is useful in EXPECT_CALL mock invocation of
  // dbus::Bus::CallMethodWithErrorResponse.
  void OnMethodCalled(dbus::MethodCall* method_call,
                      int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback* callback);

 protected:
  HermesClientTestBase();
  ~HermesClientTestBase() override;

  // Queues a pending result that will be passed to the callback in a
  // subsequent call to OnMethodCalled.
  void AddPendingMethodCallResult(
      std::unique_ptr<dbus::Response> response,
      std::unique_ptr<dbus::ErrorResponse> error_response);

  // Returns mock dbus::Bus instance.
  dbus::MockBus* GetMockBus();

  // Initialized mock bus instance.
  void InitMockBus();

 private:
  // Mock bus for simulating calls.
  scoped_refptr<dbus::MockBus> bus_;
  using MethodCallResult = std::pair<std::unique_ptr<dbus::Response>,
                                     std::unique_ptr<dbus::ErrorResponse>>;
  std::deque<MethodCallResult> pending_method_call_results_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_CLIENT_TEST_BASE_H_
