// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <surface-augmenter-client-protocol.h>

#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {
namespace {

class ClientData : public test::TestClient::CustomData {
 public:
  // Don't leak augmented (sub)surface objects.
  ~ClientData() override {
    if (augmented_surface_)
      augmented_surface_destroy(augmented_surface_);
    if (augmented_sub_surface_)
      augmented_sub_surface_destroy(augmented_sub_surface_);
  }
  augmented_surface* augmented_surface_ = nullptr;
  augmented_sub_surface* augmented_sub_surface_ = nullptr;
};

using SurfaceAugmenterTest = test::WaylandServerTest;

TEST_F(SurfaceAugmenterTest, AugmentedSubSurfacesDontSendLeaveEnter) {
  // Create a surface.
  std::unique_ptr<wl_surface> parent_wl_surface;
  test::ResourceKey parent_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    parent_wl_surface.reset(wl_compositor_create_surface(client->compositor()));
    parent_surface_key =
        test::client_util::GetResourceKey(parent_wl_surface.get());
  });
  Surface* parent_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), parent_surface_key);
  // Check that the surface sends enter/leave events.
  ASSERT_TRUE(parent_surface);
  EXPECT_TRUE(parent_surface->HasLeaveEnterCallbackForTesting());
  // Augment the surface and check that it still sends enter/leave events.
  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();
    data->augmented_surface_ = surface_augmenter_get_augmented_surface(
        client->surface_augmenter(), parent_wl_surface.get());
    client->set_data(std::move(data));
  });
  EXPECT_TRUE(parent_surface->HasLeaveEnterCallbackForTesting());

  // Create another surface.
  std::unique_ptr<wl_surface> child_wl_surface;
  test::ResourceKey child_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    child_wl_surface.reset(wl_compositor_create_surface(client->compositor()));
    child_surface_key =
        test::client_util::GetResourceKey(child_wl_surface.get());
  });
  Surface* child_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), child_surface_key);
  // Check that the surface sends enter/leave events.
  ASSERT_TRUE(child_surface);
  EXPECT_TRUE(child_surface->HasLeaveEnterCallbackForTesting());

  // Make it a subsurface of the first one.
  std::unique_ptr<wl_subsurface> child_wl_subsurface;
  test::ResourceKey child_subsurface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    child_wl_subsurface.reset(wl_subcompositor_get_subsurface(
        client->subcompositor(), child_wl_surface.get(),
        parent_wl_surface.get()));
    child_subsurface_key =
        test::client_util::GetResourceKey(child_wl_subsurface.get());
  });
  SubSurface* child_subsurface =
      test::server_util::GetUserDataForResource<SubSurface>(
          server_.get(), child_subsurface_key);
  // Check that the surface sends enter/leave events.
  ASSERT_TRUE(child_subsurface);
  EXPECT_TRUE(child_surface->HasLeaveEnterCallbackForTesting());
  EXPECT_TRUE(child_subsurface->surface()->HasLeaveEnterCallbackForTesting());

  // Augment the subsurface and check that it doesn't send the events anymore.
  PostToClientAndWait([&](test::TestClient* client) {
    ClientData* data = client->GetDataAs<ClientData>();
    data->augmented_sub_surface_ = surface_augmenter_get_augmented_subsurface(
        client->surface_augmenter(), child_wl_subsurface.get());
  });
  EXPECT_FALSE(child_surface->HasLeaveEnterCallbackForTesting());
  EXPECT_FALSE(child_subsurface->surface()->HasLeaveEnterCallbackForTesting());
}

}  // namespace
}  // namespace exo::wayland
