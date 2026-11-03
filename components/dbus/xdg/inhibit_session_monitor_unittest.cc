// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/inhibit_session_monitor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/nix/xdg_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kInhibitInterfaceName[] = "org.freedesktop.portal.Inhibit";
constexpr char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
constexpr char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
constexpr char kOtherSessionPath[] =
    "/org/freedesktop/portal/desktop/session/other/token";

class InhibitSessionMonitorTest : public testing::Test {
 public:
  void SetUp() override {
    SetPortalStateForTesting(PortalRegistrarState::kSuccess);
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
    request_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName,
        dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));
    session_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName,
        dbus::ObjectPath("/org/freedesktop/portal/desktop/session/1"));

    EXPECT_CALL(*bus_, GetConnectionName()).WillRepeatedly(Return("test"));
    EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                      dbus::ObjectPath(kPortalObjectPath)))
        .WillRepeatedly(Return(portal_proxy_.get()));
    EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                      Property(&dbus::ObjectPath::value,
                                               HasSubstr("/request/"))))
        .WillRepeatedly(Return(request_proxy_.get()));
    EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                      Property(&dbus::ObjectPath::value,
                                               HasSubstr("/session/"))))
        .WillRepeatedly(Return(session_proxy_.get()));

    EXPECT_CALL(*request_proxy_,
                ConnectToSignal(kRequestInterfaceName, "Response", _, _))
        .WillRepeatedly(
            [this](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback connected_callback) {
              response_signal_callback_ = std::move(signal_callback);
              std::move(connected_callback)
                  .Run(interface_name, signal_name, true);
            });

    // The portal proxy receives CreateMonitor and QueryEndResponse calls.
    EXPECT_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly([this](dbus::MethodCall* call, int timeout_ms,
                               dbus::ObjectProxy::ResponseOrErrorCallback cb) {
          EXPECT_EQ(call->GetInterface(), kInhibitInterfaceName);
          if (call->GetMember() == "CreateMonitor") {
            HandleCreateMonitor(call, std::move(cb));
          } else if (call->GetMember() == "QueryEndResponse") {
            dbus::MessageReader reader(call);
            dbus::ObjectPath path;
            EXPECT_TRUE(reader.PopObjectPath(&path));
            query_end_responses_.push_back(path);
            auto response = dbus::Response::CreateEmpty();
            std::move(cb).Run(response.get(), nullptr);
          } else {
            ADD_FAILURE() << "Unexpected method " << call->GetMember();
          }
        });
  }

  void TearDown() override {
    SetPortalStateForTesting(PortalRegistrarState::kIdle);
  }

 protected:
  void HandleCreateMonitor(dbus::MethodCall* call,
                           dbus::ObjectProxy::ResponseOrErrorCallback cb) {
    dbus::MessageReader reader(call);
    std::string window;
    EXPECT_TRUE(reader.PopString(&window));
    EXPECT_EQ(window, "");
    auto options = dbus_utils::ReadValue<Dictionary>(reader);
    ASSERT_TRUE(options);

    auto session_token_it = options->find("session_handle_token");
    ASSERT_NE(session_token_it, options->end());
    auto session_token =
        std::move(session_token_it->second).Take<std::string>();
    ASSERT_TRUE(session_token);
    session_path_ = dbus::ObjectPath(
        base::nix::XdgDesktopPortalSessionPath("test", *session_token));

    auto handle_token_it = options->find("handle_token");
    ASSERT_NE(handle_token_it, options->end());
    auto handle_token = std::move(handle_token_it->second).Take<std::string>();
    ASSERT_TRUE(handle_token);
    auto response = dbus::Response::CreateEmpty();
    dbus::MessageWriter writer(response.get());
    writer.AppendObjectPath(dbus::ObjectPath(
        base::nix::XdgDesktopPortalRequestPath("test", *handle_token)));
    std::move(cb).Run(response.get(), nullptr);
  }

  void ExpectStateChangedConnection(bool success) {
    EXPECT_CALL(*portal_proxy_,
                ConnectToSignal(kInhibitInterfaceName, "StateChanged", _, _))
        .WillOnce(
            [this, success](
                const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback connected_callback) {
              state_changed_callback_ = std::move(signal_callback);
              std::move(connected_callback)
                  .Run(interface_name, signal_name, success);
            });
  }

  // Sends the CreateMonitor request's Response signal with `session_handle`,
  // or with the session path derived from the session handle token if unset.
  void SendCreateMonitorResponse(
      std::optional<dbus::ObjectPath> session_handle = std::nullopt) {
    ASSERT_TRUE(response_signal_callback_);
    dbus::Signal signal(kRequestInterfaceName, "Response");
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(0);
    Dictionary results;
    results["session_handle"] = dbus_utils::Variant::Wrap<"s">(
        session_handle.value_or(session_path_).value());
    dbus_utils::WriteValue(writer, results);
    response_signal_callback_.Run(&signal);
  }

  void EmitStateChanged(const dbus::ObjectPath& session_handle,
                        InhibitSessionMonitor::SessionState state) {
    ASSERT_TRUE(state_changed_callback_);
    dbus::Signal signal(kInhibitInterfaceName, "StateChanged");
    dbus::MessageWriter writer(&signal);
    writer.AppendObjectPath(session_handle);
    Dictionary dict;
    dict["screensaver-active"] = dbus_utils::Variant::Wrap<"b">(false);
    dict["session-state"] =
        dbus_utils::Variant::Wrap<"u">(static_cast<uint32_t>(state));
    dbus_utils::WriteValue(writer, dict);
    state_changed_callback_.Run(&signal);
  }

  // Creates a monitor and waits until its session is created.
  std::unique_ptr<InhibitSessionMonitor> CreateMonitor(
      base::OnceClosure session_ending_callback) {
    ExpectStateChangedConnection(true);
    auto monitor = std::make_unique<InhibitSessionMonitor>(
        bus_, std::move(session_ending_callback));
    SendCreateMonitorResponse();
    EXPECT_TRUE(monitor->has_session_for_testing());
    return monitor;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;
  scoped_refptr<dbus::MockObjectProxy> request_proxy_;
  scoped_refptr<dbus::MockObjectProxy> session_proxy_;
  dbus::ObjectProxy::SignalCallback response_signal_callback_;
  dbus::ObjectProxy::SignalCallback state_changed_callback_;
  dbus::ObjectPath session_path_;
  std::vector<dbus::ObjectPath> query_end_responses_;
};

}  // namespace

