// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/wayland_display_observer.h"

#include <aura-shell-server-protocol.h>
#include <sys/socket.h>
#include <wayland-server-protocol-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wayland/zaura_output_manager.h"
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
    aura_output_manager_resource_ =
        wl_resource_create(client_, &zaura_output_manager_interface,
                           kZAuraOutputManagerVersion, 0);
    SetImplementation(
        aura_output_manager_resource_, nullptr,
        std::make_unique<AuraOutputManager>(aura_output_manager_resource_));
    wl_output_resource_ =
        wl_resource_create(client_, &wl_output_interface, 2, 0);
    xdg_output_resource_ =
        wl_resource_create(client_, &zxdg_output_v1_interface, 2, 0);
    output_ = std::make_unique<WaylandDisplayOutput>(GetPrimaryDisplay());
    handler_ = std::make_unique<::testing::NiceMock<MockWaylandDisplayHandler>>(
        output_.get(), wl_output_resource_);
    handler_->OnXdgOutputCreated(xdg_output_resource_);
    handler_->Initialize();
  }

  void TearDown() override {
    handler_->UnsetXdgOutputResource();
    // Reset `handler_` before `wl_output_resource_` is destroyed.
    handler_.reset();

    // If client has not yet been destroyed clean it up here.
    if (client_) {
      DestroyClient();
    }

    wl_display_destroy(wayland_display_);
    close(fds_[1]);
    output_.reset();

    test::ExoTestBase::TearDown();
  }

  // Destroys the client and all of its associated resources.
  void DestroyClient() {
    if (client_) {
      wl_client_destroy(client_);
      client_ = nullptr;
      aura_output_manager_resource_ = nullptr;
      xdg_output_resource_ = nullptr;
      wl_output_resource_ = nullptr;
    }
  }

  int fds_[2] = {0, 0};
  raw_ptr<wl_display, DanglingUntriaged> wayland_display_ = nullptr;
  raw_ptr<wl_client, DanglingUntriaged> client_ = nullptr;
  raw_ptr<wl_resource, DanglingUntriaged> aura_output_manager_resource_ =
      nullptr;
  raw_ptr<wl_resource, DanglingUntriaged> wl_output_resource_ = nullptr;
  raw_ptr<wl_resource, DanglingUntriaged> xdg_output_resource_ = nullptr;
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
  handler_->SendDisplayMetricsChanges(display, kAllChanges);
}

}  // namespace
}  // namespace wayland
}  // namespace exo
