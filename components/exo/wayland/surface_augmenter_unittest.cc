// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <surface-augmenter-client-protocol.h>

#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "surface-augmenter-client-protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {
namespace {

using SurfaceAugmenterTest = test::WaylandServerTest;

TEST_F(SurfaceAugmenterTest, AugmentedSubSurfacesDontSendLeaveEnter) {
  std::unique_ptr<wl_surface> parent_wl_surface;
  test::ResourceKey parent_surface_key;

  // Create a surface.
  PostToClientAndWait([&](test::TestClient* client) {
    parent_wl_surface.reset(wl_compositor_create_surface(client->compositor()));
    parent_surface_key =
        test::client_util::GetResourceKey(parent_wl_surface.get());
  });
  Surface* parent_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), parent_surface_key);
  ASSERT_TRUE(parent_surface);
  // Check that the surface sends enter/leave events.
  EXPECT_TRUE(parent_surface->HasLeaveEnterCallbackForTesting());
  // Augment the surface.
  PostToClientAndWait([&](test::TestClient* client) {
    surface_augmenter_get_augmented_surface(client->surface_augmenter(),
                                            parent_wl_surface.get());
  });
  // Check that the surface still sends enter/leave events.
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
  ASSERT_TRUE(child_surface);
  // Check that the surface sends enter/leave events.
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
  ASSERT_TRUE(child_subsurface);
  // Check that the surface sends enter/leave events.
  EXPECT_TRUE(child_surface->HasLeaveEnterCallbackForTesting());
  EXPECT_TRUE(child_subsurface->surface()->HasLeaveEnterCallbackForTesting());

  // Augment the subsurface.
  PostToClientAndWait([&](test::TestClient* client) {
    surface_augmenter_get_augmented_subsurface(client->surface_augmenter(),
                                               child_wl_subsurface.get());
  });
  // Check that the surface doesn't send enter/leave events anymore.
  EXPECT_FALSE(child_surface->HasLeaveEnterCallbackForTesting());
  EXPECT_FALSE(child_subsurface->surface()->HasLeaveEnterCallbackForTesting());
}

}  // namespace
}  // namespace exo::wayland