TEST_F(InhibitSessionMonitorTest, PortalUnavailable) {
  SetPortalStateForTesting(PortalRegistrarState::kFailed);
  EXPECT_CALL(*portal_proxy_, ConnectToSignal(_, _, _, _)).Times(0);

  InhibitSessionMonitor monitor(bus_, base::DoNothing());
  EXPECT_FALSE(monitor.has_session_for_testing());
}

TEST_F(InhibitSessionMonitorTest, InhibitVersionTooOld) {
  SetPortalInterfaceVersionForTesting(kInhibitInterfaceName, 2);
  EXPECT_CALL(*portal_proxy_, ConnectToSignal(_, _, _, _)).Times(0);

  InhibitSessionMonitor monitor(bus_, base::DoNothing());
  EXPECT_FALSE(monitor.has_session_for_testing());
}

TEST_F(InhibitSessionMonitorTest, StateChangedConnectionFailed) {
  ExpectStateChangedConnection(false);

  InhibitSessionMonitor monitor(bus_, base::DoNothing());
  EXPECT_FALSE(response_signal_callback_);
  EXPECT_FALSE(monitor.has_session_for_testing());
}

TEST_F(InhibitSessionMonitorTest, CreateMonitorFailed) {
  ExpectStateChangedConnection(true);
  InhibitSessionMonitor monitor(bus_, base::DoNothing());

  dbus::Signal signal(kRequestInterfaceName, "Response");
  dbus::MessageWriter writer(&signal);
  writer.AppendUint32(2);
  dbus_utils::WriteValue(writer, Dictionary());
  response_signal_callback_.Run(&signal);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(monitor.has_session_for_testing());
  // State changes are ignored without a monitor session.
  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kQueryEnd);
  EXPECT_TRUE(query_end_responses_.empty());
}

