// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/background_portal.h"

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kBackgroundInterfaceName[] = "org.freedesktop.portal.Background";

class BackgroundPortalTest : public testing::Test {
 public:
  void SetUp() override {
    SetPortalStateForTesting(PortalRegistrarState::kSuccess);
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));

    EXPECT_CALL(*bus_, IsConnected()).Times(0);
    EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                      dbus::ObjectPath(kPortalObjectPath)))
        .WillRepeatedly(Return(mock_portal_proxy_.get()));
    EXPECT_CALL(*bus_, GetConnectionName()).WillRepeatedly(Return("test"));
  }

  void TearDown() override {
    SetPortalStateForTesting(PortalRegistrarState::kIdle);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;
};

TEST_F(BackgroundPortalTest, RequestBackgroundSuccess) {
  auto mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));

  EXPECT_CALL(
      *bus_, GetObjectProxy(kPortalServiceName,
                            testing::Property(&dbus::ObjectPath::value,
                                              testing::HasSubstr("/request/"))))
      .WillRepeatedly(Return(mock_request_proxy.get()));

  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetInterface(), kBackgroundInterfaceName);
        EXPECT_EQ(call->GetMember(), "RequestBackground");

        dbus::MessageReader reader(call);
        std::string parent_window;
        EXPECT_TRUE(reader.PopString(&parent_window));
        EXPECT_EQ(parent_window, "handle");

        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it_reason = options->find("reason");
        ASSERT_NE(it_reason, options->end());
        EXPECT_EQ(std::move(it_reason->second).Take<std::string>(),
                  "Testing background");

        auto it_token = options->find("handle_token");
        ASSERT_NE(it_token, options->end());
        auto token = std::move(it_token->second).Take<std::string>();
        ASSERT_TRUE(token);

        std::string request_path =
            base::nix::XdgDesktopPortalRequestPath("test", *token);
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(request_path));
        std::move(callback).Run(response.get(), nullptr);
      });

  dbus::ObjectProxy::SignalCallback response_signal_callback;
  EXPECT_CALL(
      *mock_request_proxy,
      ConnectToSignal("org.freedesktop.portal.Request", "Response", _, _))
      .WillRepeatedly(
          [&](const std::string& interface_name, const std::string& signal_name,
              dbus::ObjectProxy::SignalCallback signal_callback,
              dbus::ObjectProxy::OnConnectedCallback connected_callback) {
            response_signal_callback = signal_callback;
            std::move(connected_callback)
                .Run(interface_name, signal_name, true);
          });

  BackgroundOptions options;
  options.reason = "Testing background";
  options.autostart = true;
  options.commandline = {"chrome", "--no-startup-window"};

  base::expected<BackgroundResult, ResponseError> result;
  base::RunLoop run_loop;

  BackgroundPortal portal(bus_);
  portal.RequestBackground(
      "handle", options,
      base::BindLambdaForTesting(
          [&](base::expected<BackgroundResult, ResponseError> res) {
            result = res;
            run_loop.Quit();
          }));

  dbus::Signal response_signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter writer(&response_signal);
  writer.AppendUint32(0);  // Success response code

  Dictionary dict;
  dict["background"] = dbus_utils::Variant::Wrap<"b">(true);
  dict["autostart"] = dbus_utils::Variant::Wrap<"b">(true);
  dbus_utils::WriteValue(writer, dict);

  response_signal_callback.Run(&response_signal);
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->background_allowed);
  EXPECT_TRUE(result->autostart_enabled);
}

