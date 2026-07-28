// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/session.h"

#include "base/functional/callback_helpers.h"
#include "base/nix/xdg_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace dbus_xdg {

constexpr char kTestSessionPath[] =
    "/org/freedesktop/portal/desktop/session/1_0";

class SessionTest : public testing::Test {
 public:
  void SetUp() override {
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    mock_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName, dbus::ObjectPath(kTestSessionPath));

    EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                      dbus::ObjectPath(kTestSessionPath)))
        .WillRepeatedly(Return(mock_proxy_.get()));
    EXPECT_CALL(*bus_, GetConnectionName()).WillRepeatedly(Return("test"));
  }

  std::unique_ptr<Session> CreateTestSession(
      const dbus::ObjectPath& session_path) {
    // base::WrapUnique(new Session(...)) is used here to access the private
    // constructor for testing.
    return base::WrapUnique(new Session(bus_, session_path));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
};

TEST_F(SessionTest, DestructorCallsClose) {
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _));

  auto session = CreateTestSession(dbus::ObjectPath(kTestSessionPath));
  EXPECT_EQ(session->path(), dbus::ObjectPath(kTestSessionPath));
}

TEST_F(SessionTest, CreateSessionSuccessDirect) {
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop"));

  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "CreateSession");
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(kTestSessionPath));
        std::move(callback).Run(response.get(), nullptr);
      });

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _));

  Session* create_result = nullptr;
  auto session = Session::CreateDirect(
      bus_, mock_portal_proxy.get(), "org.freedesktop.portal.Location",
      Dictionary(),
      base::BindLambdaForTesting([&](Session* sess) { create_result = sess; }));

  task_environment_.RunUntilIdle();

  EXPECT_EQ(create_result, session.get());
  ASSERT_TRUE(session);
  EXPECT_EQ(session->path(), dbus::ObjectPath(kTestSessionPath));
}

TEST_F(SessionTest, CreateSessionSuccessRequest) {
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop"));

  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "CreateSession");
        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("handle_token");
        ASSERT_NE(it, options->end());
        auto token = std::move(it->second).Take<std::string>();
        ASSERT_TRUE(token);
        std::string request_path =
            base::nix::XdgDesktopPortalRequestPath("test", *token);
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(request_path));
        std::move(callback).Run(response.get(), nullptr);
      });

  auto mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));

  EXPECT_CALL(
      *bus_, GetObjectProxy(kPortalServiceName,
                            testing::Property(&dbus::ObjectPath::value,
                                              testing::HasSubstr("/request/"))))
      .WillRepeatedly(Return(mock_request_proxy.get()));

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

  Session* create_result = nullptr;
  auto session = Session::CreateWithRequest(
      bus_, mock_portal_proxy.get(), "org.freedesktop.portal.GlobalShortcuts",
      Dictionary(),
      base::BindLambdaForTesting([&](Session* sess) { create_result = sess; }));

  dbus::Signal response_signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter writer(&response_signal);
  writer.AppendUint32(0);

  Dictionary dict;
  dict["session_handle"] = dbus_utils::Variant::Wrap<"s">(kTestSessionPath);
  dbus_utils::WriteValue(writer, dict);

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _));

  response_signal_callback.Run(&response_signal);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(create_result, session.get());
  ASSERT_TRUE(session);
  EXPECT_EQ(session->path(), dbus::ObjectPath(kTestSessionPath));
}

