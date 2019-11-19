// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cec_service_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::ContainerEq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

namespace {

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

// Functor that can be passed to GoogleMock to respond to a GetTvsPowerStatus
// D-Bus method call with the given power states.
class GetTvsPowerStatusHandler {
 public:
  explicit GetTvsPowerStatusHandler(std::vector<int32_t> power_states)
      : power_states_(power_states) {}

  void operator()(dbus::MethodCall* method_call,
                  int timeout_ms,
                  dbus::ObjectProxy::ResponseCallback* callback) {
    method_call->SetSerial(1);  // arbitrary but needed by FromMethodCall
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);

    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("i", &array_writer);
    for (int32_t power_state : power_states_)
      array_writer.AppendInt32(power_state);
    writer.CloseContainer(&array_writer);

    // Run the response callback asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*callback), base::Owned(response.release())));
  }

 private:
  std::vector<int32_t> power_states_;
};

}  // namespace

class CecServiceClientTest : public testing::Test {
 public:
  CecServiceClientTest() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    // Suppress warnings about uninteresting calls to the bus.
    mock_bus_ = base::MakeRefCounted<::testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());

    // Setup mock bus and proxy.
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), cecservice::kCecServiceName,
        dbus::ObjectPath(cecservice::kCecServicePath));

    ON_CALL(*mock_bus_.get(),
            GetObjectProxy(cecservice::kCecServiceName,
                           dbus::ObjectPath(cecservice::kCecServicePath)))
        .WillByDefault(Return(mock_proxy_.get()));

    // Create a client with the mock bus.
    client_ = CecServiceClient::Create();
    client_->Init(mock_bus_.get());

    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<CecServiceClient> client_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
};

TEST_F(CecServiceClientTest, SendStandByTriggersDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(
                  HasMember(cecservice::kSendStandByToAllDevicesMethod), _, _));

  client_->SendStandBy();
}

TEST_F(CecServiceClientTest, SendWakeUpTriggersDBusMessage) {
  EXPECT_CALL(
      *mock_proxy_.get(),
      DoCallMethod(HasMember(cecservice::kSendWakeUpToAllDevicesMethod), _, _));

  client_->SendWakeUp();
}

TEST_F(CecServiceClientTest, QueryPowerStatusSendDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(cecservice::kGetTvsPowerStatus), _, _));

  client_->QueryDisplayCecPowerState(base::DoNothing());

  base::RunLoop().RunUntilIdle();
}

TEST_F(CecServiceClientTest, QueryPowerStatusNoCecDevicesGivesEmptyResponse) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(cecservice::kGetTvsPowerStatus), _, _))
      .WillOnce(Invoke(GetTvsPowerStatusHandler({})));

  base::MockCallback<CecServiceClient::PowerStateCallback> callback;
  EXPECT_CALL(callback,
              Run(ContainerEq(std::vector<CecServiceClient::PowerState>())));

  client_->QueryDisplayCecPowerState(callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CecServiceClientTest, QueryPowerStatusOneDeviceIsPropagated) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(cecservice::kGetTvsPowerStatus), _, _))
      .WillOnce(
          Invoke(GetTvsPowerStatusHandler({cecservice::kTvPowerStatusOn})));

  base::MockCallback<CecServiceClient::PowerStateCallback> callback;
  EXPECT_CALL(callback,
              Run(ContainerEq(std::vector<CecServiceClient::PowerState>(
                  {CecServiceClient::PowerState::kOn}))));

  client_->QueryDisplayCecPowerState(callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(CecServiceClientTest, QueryPowerStatusAllStatesCorrectlyHandled) {
  std::vector<int32_t> power_states{
      cecservice::kTvPowerStatusError,
      cecservice::kTvPowerStatusAdapterNotConfigured,
      cecservice::kTvPowerStatusNoTv,
      cecservice::kTvPowerStatusOn,
      cecservice::kTvPowerStatusStandBy,
      cecservice::kTvPowerStatusToOn,
      cecservice::kTvPowerStatusToStandBy,
      cecservice::kTvPowerStatusUnknown,
  };

  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(cecservice::kGetTvsPowerStatus), _, _))
      .WillOnce(Invoke(GetTvsPowerStatusHandler(std::move(power_states))));

  base::MockCallback<CecServiceClient::PowerStateCallback> callback;
  EXPECT_CALL(callback,
              Run(ContainerEq(std::vector<CecServiceClient::PowerState>({
                  CecServiceClient::PowerState::kError,
                  CecServiceClient::PowerState::kAdapterNotConfigured,
                  CecServiceClient::PowerState::kNoDevice,
                  CecServiceClient::PowerState::kOn,
                  CecServiceClient::PowerState::kStandBy,
                  CecServiceClient::PowerState::kTransitioningToOn,
                  CecServiceClient::PowerState::kTransitioningToStandBy,
                  CecServiceClient::PowerState::kUnknown,
              }))));

  client_->QueryDisplayCecPowerState(callback.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