TEST_F(BackgroundPortalTest,
       RequestBackgroundMissingBackgroundKeyReturnsError) {
  auto mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));

  EXPECT_CALL(
      *bus_, GetObjectProxy(kPortalServiceName,
                            testing::Property(&dbus::ObjectPath::value,
                                              testing::HasSubstr("/request/"))))
      .WillRepeatedly(Return(mock_request_proxy.get()));

  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(call);
        std::string parent_window;
        reader.PopString(&parent_window);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        auto it_token = options->find("handle_token");
        auto token = std::move(it_token->second).Take<std::string>();

        std::string request_path =
            base::nix::XdgDesktopPortalRequestPath("test", *token);
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(request_path));
        std::move(callback).Run(response.get(), nullptr);
      });

  dbus::ObjectProxy::SignalCallback response_signal_callback;
  EXPECT_CALL(
      *mock_request_proxy,
      ConnectToSignal("org.freedesktop.portal.Request", "Response", _, _))
      .WillRepeatedly(
          [&](const std::string& interface_name, const std::string& signal_name,
              dbus::ObjectProxy::SignalCallback signal_callback,
              dbus::ObjectProxy::OnConnectedCallback connected_callback) {
            response_signal_callback = signal_callback;
            std::move(connected_callback)
                .Run(interface_name, signal_name, true);
          });

  BackgroundOptions options;
  base::expected<BackgroundResult, ResponseError> result;
  base::RunLoop run_loop;

  BackgroundPortal portal(bus_);
  portal.RequestBackground(
      "", options,
      base::BindLambdaForTesting(
          [&](base::expected<BackgroundResult, ResponseError> res) {
            result = res;
            run_loop.Quit();
          }));

  dbus::Signal response_signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter writer(&response_signal);
  writer.AppendUint32(0);

  // Response dictionary missing the required "background" key
  Dictionary dict;
  dict["autostart"] = dbus_utils::Variant::Wrap<"b">(true);
  dbus_utils::WriteValue(writer, dict);

  response_signal_callback.Run(&response_signal);
  run_loop.Run();

  // Must fail validation cleanly rather than silently defaulting to false
  // (denial)
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ResponseError::kInvalidSignalResponse);
}

