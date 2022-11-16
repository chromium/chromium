// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_observer.h"

#include <sys/socket.h>
#include <wayland-server-protocol-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include "components/exo/test/exo_test_base.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace {

class MockWaylandDisplayHandler : public WaylandDisplayHandler {
 public:
  using WaylandDisplayHandler::WaylandDisplayHandler;
  MockWaylandDisplayHandler(const MockWaylandDisplayHandler&) = delete;
  MockWaylandDisplayHandler& operator=(const MockWaylandDisplayHandler&) =
      delete;
  ~MockWaylandDisplayHandler() override = default;

  MOCK_METHOD(void,
              XdgOutputSendLogicalPosition,
              (const gfx::Point&),
              (override));
  MOCK_METHOD(void, XdgOutputSendLogicalSize, (const gfx::Size&), (override));
};

class WaylandDisplayObserverTest : public test::ExoTestBase {
 protected:
  static constexpr uint32_t kAllChanges = 0xFFFFFFFF;

  void SetUp() override {
    test::ExoTestBase::SetUp();

    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds_), 0);
    wayland_display_ = wl_display_create();
    client_ = wl_client_create(wayland_display_, fds_[0]);
    wl_output_resource_ =
        wl_resource_create(client_, &wl_output_interface, 2, 0);
    xdg_output_resource_ =
        wl_resource_create(client_, &zxdg_output_v1_interface, 2, 0);
    output_ = std::make_unique<WaylandDisplayOutput>(GetPrimaryDisplay().id());
    handler_ = std::make_unique<::testing::NiceMock<MockWaylandDisplayHandler>>(
        output_.get(), wl_output_resource_);
    handler_->OnXdgOutputCreated(xdg_output_resource_);
    handler_->Initialize();
  }

  void TearDown() override {
    handler_->UnsetXdgOutputResource();
    wl_resource_destroy(xdg_output_resource_);
    wl_resource_destroy(wl_output_resource_);
    wl_client_destroy(client_);
    wl_display_destroy(wayland_display_);
    close(fds_[1]);
    handler_.reset();
    output_.reset();

    test::ExoTestBase::TearDown();
  }

  int fds_[2] = {0, 0};
  wl_display* wayland_display_ = nullptr;
  wl_client* client_ = nullptr;
  wl_resource* wl_output_resource_ = nullptr;
  wl_resource* xdg_output_resource_ = nullptr;
  std::unique_ptr<WaylandDisplayOutput> output_;
  std::unique_ptr<MockWaylandDisplayHandler> handler_;
};

TEST_F(WaylandDisplayObserverTest, SendLogicalPositionAndSize) {
  constexpr gfx::Point kExpectedOrigin(10, 20);
  constexpr gfx::Size kExpectedSize(800, 600);
  constexpr gfx::Rect kExpectedBounds(kExpectedOrigin, kExpectedSize);
  display::Display display(GetPrimaryDisplay().id(), kExpectedBounds);
  display.set_device_scale_factor(2);
  display.set_rotation(display::Display::ROTATE_180);
  display.set_panel_rotation(display::Display::ROTATE_270);

  EXPECT_CALL(*handler_, XdgOutputSendLogicalPosition(kExpectedOrigin))
      .Times(1);
  EXPECT_CALL(*handler_, XdgOutputSendLogicalSize(kExpectedSize)).Times(1);
  handler_->OnDisplayMetricsChanged(display, kAllChanges);
}

}  // namespace
}  // namespace wayland
}  // namespace exo
