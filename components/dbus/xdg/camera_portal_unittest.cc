// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dbus/xdg/camera_portal.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/scoped_environment_variable_override.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/version_info/nix/version_extra_utils.h"
#include "components/dbus/utils/read_value.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace dbus_xdg {

namespace {

constexpr char kConnectionName[] = ":1.42";
constexpr char kCameraInterface[] = "org.freedesktop.portal.Camera";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";

enum class AccessCameraReply {
  kSuccess,
  kUserCancelled,
  kOther,
};

// The desktop file name without ".desktop", e.g. "chromium-test" or
// "google-chrome-beta" in branded builds.
std::string ExpectedAppId() {
  auto env = base::Environment::Create();
  std::string name = version_info::nix::GetDesktopName(*env);
  EXPECT_TRUE(name.ends_with(".desktop"));
  return name.substr(0, name.size() - std::string_view(".desktop").size());
}

class CameraPortalTest : public testing::Test {
 public:
  void SetUp() override {
    bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());
    portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName, dbus::ObjectPath(kPortalObjectPath));
    request_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        bus_.get(), kPortalServiceName,
        dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1"));

    ON_CALL(*bus_, GetConnectionName()).WillByDefault(Return(kConnectionName));
    ON_CALL(*bus_, GetDBusTaskRunner())
        .WillByDefault(
            Return(base::SequencedTaskRunner::GetCurrentDefault().get()));
    EXPECT_CALL(*bus_, GetObjectProxy(kPortalServiceName,
                                      dbus::ObjectPath(kPortalObjectPath)))
        .WillRepeatedly(Return(portal_proxy_.get()));
    EXPECT_CALL(*bus_, GetObjectProxy(
                           kPortalServiceName,
                           testing::Property(&dbus::ObjectPath::value,
                                             testing::HasSubstr("/request/"))))
        .WillRepeatedly(Return(request_proxy_.get()));
    EXPECT_CALL(*request_proxy_,
                ConnectToSignal(kRequestInterface, "Response", _, _))
        .WillRepeatedly(
            [this](const std::string& interface_name,
                   const std::string& signal_name,
                   dbus::ObjectProxy::SignalCallback signal_callback,
                   dbus::ObjectProxy::OnConnectedCallback connected_callback) {
              response_signal_callback_ = signal_callback;
              std::move(connected_callback)
                  .Run(interface_name, signal_name, true);
            });
    // The bus is shut down when the portal is destroyed.
    EXPECT_CALL(*bus_, ShutdownAndBlock()).WillOnce([this] {
      bus_shut_down_ = true;
    });
  }

 protected:
  // Handles Register, AccessCamera and OpenPipeWireRemote on the portal
  // object, and records the order of the calls in `calls_`.
  void ExpectPortalCalls(bool register_succeeds,
                         std::optional<AccessCameraReply> access_reply) {
    EXPECT_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillRepeatedly(
            [this, register_succeeds, access_reply](
                dbus::MethodCall* call, int timeout_ms,
                dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              calls_.push_back(call->GetMember());
              dbus::MessageReader reader(call);
              if (call->GetMember() == kMethodRegister) {
                EXPECT_EQ(call->GetInterface(), kRegistryInterface);
                std::string app_id;
                EXPECT_TRUE(reader.PopString(&app_id));
                EXPECT_EQ(app_id, ExpectedAppId());
                if (!register_succeeds) {
                  call->SetSerial(1);
                  auto error = dbus::ErrorResponse::FromMethodCall(
                      call, "org.freedesktop.portal.Error.Failed", "failed");
                  std::move(callback).Run(nullptr, error.get());
                  return;
                }
                auto response = dbus::Response::CreateEmpty();
                std::move(callback).Run(response.get(), nullptr);
                return;
              }

              EXPECT_EQ(call->GetInterface(), kCameraInterface);
              if (call->GetMember() == "AccessCamera") {
                auto options = dbus_utils::ReadValue<Dictionary>(reader);
                ASSERT_TRUE(options);
                auto token =
                    std::move(options->at("handle_token")).Take<std::string>();
                ASSERT_TRUE(token);
                auto response = dbus::Response::CreateEmpty();
                dbus::MessageWriter writer(response.get());
                writer.AppendObjectPath(
                    dbus::ObjectPath(base::nix::XdgDesktopPortalRequestPath(
                        kConnectionName, *token)));
                std::move(callback).Run(response.get(), nullptr);
                SendAccessCameraResponse(*access_reply);
                return;
              }

              EXPECT_EQ(call->GetMember(), "OpenPipeWireRemote");
              int fds[2];
              ASSERT_EQ(pipe(fds), 0);
              base::ScopedFD read_end(fds[0]);
              base::ScopedFD write_end(fds[1]);
              auto response = dbus::Response::CreateEmpty();
              dbus::MessageWriter writer(response.get());
              writer.AppendFileDescriptor(read_end.get());
              std::move(callback).Run(response.get(), nullptr);
            });
  }

  void SendAccessCameraResponse(AccessCameraReply reply) {
    dbus::Signal signal(kRequestInterface, "Response");
    dbus::MessageWriter writer(&signal);
    switch (reply) {
      case AccessCameraReply::kSuccess:
        writer.AppendUint32(0);
        break;
      case AccessCameraReply::kUserCancelled:
        writer.AppendUint32(1);
        break;
      case AccessCameraReply::kOther:
        writer.AppendUint32(2);
        break;
    }
    dbus_utils::WriteValue(writer, Dictionary());
    response_signal_callback_.Run(&signal);
  }

  CameraPortalResult Open() {
    base::RunLoop run_loop;
    std::optional<CameraPortalResult> result;
    {
      CameraPortal portal(bus_);
      portal.OpenPipeWireRemote(
          base::BindLambdaForTesting([&](CameraPortalResult r) {
            result = std::move(r);
            run_loop.Quit();
          }));
      run_loop.Run();
    }
    // Wait for the posted bus shutdown.
    EXPECT_TRUE(base::test::RunUntil([this] { return bus_shut_down_; }));
    return std::move(*result);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedEnvironmentVariableOverride desktop_override_{
      "CHROME_DESKTOP", "chromium-test.desktop"};
  base::ScopedEnvironmentVariableOverride channel_override_{
      version_info::nix::kChromeVersionExtra, "beta"};
  base::ScopedEnvironmentVariableOverride snap_override_{"SNAP_NAME"};
  base::nix::ScopedAppSandboxOverrideForTesting sandbox_override_{
      base::nix::AppSandbox::kNone};
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;
  scoped_refptr<dbus::MockObjectProxy> request_proxy_;
  dbus::ObjectProxy::SignalCallback response_signal_callback_;
  std::vector<std::string> calls_;
  bool bus_shut_down_ = false;
};

