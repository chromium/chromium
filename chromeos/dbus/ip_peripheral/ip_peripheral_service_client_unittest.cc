// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Return;

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

}  // namespace

class IpPeripheralServiceClientTest : public testing::Test {
 public:
  IpPeripheralServiceClientTest() = default;
  ~IpPeripheralServiceClientTest() override = default;

  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    // Suppress warnings about uninteresting calls to the bus.
    mock_bus_ = base::MakeRefCounted<::testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());

    // Setup mock bus and proxy.
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), ip_peripheral::kIpPeripheralServiceName,
        dbus::ObjectPath(ip_peripheral::kIpPeripheralServicePath));

    ON_CALL(*mock_bus_.get(),
            GetObjectProxy(
                ip_peripheral::kIpPeripheralServiceName,
                dbus::ObjectPath(ip_peripheral::kIpPeripheralServicePath)))
        .WillByDefault(Return(mock_proxy_.get()));

    // Create a client with the mock bus.
    IpPeripheralServiceClient::Initialize(mock_bus_.get());
    client_ = IpPeripheralServiceClient::Get();

    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { IpPeripheralServiceClient::Shutdown(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<IpPeripheralServiceClient, ExperimentalAsh> client_ = nullptr;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
};

TEST_F(IpPeripheralServiceClientTest, GetPanDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(ip_peripheral::kGetPanMethod), _, _));

  client_->GetPan("", base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(IpPeripheralServiceClientTest, GetTiltDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(ip_peripheral::kGetTiltMethod), _, _));

  client_->GetTilt("", base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(IpPeripheralServiceClientTest, GetZoomDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(ip_peripheral::kGetZoomMethod), _, _));

  client_->GetZoom("", base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(IpPeripheralServiceClientTest, SetPanDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(ip_peripheral::kSetPanMethod), _, _));

  client_->SetPan("", 0, base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(IpPeripheralServiceClientTest, SetTiltDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(ip_peripheral::kSetTiltMethod), _, _));

  client_->SetTilt("", 0, base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_F(IpPeripheralServiceClientTest, SetZoomDBusMessage) {
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(ip_peripheral::kSetZoomMethod), _, _));

  client_->SetZoom("", 0, base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
