// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <wayland-server-protocol-core.h>
#include "components/exo/display.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {
namespace {

class ClientData : public test::TestClient::CustomData {
 public:
  std::unique_ptr<wl_surface> surface;
  std::unique_ptr<wl_surface> surface2;
};

class WaylandAuraShellServerTest : public test::WaylandServerTest {
 public:
  WaylandAuraShellServerTest() = default;

  WaylandAuraShellServerTest(const WaylandAuraShellServerTest&) = delete;
  WaylandAuraShellServerTest& operator=(const WaylandAuraShellServerTest&) =
      delete;

  ~WaylandAuraShellServerTest() override = default;

  // test::WaylandServerTest:
  void SetUp() override {
    WaylandServerTest::SetUp();
    display_ = server_->GetDisplay();
  }

  void TearDown() override { WaylandServerTest::TearDown(); }

  void OnAuraShellActivated(zaura_shell*,
                            wl_surface* gained_active,
                            wl_surface* lost_active) {
    gained_active_ = gained_active;
    lost_active_ = lost_active;
    activated_call_count_++;
  }

  void SetupClientSurface() {
    PostToClientAndWait([&](test::TestClient* client) {
      zaura_shell_add_listener(client->aura_shell(), &kAuraShellListener, this);
      auto data = std::make_unique<ClientData>();
      data->surface.reset(wl_compositor_create_surface(client->compositor()));

      surface_key_ = test::client_util::GetResourceKey(data->surface.get());
      client->set_data(std::move(data));
    });
  }

  void SetupMultipleClientSurfaces() {
    PostToClientAndWait([&](test::TestClient* client) {
      zaura_shell_add_listener(client->aura_shell(), &kAuraShellListener, this);
      auto data = std::make_unique<ClientData>();
      data->surface.reset(wl_compositor_create_surface(client->compositor()));
      data->surface2.reset(wl_compositor_create_surface(client->compositor()));

      surface_key_ = test::client_util::GetResourceKey(data->surface.get());
      surface_key2_ = test::client_util::GetResourceKey(data->surface2.get());
      client->set_data(std::move(data));
    });
  }

  Surface* GetClientSurface(test::ResourceKey surface_key) {
    return test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                              surface_key);
  }

  const zaura_shell_listener kAuraShellListener = {
      [](void* data, struct zaura_shell* zaura_shell, uint32_t layout_mode) {},
      [](void* data, struct zaura_shell* zaura_shell, uint32_t id) {},
      [](void* data,
         struct zaura_shell* zaura_shell,
         struct wl_array* desk_names) {},
      [](void* data,
         struct zaura_shell* zaura_shell,
         int32_t active_desk_index) {},
      [](void* data,
         struct zaura_shell* zaura_shell,
         struct wl_surface* gained_active,
         struct wl_surface* lost_active) {
        static_cast<WaylandAuraShellServerTest*>(data)->OnAuraShellActivated(
            zaura_shell, gained_active, lost_active);
      }};

  Display* display_;
  wl_surface* gained_active_ = nullptr;
  wl_surface* lost_active_ = nullptr;
  int32_t activated_call_count_ = 0;

  test::ResourceKey surface_key_;
  test::ResourceKey surface_key2_;
};

// Home screen -> any window
TEST_F(WaylandAuraShellServerTest, HasFocusedClientChangedSendActivated) {
  SetupClientSurface();
  Surface* surface = GetClientSurface(surface_key_);
  ASSERT_TRUE(surface);

  display_->seat()->OnWindowFocused(surface->window(), nullptr);
  // Wait until all wayland events are sent.
  PostToClientAndWait([]() {});
  EXPECT_TRUE(gained_active_ != nullptr);
  EXPECT_TRUE(lost_active_ == nullptr);
  EXPECT_EQ(1, activated_call_count_);
}

// Exo client window -> Same exo client another window
TEST_F(WaylandAuraShellServerTest, FocusedClientChangedSendActivated) {
  SetupMultipleClientSurfaces();
  Surface* surface = GetClientSurface(surface_key_);
  ASSERT_TRUE(surface);

  display_->seat()->OnWindowFocused(surface->window(), nullptr);
  // Reset previous gained and lost active info.
  gained_active_ = nullptr;
  lost_active_ = nullptr;

  Surface* surface2 = GetClientSurface(surface_key2_);
  ASSERT_TRUE(surface2);
  display_->seat()->OnWindowFocused(surface2->window(), surface->window());
  // Wait until all wayland events are sent.
  PostToClientAndWait([]() {});

  EXPECT_TRUE(gained_active_ != nullptr);
  EXPECT_TRUE(lost_active_ != nullptr);
  EXPECT_EQ(2, activated_call_count_);
}

// Exo client window -> Chrome window
TEST_F(WaylandAuraShellServerTest, FocusedClientChangedToNonExoSendActivated) {
  SetupMultipleClientSurfaces();
  Surface* surface = GetClientSurface(surface_key_);
  ASSERT_TRUE(surface);
  display_->seat()->OnWindowFocused(surface->window(), nullptr);

  // Reset previous gained and lost active info.
  gained_active_ = nullptr;
  lost_active_ = nullptr;

  Surface* surface2 = GetClientSurface(surface_key2_);
  ASSERT_TRUE(surface2);
  // Chrome surface doesn't have wayland resource.
  SetSurfaceResource(surface2, nullptr);
  display_->seat()->OnWindowFocused(surface2->window(), surface->window());
  // Wait until all wayland events are sent.
  PostToClientAndWait([]() {});

  EXPECT_TRUE(gained_active_ == nullptr);
  EXPECT_TRUE(lost_active_ != nullptr);
  EXPECT_EQ(2, activated_call_count_);
}

// Chrome window -> Chrome window
TEST_F(WaylandAuraShellServerTest,
       NonExoFocusedClientChangedNotSendingActivated) {
  SetupMultipleClientSurfaces();
  Surface* surface = GetClientSurface(surface_key_);
  ASSERT_TRUE(surface);
  // Chrome surface doesn't have wayland resource.
  SetSurfaceResource(surface, nullptr);
  display_->seat()->OnWindowFocused(surface->window(), nullptr);

  // Reset previous gained and lost active info.
  gained_active_ = nullptr;
  lost_active_ = nullptr;

  Surface* surface2 = GetClientSurface(surface_key2_);
  ASSERT_TRUE(surface2);
  // Chrome surface doesn't have wayland resource.
  SetSurfaceResource(surface2, nullptr);
  display_->seat()->OnWindowFocused(surface2->window(), surface->window());
  // Wait until all wayland events are sent.
  PostToClientAndWait([]() {});

  EXPECT_EQ(nullptr, gained_active_);
  EXPECT_EQ(nullptr, lost_active_);
  EXPECT_EQ(1, activated_call_count_);
}

}  // namespace
}  // namespace exo::wayland