TEST_F(CameraPortalTest, RegistersDesktopNameBeforeAccessCamera) {
  ExpectPortalCalls(/*register_succeeds=*/true, AccessCameraReply::kSuccess);

  CameraPortalResult result = Open();

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_valid());
  EXPECT_THAT(calls_, testing::ElementsAre(kMethodRegister, "AccessCamera",
                                           "OpenPipeWireRemote"));
}

TEST_F(CameraPortalTest, RegisterFailure) {
  ExpectPortalCalls(/*register_succeeds=*/false, std::nullopt);

  CameraPortalResult result = Open();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), CameraPortalError::kFailed);
  EXPECT_THAT(calls_, testing::ElementsAre(kMethodRegister));
}

TEST_F(CameraPortalTest, UserCancelled) {
  ExpectPortalCalls(/*register_succeeds=*/true,
                    AccessCameraReply::kUserCancelled);

  CameraPortalResult result = Open();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), CameraPortalError::kDenied);
  EXPECT_THAT(calls_, testing::ElementsAre(kMethodRegister, "AccessCamera"));
}

TEST_F(CameraPortalTest, AccessCameraFailure) {
  ExpectPortalCalls(/*register_succeeds=*/true, AccessCameraReply::kOther);

  CameraPortalResult result = Open();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), CameraPortalError::kFailed);
  EXPECT_THAT(calls_, testing::ElementsAre(kMethodRegister, "AccessCamera"));
}

TEST_F(CameraPortalTest, NotUsedInFlatpak) {
  base::nix::ScopedAppSandboxOverrideForTesting flatpak(
      base::nix::AppSandbox::kFlatpak);
  EXPECT_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _)).Times(0);

  CameraPortalResult result = Open();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), CameraPortalError::kFailed);
}

}  // namespace

}  // namespace dbus_xdg