TEST_F(InhibitSessionMonitorTest, QueryEndThenEnding) {
  base::test::TestFuture<void> session_ending;
  auto monitor = CreateMonitor(session_ending.GetCallback());

  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kRunning);
  EXPECT_TRUE(query_end_responses_.empty());

  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kQueryEnd);
  ASSERT_EQ(query_end_responses_.size(), 1u);
  EXPECT_EQ(query_end_responses_[0], session_path_);
  EXPECT_FALSE(session_ending.IsReady());

  EmitStateChanged(session_path_, InhibitSessionMonitor::SessionState::kEnding);
  EXPECT_TRUE(session_ending.IsReady());
}

// Logout can be cancelled after "Query End", e.g. when another application
// inhibits it, so the session ending callback must not run.
TEST_F(InhibitSessionMonitorTest, QueryEndThenCancelled) {
  base::test::TestFuture<void> session_ending;
  auto monitor = CreateMonitor(session_ending.GetCallback());

  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kQueryEnd);
  EXPECT_EQ(query_end_responses_.size(), 1u);
  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kRunning);
  EXPECT_FALSE(session_ending.IsReady());

  // A later logout is acknowledged again.
  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kQueryEnd);
  EXPECT_EQ(query_end_responses_.size(), 2u);
  EXPECT_FALSE(session_ending.IsReady());
}

// The portal may report state changes before the CreateMonitor request's
// Response signal.
TEST_F(InhibitSessionMonitorTest, StateChangedBeforeResponse) {
  base::test::TestFuture<void> session_ending;
  ExpectStateChangedConnection(true);
  InhibitSessionMonitor monitor(bus_, session_ending.GetCallback());
  ASSERT_TRUE(session_path_.IsValid());

  EmitStateChanged(session_path_,
                   InhibitSessionMonitor::SessionState::kQueryEnd);
  ASSERT_EQ(query_end_responses_.size(), 1u);
  EXPECT_EQ(query_end_responses_[0], session_path_);
  EmitStateChanged(session_path_, InhibitSessionMonitor::SessionState::kEnding);
  EXPECT_TRUE(session_ending.IsReady());
}

// The session handle in the CreateMonitor response takes precedence over the
// path derived from the session handle token.
TEST_F(InhibitSessionMonitorTest, UsesSessionHandleFromResponse) {
  base::test::TestFuture<void> session_ending;
  ExpectStateChangedConnection(true);
  InhibitSessionMonitor monitor(bus_, session_ending.GetCallback());
  const dbus::ObjectPath token_path = session_path_;
  const dbus::ObjectPath response_path(kOtherSessionPath);
  ASSERT_NE(token_path, response_path);
  SendCreateMonitorResponse(response_path);
  // Session runs its creation callback asynchronously.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(monitor.has_session_for_testing());

  EmitStateChanged(token_path, InhibitSessionMonitor::SessionState::kQueryEnd);
  EXPECT_TRUE(query_end_responses_.empty());

  EmitStateChanged(response_path,
                   InhibitSessionMonitor::SessionState::kQueryEnd);
  ASSERT_EQ(query_end_responses_.size(), 1u);
  EXPECT_EQ(query_end_responses_[0], response_path);
  EmitStateChanged(response_path, InhibitSessionMonitor::SessionState::kEnding);
  EXPECT_TRUE(session_ending.IsReady());
}

TEST_F(InhibitSessionMonitorTest, IgnoresOtherSessions) {
  base::test::TestFuture<void> session_ending;
  auto monitor = CreateMonitor(session_ending.GetCallback());

  const dbus::ObjectPath other_path(kOtherSessionPath);
  EmitStateChanged(other_path, InhibitSessionMonitor::SessionState::kQueryEnd);
  EmitStateChanged(other_path, InhibitSessionMonitor::SessionState::kEnding);
  EXPECT_TRUE(query_end_responses_.empty());
  EXPECT_FALSE(session_ending.IsReady());
}

TEST_F(InhibitSessionMonitorTest, DestructorClosesSession) {
  auto monitor = CreateMonitor(base::DoNothing());

  EXPECT_CALL(*session_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([](dbus::MethodCall* call, int timeout_ms,
                   dbus::ObjectProxy::ResponseOrErrorCallback cb) {
        EXPECT_EQ(call->GetInterface(), kSessionInterfaceName);
        EXPECT_EQ(call->GetMember(), "Close");
      });
  monitor.reset();
}

}  // namespace dbus_xdg
