// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace ash {

namespace {

using ::logging::LOGGING_ERROR;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Return;

// Matches the D-Bus method call for GetRgbKeyboardCapabilities.
MATCHER(IsGetRgbKeyboardCapabilitiesCall, "") {
  return arg->GetInterface() == rgbkbd::kRgbkbdServiceName &&
         arg->GetMember() == rgbkbd::kGetRgbKeyboardCapabilities;
}

// Returns a gMock action that answers the D-Bus call with `capabilities`.
auto ReplyWithCapabilities(rgbkbd::RgbKeyboardCapabilities capabilities) {
  return [capabilities](dbus::MethodCall* method_call, int timeout_ms,
                        dbus::ObjectProxy::ResponseOrErrorCallback callback) {
    std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendUint32(static_cast<uint32_t>(capabilities));
    std::move(callback).Run(response.get(), /*error_response=*/nullptr);
  };
}

// Returns a gMock action that fails the D-Bus call with `error_name`.
auto ReplyWithError(const std::string& error_name) {
  return [error_name](dbus::MethodCall* method_call, int timeout_ms,
                      dbus::ObjectProxy::ResponseOrErrorCallback callback) {
    // ErrorResponse::FromMethodCall() needs the call to have a serial.
    method_call->SetSerial(1);
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(method_call, error_name,
                                            "Test error");
    std::move(callback).Run(/*response=*/nullptr, error_response.get());
  };
}

}  // namespace

class RgbkbdClientTest : public testing::Test {
 public:
  RgbkbdClientTest() = default;
  RgbkbdClientTest(const RgbkbdClientTest&) = delete;
  RgbkbdClientTest& operator=(const RgbkbdClientTest&) = delete;
  ~RgbkbdClientTest() override = default;

  void SetUp() override {
    const dbus::ObjectPath path(rgbkbd::kRgbkbdServicePath);
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), rgbkbd::kRgbkbdServiceName, path);
    EXPECT_CALL(*bus_, GetObjectProxy(rgbkbd::kRgbkbdServiceName, path))
        .WillOnce(Return(proxy_.get()));
    // `Init()` subscribes to the CapabilityUpdatedForTesting signal.
    EXPECT_CALL(*proxy_,
                ConnectToSignal(rgbkbd::kRgbkbdServiceName,
                                rgbkbd::kCapabilityUpdatedForTesting, _, _));
    RgbkbdClient::Initialize(bus_.get());

    // Allow any log message. Tests add stricter expectations on top.
    EXPECT_CALL(mock_log_, Log).Times(AnyNumber());
    mock_log_.StartCapturingLogs();
  }

  void TearDown() override {
    mock_log_.StopCapturingLogs();
    RgbkbdClient::Shutdown();
  }

 protected:
  // Calls `RgbkbdClient::GetRgbKeyboardCapabilities()` and returns the result.
  std::optional<rgbkbd::RgbKeyboardCapabilities> GetCapabilities() {
    base::test::TestFuture<std::optional<rgbkbd::RgbKeyboardCapabilities>>
        future;
    RgbkbdClient::Get()->GetRgbKeyboardCapabilities(future.GetCallback());
    return future.Get();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> proxy_;
  base::test::MockLog mock_log_;
};

TEST_F(RgbkbdClientTest, ReturnsCapabilities) {
  EXPECT_CALL(*proxy_, CallMethodWithErrorResponse(
                           IsGetRgbKeyboardCapabilitiesCall(),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(ReplyWithCapabilities(
          rgbkbd::RgbKeyboardCapabilities::kIndividualKey));
  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, _, _, _, _)).Times(0);

  EXPECT_EQ(GetCapabilities(), rgbkbd::RgbKeyboardCapabilities::kIndividualKey);
}

// rgbkbd isn't installed on some images (e.g. ChromeOS Flex), so D-Bus fails
// with ServiceUnknown. That means there is no RGB keyboard, not an error.
TEST_F(RgbkbdClientTest, ServiceUnknownMeansNoRgbKeyboard) {
  EXPECT_CALL(*proxy_, CallMethodWithErrorResponse(
                           IsGetRgbKeyboardCapabilitiesCall(), _, _))
      .WillOnce(ReplyWithError(DBUS_ERROR_SERVICE_UNKNOWN));
  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, _, _, _, _)).Times(0);

  EXPECT_EQ(GetCapabilities(), rgbkbd::RgbKeyboardCapabilities::kNone);
}

// Any other error is a real failure, so it's logged with the error name.
TEST_F(RgbkbdClientTest, OtherErrorIsLogged) {
  EXPECT_CALL(*proxy_, CallMethodWithErrorResponse(
                           IsGetRgbKeyboardCapabilitiesCall(), _, _))
      .WillOnce(ReplyWithError(DBUS_ERROR_NO_REPLY));
  EXPECT_CALL(mock_log_, Log(LOGGING_ERROR, EndsWith("rgbkbd_client.cc"), _, _,
                             HasSubstr(DBUS_ERROR_NO_REPLY)));

  EXPECT_EQ(GetCapabilities(), std::nullopt);
}

TEST_F(RgbkbdClientTest, NoResponseIsLogged) {
  EXPECT_CALL(*proxy_, CallMethodWithErrorResponse(
                           IsGetRgbKeyboardCapabilitiesCall(), _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        std::move(callback).Run(/*response=*/nullptr,
                                /*error_response=*/nullptr);
      });
  EXPECT_CALL(mock_log_,
              Log(LOGGING_ERROR, EndsWith("rgbkbd_client.cc"), _, _, _));

  EXPECT_EQ(GetCapabilities(), std::nullopt);
}

TEST_F(RgbkbdClientTest, InvalidResponseIsLogged) {
  EXPECT_CALL(*proxy_, CallMethodWithErrorResponse(
                           IsGetRgbKeyboardCapabilitiesCall(), _, _))
      .WillOnce([](dbus::MethodCall* method_call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        // A reply without the expected uint32.
        std::unique_ptr<dbus::Response> response =
            dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), /*error_response=*/nullptr);
      });
  EXPECT_CALL(mock_log_,
              Log(LOGGING_ERROR, EndsWith("rgbkbd_client.cc"), _, _, _));

  EXPECT_EQ(GetCapabilities(), std::nullopt);
}

}  // namespace ash