TEST_F(BackgroundPortalTest, RequestBackgroundFailsWhenPortalUnavailable) {
  SetPortalInterfaceVersionForTesting(kBackgroundInterfaceName, 0);
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .Times(0);

  BackgroundOptions options;
  base::expected<BackgroundResult, ResponseError> result;
  base::RunLoop run_loop;

  BackgroundPortal portal(bus_);
  portal.RequestBackground(
      "", options,
      base::BindLambdaForTesting(
          [&](base::expected<BackgroundResult, ResponseError> res) {
            result = res;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ResponseError::kMethodCallFailed);
}

TEST_F(BackgroundPortalTest, NullBusFailsImmediately) {
  BackgroundPortal portal(nullptr);

  base::expected<BackgroundResult, ResponseError> request_result;
  portal.RequestBackground(
      "handle", BackgroundOptions(),
      base::BindLambdaForTesting(
          [&](base::expected<BackgroundResult, ResponseError> res) {
            request_result = res;
          }));
  ASSERT_FALSE(request_result.has_value());
  EXPECT_EQ(request_result.error(), ResponseError::kMethodCallFailed);

  bool status_success = true;
  portal.SetStatus("Running in background",
                   base::BindLambdaForTesting(
                       [&](bool success) { status_success = success; }));
  EXPECT_FALSE(status_success);
}

TEST_F(BackgroundPortalTest,
       RequestBackgroundClosesInFlightRequestBeforeStartingNewOne) {
  auto mock_request_proxy1 = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));
  auto mock_request_proxy2 = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/2"));

  dbus::ObjectPath request_path1;
  EXPECT_CALL(
      *bus_, GetObjectProxy(kPortalServiceName,
                            testing::Property(&dbus::ObjectPath::value,
                                              testing::HasSubstr("/request/"))))
      .WillRepeatedly(
          [&](std::string_view,
              const dbus::ObjectPath& object_path) -> dbus::ObjectProxy* {
            if (request_path1.value().empty()) {
              request_path1 = object_path;
            }
            return object_path == request_path1 ? mock_request_proxy1.get()
                                                : mock_request_proxy2.get();
          });

  EXPECT_CALL(
      *mock_request_proxy1,
      ConnectToSignal("org.freedesktop.portal.Request", "Response", _, _))
      .WillOnce([](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback,
                   dbus::ObjectProxy::OnConnectedCallback connected_callback) {
        std::move(connected_callback).Run(interface_name, signal_name, true);
      });

  dbus::ObjectProxy::SignalCallback response_signal_callback2;
  EXPECT_CALL(
      *mock_request_proxy2,
      ConnectToSignal("org.freedesktop.portal.Request", "Response", _, _))
      .WillOnce([&](const std::string& interface_name,
                    const std::string& signal_name,
                    dbus::ObjectProxy::SignalCallback signal_callback,
                    dbus::ObjectProxy::OnConnectedCallback connected_callback) {
        response_signal_callback2 = signal_callback;
        std::move(connected_callback).Run(interface_name, signal_name, true);
      });

  auto reply_with_request_path =
      [](dbus::MethodCall* call, int timeout_ms,
         dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetInterface(), kBackgroundInterfaceName);
        EXPECT_EQ(call->GetMember(), "RequestBackground");
        dbus::MessageReader reader(call);
        std::string parent_window;
        ASSERT_TRUE(reader.PopString(&parent_window));
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it_token = options->find("handle_token");
        ASSERT_NE(it_token, options->end());
        auto token = std::move(it_token->second).Take<std::string>();
        ASSERT_TRUE(token);
        std::string path =
            base::nix::XdgDesktopPortalRequestPath("test", *token);
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(path));
        std::move(callback).Run(response.get(), nullptr);
      };

  {
    InSequence seq;
    EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillOnce(reply_with_request_path);
    EXPECT_CALL(*mock_request_proxy1, CallMethod(_, _, _))
        .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                     dbus::ObjectProxy::ResponseCallback callback) {
          EXPECT_EQ(call->GetInterface(), "org.freedesktop.portal.Request");
          EXPECT_EQ(call->GetMember(), "Close");
          auto response = dbus::Response::CreateEmpty();
          std::move(callback).Run(response.get());
        });
    EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillOnce(reply_with_request_path);
  }

  BackgroundPortal portal(bus_);
  BackgroundOptions options;
  bool first_callback_called = false;
  portal.RequestBackground(
      "handle1", options,
      base::BindLambdaForTesting(
          [&](base::expected<BackgroundResult, ResponseError>) {
            first_callback_called = true;
          }));

  base::expected<BackgroundResult, ResponseError> second_result;
  base::RunLoop run_loop;
  portal.RequestBackground(
      "handle2", options,
      base::BindLambdaForTesting(
          [&](base::expected<BackgroundResult, ResponseError> res) {
            second_result = res;
            run_loop.Quit();
          }));

  dbus::Signal response_signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter writer(&response_signal);
  writer.AppendUint32(0);
  Dictionary dict;
  dict["background"] = dbus_utils::Variant::Wrap<"b">(true);
  dbus_utils::WriteValue(writer, dict);

  response_signal_callback2.Run(&response_signal);
  run_loop.Run();

  EXPECT_FALSE(first_callback_called);
  ASSERT_TRUE(second_result.has_value());
  EXPECT_TRUE(second_result->background_allowed);
}

TEST_F(BackgroundPortalTest, SetStatusSuccess) {
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetInterface(), kBackgroundInterfaceName);
        EXPECT_EQ(call->GetMember(), "SetStatus");

        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("message");
        ASSERT_NE(it, options->end());
        EXPECT_EQ(std::move(it->second).Take<std::string>(),
                  "Running in background");

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  bool status_success = false;
  base::RunLoop run_loop;
  BackgroundPortal portal(bus_);
  portal.SetStatus("Running in background",
                   base::BindLambdaForTesting([&](bool success) {
                     status_success = success;
                     run_loop.Quit();
                   }));

  run_loop.Run();
  EXPECT_TRUE(status_success);
}