TEST_F(SessionTest, CreateSessionSuccessRequestObjectPath) {
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop"));

  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "CreateSession");
        dbus::MessageReader reader(call);
        auto options = dbus_utils::ReadValue<Dictionary>(reader);
        ASSERT_TRUE(options);
        auto it = options->find("handle_token");
        ASSERT_NE(it, options->end());
        auto token = std::move(it->second).Take<std::string>();
        ASSERT_TRUE(token);
        std::string request_path =
            base::nix::XdgDesktopPortalRequestPath("test", *token);
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(dbus::ObjectPath(request_path));
        std::move(callback).Run(response.get(), nullptr);
      });

  auto mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));

  EXPECT_CALL(
      *bus_, GetObjectProxy(kPortalServiceName,
                            testing::Property(&dbus::ObjectPath::value,
                                              testing::HasSubstr("/request/"))))
      .WillRepeatedly(Return(mock_request_proxy.get()));

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

  Session* create_result = nullptr;
  auto session = Session::CreateWithRequest(
      bus_, mock_portal_proxy.get(), "org.freedesktop.portal.InputCapture",
      Dictionary(),
      base::BindLambdaForTesting([&](Session* sess) { create_result = sess; }));

  dbus::Signal response_signal("org.freedesktop.portal.Request", "Response");
  dbus::MessageWriter writer(&response_signal);
  writer.AppendUint32(0);

  Dictionary dict;
  dict["session_handle"] =
      dbus_utils::Variant::Wrap<"o">(dbus::ObjectPath(kTestSessionPath));
  dbus_utils::WriteValue(writer, dict);

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _));

  response_signal_callback.Run(&response_signal);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(create_result, session.get());
  ASSERT_TRUE(session);
  EXPECT_EQ(session->path(), dbus::ObjectPath(kTestSessionPath));
}

TEST_F(SessionTest, CreateSessionCancelledClosesSession) {
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop"));

  dbus::ObjectProxy::ResponseOrErrorCallback create_session_callback;
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "CreateSession");
        create_session_callback = std::move(callback);
      });

  {
    auto session = Session::CreateDirect(bus_, mock_portal_proxy.get(),
                                         "org.freedesktop.portal.Location",
                                         Dictionary(), base::DoNothing());
  }

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "Close");
        std::move(callback).Run(nullptr, nullptr);
      });

  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(dbus::ObjectPath(kTestSessionPath));
  std::move(create_session_callback).Run(response.get(), nullptr);
}

TEST_F(SessionTest, SessionDestroyedBeforeCallbackRuns) {
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop"));

  dbus::ObjectProxy::ResponseOrErrorCallback create_session_callback;
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "CreateSession");
        create_session_callback = std::move(callback);
      });

  Session* create_result = reinterpret_cast<Session*>(0xdeadbeef);
  bool callback_called = false;
  auto session = Session::CreateDirect(
      bus_, mock_portal_proxy.get(), "org.freedesktop.portal.Location",
      Dictionary(), base::BindLambdaForTesting([&](Session* sess) {
        callback_called = true;
        create_result = sess;
      }));

  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(call->GetMember(), "Close");
        std::move(callback).Run(nullptr, nullptr);
      });

  auto response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(dbus::ObjectPath(kTestSessionPath));
  std::move(create_session_callback).Run(response.get(), nullptr);

  // Destroy session before the posted create_callback task executes.
  session.reset();

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(callback_called);
}

TEST_F(SessionTest, RequestReleaseDoesNotSelfDelete) {
  auto mock_portal_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop"));

  auto mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
      bus_.get(), kPortalServiceName,
      dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));

  EXPECT_CALL(
      *bus_, GetObjectProxy(kPortalServiceName,
                            testing::Property(&dbus::ObjectPath::value,
                                              testing::HasSubstr("/request/"))))
      .WillRepeatedly(Return(mock_request_proxy.get()));

  dbus::ObjectProxy::ResponseOrErrorCallback method_callback;
  EXPECT_CALL(*mock_portal_proxy, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        method_callback = std::move(callback);
      });

  bool callback_called = false;
  auto request = std::make_unique<Request>(
      bus_, mock_portal_proxy.get(), "org.freedesktop.portal.Location",
      "CreateSession", Dictionary(),
      base::BindLambdaForTesting(
          [&](Results results) { callback_called = true; }));

  // Calling Release() suppresses sending Close on dtor, but does not
  // self-delete.
  request->Release();

  // Fail method call to trigger Finish().
  std::move(method_callback).Run(nullptr, nullptr);

  EXPECT_TRUE(callback_called);
  // Verify request unique_ptr is still valid and can be reset without
  // double-free.
  EXPECT_TRUE(request);
  request.reset();
}

}  // namespace dbus_xdg
