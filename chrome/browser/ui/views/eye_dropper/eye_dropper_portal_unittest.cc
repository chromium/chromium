// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_portal.h"

#include <tuple>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/utils/write_value.h"
#include "components/dbus/xdg/portal.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

constexpr char kBusName[] = ":1.123";
constexpr char kPortalServiceName[] = "org.freedesktop.portal.Desktop";
constexpr char kPortalObjectPath[] = "/org/freedesktop/portal/desktop";
constexpr char kScreenshotInterfaceName[] = "org.freedesktop.portal.Screenshot";
constexpr char kMethodPickColor[] = "PickColor";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";
constexpr char kSignalResponse[] = "Response";

class MockEyeDropperListener : public content::EyeDropperListener {
 public:
  void ColorSelected(SkColor color) override {
    color_selected_ = true;
    selected_color_ = color;
  }
  void ColorSelectionCanceled() override { color_selection_canceled_ = true; }

  bool color_selected() const { return color_selected_; }
  bool color_selection_canceled() const { return color_selection_canceled_; }
  SkColor selected_color() const { return selected_color_; }

 private:
  bool color_selected_ = false;
  bool color_selection_canceled_ = false;
  SkColor selected_color_ = SK_ColorTRANSPARENT;
};

}  // namespace

class EyeDropperPortalTest : public ChromeRenderViewHostTestHarness {
 public:
  EyeDropperPortalTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    dbus_xdg::SetPortalStateForTesting(
        dbus_xdg::PortalRegistrarState::kSuccess);

    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(dbus::Bus::Options());

    EXPECT_CALL(*mock_bus_, AssertOnOriginThread()).WillRepeatedly([] {});
    EXPECT_CALL(*mock_bus_, GetDBusTaskRunner())
        .WillRepeatedly(
            Return(base::SingleThreadTaskRunner::GetCurrentDefault().get()));

    mock_portal_proxy_ = base::MakeRefCounted<dbus::MockObjectProxy>(
        mock_bus_.get(), kPortalServiceName,
        dbus::ObjectPath(kPortalObjectPath));
    EXPECT_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName,
                                           dbus::ObjectPath(kPortalObjectPath)))
        .WillRepeatedly(Return(mock_portal_proxy_.get()));
  }

  void TearDown() override {
    dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kIdle);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_portal_proxy_;
  MockEyeDropperListener listener_;
};

TEST_F(EyeDropperPortalTest, Success) {
  EXPECT_CALL(*mock_bus_, GetConnectionName()).WillRepeatedly(Return(kBusName));

  dbus::ObjectPath request_path;
  scoped_refptr<dbus::MockObjectProxy> mock_request_proxy;
  dbus::ObjectProxy::SignalCallback response_callback;
  bool signal_connected = false;

  // The request object path is generated randomly, so we need to capture it.
  EXPECT_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName, _))
      .WillRepeatedly([&](std::string_view service_name,
                          const dbus::ObjectPath& object_path) {
        if (object_path.value() == kPortalObjectPath) {
          return mock_portal_proxy_.get();
        }
        request_path = object_path;
        mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(), kPortalServiceName, request_path);
        EXPECT_CALL(*mock_request_proxy,
                    ConnectToSignal(kRequestInterface, kSignalResponse, _, _))
            .WillOnce([&](const std::string& interface_name,
                          const std::string& signal_name,
                          dbus::ObjectProxy::SignalCallback signal_callback,
                          dbus::ObjectProxy::OnConnectedCallback
                              on_connected_callback) {
              response_callback = std::move(signal_callback);
              std::move(on_connected_callback)
                  .Run(interface_name, signal_name, true);
              signal_connected = true;
            });
        return mock_request_proxy.get();
      });

  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        EXPECT_EQ(method_call->GetInterface(), kScreenshotInterfaceName);
        EXPECT_EQ(method_call->GetMember(), kMethodPickColor);

        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(request_path);
        std::move(callback).Run(response.get(), nullptr);
      });

  auto eye_dropper =
      EyeDropperPortal::CreateForTesting(main_rfh(), &listener_, mock_bus_);

  ASSERT_TRUE(base::test::RunUntil([&]() { return signal_connected; }));
  ASSERT_FALSE(response_callback.is_null());

  // Simulate success response.
  dbus::Signal signal(kRequestInterface, kSignalResponse);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint32(0);  // Success

  std::map<std::string, dbus_utils::Variant> results;
  results["color"] =
      dbus_utils::Variant::Wrap<"(ddd)">(std::make_tuple(1.0, 0.5, 0.0));
  dbus_utils::WriteValue(writer, results);

  response_callback.Run(&signal);
  EXPECT_TRUE(listener_.color_selected());
  EXPECT_EQ(listener_.selected_color(), SkColorSetRGB(255, 128, 0));
}

TEST_F(EyeDropperPortalTest, Cancelled) {
  EXPECT_CALL(*mock_bus_, GetConnectionName()).WillRepeatedly(Return(kBusName));

  dbus::ObjectPath request_path;
  scoped_refptr<dbus::MockObjectProxy> mock_request_proxy;
  dbus::ObjectProxy::SignalCallback response_callback;
  bool signal_connected = false;

  EXPECT_CALL(*mock_bus_, GetObjectProxy(kPortalServiceName, _))
      .WillRepeatedly([&](std::string_view service_name,
                          const dbus::ObjectPath& object_path) {
        if (object_path.value() == kPortalObjectPath) {
          return mock_portal_proxy_.get();
        }
        request_path = object_path;
        mock_request_proxy = base::MakeRefCounted<dbus::MockObjectProxy>(
            mock_bus_.get(), kPortalServiceName, request_path);
        EXPECT_CALL(*mock_request_proxy,
                    ConnectToSignal(kRequestInterface, kSignalResponse, _, _))
            .WillOnce([&](const std::string& interface_name,
                          const std::string& signal_name,
                          dbus::ObjectProxy::SignalCallback signal_callback,
                          dbus::ObjectProxy::OnConnectedCallback
                              on_connected_callback) {
              response_callback = std::move(signal_callback);
              std::move(on_connected_callback)
                  .Run(interface_name, signal_name, true);
              signal_connected = true;
            });
        return mock_request_proxy.get();
      });

  EXPECT_CALL(*mock_portal_proxy_, CallMethodWithErrorResponse(_, _, _))
      .WillOnce([&](dbus::MethodCall* method_call, int timeout_ms,
                    dbus::ObjectProxy::ResponseOrErrorCallback callback) {
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        writer.AppendObjectPath(request_path);
        std::move(callback).Run(response.get(), nullptr);
      });

  auto eye_dropper =
      EyeDropperPortal::CreateForTesting(main_rfh(), &listener_, mock_bus_);

  ASSERT_TRUE(base::test::RunUntil([&]() { return signal_connected; }));
  ASSERT_FALSE(response_callback.is_null());

  // Simulate user cancel.
  dbus::Signal signal(kRequestInterface, kSignalResponse);
  dbus::MessageWriter writer(&signal);
  writer.AppendUint32(1);  // Cancelled

  response_callback.Run(&signal);
  EXPECT_TRUE(listener_.color_selection_canceled());
}

TEST_F(EyeDropperPortalTest, PortalNotAvailable) {
  dbus_xdg::SetPortalStateForTesting(dbus_xdg::PortalRegistrarState::kFailed);

  auto eye_dropper =
      EyeDropperPortal::CreateForTesting(main_rfh(), &listener_, mock_bus_);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return listener_.color_selection_canceled(); }));
}