TEST_F(BackgroundPortalTest, SetStatusRequiresVersion2) {
  SetPortalInterfaceVersionForTesting(kBackgroundInterfaceName, 1);
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .Times(0);

  bool status_success = true;
  base::RunLoop run_loop;
  BackgroundPortal portal(bus_);
  portal.SetStatus("Running in background",
                   base::BindLambdaForTesting([&](bool success) {
                     status_success = success;
                     run_loop.Quit();
                   }));

  run_loop.Run();
  EXPECT_FALSE(status_success);
}

TEST_F(BackgroundPortalTest, SetStatusTruncatesAndNormalizesNewlines) {
  // Exact 96-character ASCII message is accepted without truncation.
  const std::string exact_96_msg(96, 'x');
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("message");
        ASSERT_NE(it, options->end());
        EXPECT_EQ(std::move(it->second).Take<std::string>(), exact_96_msg);

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  BackgroundPortal portal(bus_);
  {
    bool status_success = false;
    base::RunLoop run_loop;
    portal.SetStatus(exact_96_msg,
                     base::BindLambdaForTesting([&](bool success) {
                       status_success = success;
                       run_loop.Quit();
                     }));
    run_loop.Run();
    EXPECT_TRUE(status_success);
  }

  // Input string with newlines and >96 characters is normalized and truncated
  // to 96 code points.
  std::string long_msg_with_newlines =
      "Running in background\r\nLine two\n" + std::string(100, 'a');

  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("message");
        ASSERT_NE(it, options->end());
        std::string sent_msg =
            std::move(it->second).Take<std::string>().value();

        EXPECT_EQ(sent_msg.find('\n'), std::string::npos);
        EXPECT_EQ(sent_msg.find('\r'), std::string::npos);
        EXPECT_EQ(sent_msg.length(), 96u);

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  bool status_success = false;
  base::RunLoop run_loop;
  portal.SetStatus(long_msg_with_newlines,
                   base::BindLambdaForTesting([&](bool success) {
                     status_success = success;
                     run_loop.Quit();
                   }));

  run_loop.Run();
  EXPECT_TRUE(status_success);
}

TEST_F(BackgroundPortalTest, SetStatusMultibyteUTF8Truncation) {
  // 3-byte UTF-8 character (e.g. '好' = \xe5\xa5\xbd) repeated 100 times (300
  // bytes = 100 code points).
  std::string multibyte_str;
  for (int i = 0; i < 100; ++i) {
    multibyte_str += "\xe5\xa5\xbd";
  }

  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("message");
        ASSERT_NE(it, options->end());
        std::string sent_msg =
            std::move(it->second).Take<std::string>().value();

        // Truncated to 96 UTF-8 code points (96 * 3 = 288 bytes), preserving
        // full code points rather than cutting at 96 bytes.
        EXPECT_EQ(sent_msg.length(), 288u);
        EXPECT_TRUE(base::IsStringUTF8(sent_msg));

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  bool status_success = false;
  base::RunLoop run_loop;
  BackgroundPortal portal(bus_);
  portal.SetStatus(multibyte_str, base::BindLambdaForTesting([&](bool success) {
                     status_success = success;
                     run_loop.Quit();
                   }));

  run_loop.Run();
  EXPECT_TRUE(status_success);

  // Incomplete trailing UTF-8 sequence is stripped so D-Bus receives valid
  // UTF-8.
  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("message");
        ASSERT_NE(it, options->end());
        EXPECT_EQ(std::move(it->second).Take<std::string>(), "abc");

        auto response = dbus::Response::CreateEmpty();
        std::move(callback).Run(response.get(), nullptr);
      });

  bool invalid_utf8_success = false;
  base::RunLoop invalid_utf8_loop;
  portal.SetStatus("abc\xe5\xa5", base::BindLambdaForTesting([&](bool success) {
                     invalid_utf8_success = success;
                     invalid_utf8_loop.Quit();
                   }));
  invalid_utf8_loop.Run();
  EXPECT_TRUE(invalid_utf8_success);
}

}  // namespace

}  // namespace dbus_xdg
