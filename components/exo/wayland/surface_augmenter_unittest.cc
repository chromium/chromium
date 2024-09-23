// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <surface-augmenter-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include "build/build_config.h"

#include "base/memory/raw_ptr.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/compositor/layer.h"

namespace exo::wayland {
namespace {

class ClientData : public test::TestClient::CustomData {
 public:
  // Don't leak augmented (sub)surface objects.
  ~ClientData() override {
    if (augmented_surface) {
      augmented_surface_destroy(augmented_surface);
    }
    if (augmented_sub_surface) {
      augmented_sub_surface_destroy(augmented_sub_surface);
    }
  }

  std::unique_ptr<wl_surface> parent_wl_surface;

  std::unique_ptr<wl_surface> child_wl_surface;
  std::unique_ptr<wl_subsurface> child_wl_subsurface;
  std::unique_ptr<wl_surface> child2_wl_surface;
  std::unique_ptr<wl_subsurface> child2_wl_subsurface;

  raw_ptr<augmented_surface, DanglingUntriaged> augmented_surface = nullptr;
  raw_ptr<augmented_sub_surface, DanglingUntriaged> augmented_sub_surface =
      nullptr;
};

using SurfaceAugmenterTest = test::WaylandServerTest;

TEST_F(SurfaceAugmenterTest, AugmentedSubSurfacesDontSendLeaveEnter) {
  // Create a surface.
  test::ResourceKey parent_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();
    data->parent_wl_surface.reset(
        wl_compositor_create_surface(client->compositor()));
    parent_surface_key =
        test::client_util::GetResourceKey(data->parent_wl_surface.get());
    client->set_data(std::move(data));
  });
  Surface* parent_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), parent_surface_key);
  // Check that the surface sends enter/leave events.
  ASSERT_TRUE(parent_surface);
  EXPECT_TRUE(parent_surface->HasLeaveEnterCallbackForTesting());
  // Normal surface will create accessibility nodes.
  EXPECT_FALSE(parent_surface->window()->GetProperty(
      ui::kAXConsiderInvisibleAndIgnoreChildren));

  // Create another surface.
  test::ResourceKey child_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    ClientData* data = client->GetDataAs<ClientData>();
    data->child_wl_surface.reset(
        wl_compositor_create_surface(client->compositor()));
    child_surface_key =
        test::client_util::GetResourceKey(data->child_wl_surface.get());
  });
  Surface* child_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), child_surface_key);
  // Check that the surface sends enter/leave events.
  ASSERT_TRUE(child_surface);
  EXPECT_TRUE(child_surface->HasLeaveEnterCallbackForTesting());

  // Make it a subsurface of the first one.
  test::ResourceKey child_subsurface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    ClientData* data = client->GetDataAs<ClientData>();
    data->child_wl_subsurface.reset(wl_subcompositor_get_subsurface(
        client->subcompositor(), data->child_wl_surface.get(),
        data->parent_wl_surface.get()));
    child_subsurface_key =
        test::client_util::GetResourceKey(data->child_wl_subsurface.get());
  });
  SubSurface* child_subsurface =
      test::server_util::GetUserDataForResource<SubSurface>(
          server_.get(), child_subsurface_key);
  // Check that the surface sends enter/leave events.
  ASSERT_TRUE(child_subsurface);
  EXPECT_TRUE(child_surface->HasLeaveEnterCallbackForTesting());
  EXPECT_TRUE(child_subsurface->surface()->HasLeaveEnterCallbackForTesting());

  // Create yet another surface. Make it augmented.
  test::ResourceKey child2_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ClientData>();
    data->child2_wl_surface.reset(
        wl_compositor_create_surface(client->compositor()));
    data->augmented_surface = surface_augmenter_get_augmented_surface(
        client->surface_augmenter(), data->child2_wl_surface.get());
    child2_surface_key =
        test::client_util::GetResourceKey(data->child2_wl_surface.get());
  });
  Surface* child2_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), child2_surface_key);
  ASSERT_TRUE(child2_surface);

  // Make it a subsurface of the first one.
  test::ResourceKey child2_subsurface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ClientData>();

    data->child2_wl_subsurface.reset(wl_subcompositor_get_subsurface(
        client->subcompositor(), data->child2_wl_surface.get(),
        data->parent_wl_surface.get()));

    child2_subsurface_key =
        test::client_util::GetResourceKey(data->child2_wl_subsurface.get());
  });
  SubSurface* child2_subsurface =
      test::server_util::GetUserDataForResource<SubSurface>(
          server_.get(), child2_subsurface_key);
  ASSERT_TRUE(child2_subsurface);

  // Check that it does not send the events.
  EXPECT_FALSE(child2_surface->HasLeaveEnterCallbackForTesting());
  EXPECT_FALSE(child2_subsurface->surface()->HasLeaveEnterCallbackForTesting());
}

class ShellClientData : public ClientData {
 public:
  void CreateXdgToplevel(test::TestClient* client, wl_surface* surface) {
    ASSERT_FALSE(xdg_surface_);
    xdg_surface_.reset(
        xdg_wm_base_get_xdg_surface(client->xdg_wm_base(), surface));

    ASSERT_FALSE(xdg_toplevel_);
    xdg_toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));

    ASSERT_FALSE(aura_toplevel_);
    aura_toplevel_.reset(zaura_shell_get_aura_toplevel_for_xdg_toplevel(
        client->aura_shell(), xdg_toplevel_.get()));
  }

  std::unique_ptr<test::TestBuffer> parent_buffer;
  std::unique_ptr<test::TestBuffer> child_buffer;

  std::unique_ptr<wl_surface> child2_wl_surface;
  std::unique_ptr<wl_subsurface> child2_wl_subsurface;

 private:
  std::unique_ptr<xdg_surface> xdg_surface_;
  std::unique_ptr<xdg_toplevel> xdg_toplevel_;
  std::unique_ptr<zaura_toplevel> aura_toplevel_;
};

TEST_F(SurfaceAugmenterTest, AugmentedSubSurfacesAreNotAttachedToLayerTree) {
  //----------------------------------------------------------------
  //  Create a surface (top level).
  //----------------------------------------------------------------
  test::ResourceKey parent_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<ShellClientData>();

    data->parent_buffer =
        client->shm_buffer_factory()->CreateBuffer(0, 256, 256);
    data->parent_wl_surface.reset(
        wl_compositor_create_surface(client->compositor()));

    data->CreateXdgToplevel(client, data->parent_wl_surface.get());
    wl_surface_attach(data->parent_wl_surface.get(),
                      data->parent_buffer->resource(), 0, 0);
    wl_surface_commit(data->parent_wl_surface.get());

    parent_surface_key =
        test::client_util::GetResourceKey(data->parent_wl_surface.get());
    client->set_data(std::move(data));
  });
  Surface* parent_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), parent_surface_key);
  auto* parent_shell_surface = GetShellSurfaceBaseForWindow(
      parent_surface->window()->GetToplevelWindow());
  ASSERT_TRUE(parent_surface);
  ASSERT_TRUE(parent_shell_surface);
  ASSERT_TRUE(parent_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(gfx::SizeF(256, 256), parent_surface->content_size());

  //----------------------------------------------------------------
  //  Create another surface (subsurface of the toplevel one).
  //----------------------------------------------------------------
  test::ResourceKey child_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ShellClientData>();

    data->child_buffer =
        client->shm_buffer_factory()->CreateBuffer(0, 256, 256);
    data->child_wl_surface.reset(
        wl_compositor_create_surface(client->compositor()));

    child_surface_key =
        test::client_util::GetResourceKey(data->child_wl_surface.get());
  });
  Surface* child_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), child_surface_key);
  ASSERT_TRUE(child_surface);

  // Make it a subsurface of the first one.
  test::ResourceKey child_subsurface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ShellClientData>();

    data->child_wl_subsurface.reset(wl_subcompositor_get_subsurface(
        client->subcompositor(), data->child_wl_surface.get(),
        data->parent_wl_surface.get()));

    child_subsurface_key =
        test::client_util::GetResourceKey(data->child_wl_subsurface.get());
  });
  SubSurface* child_subsurface =
      test::server_util::GetUserDataForResource<SubSurface>(
          server_.get(), child_subsurface_key);
  ASSERT_TRUE(child_subsurface);

  //----------------------------------------------------------------
  //  Commit the surface hierarchy.
  //----------------------------------------------------------------
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ShellClientData>();
    wl_surface_commit(data->parent_wl_surface.get());
  });

  // Check that the surfaces's Layers are attached.
  EXPECT_TRUE(
      parent_surface->window()->layer()->cc_layer_for_testing()->IsAttached());
  EXPECT_TRUE(
      child_surface->window()->layer()->cc_layer_for_testing()->IsAttached());

  //----------------------------------------------------------------
  //  Create yet another surface. Make it augmented.
  //----------------------------------------------------------------
  test::ResourceKey child2_surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ShellClientData>();
    data->child2_wl_surface.reset(
        wl_compositor_create_surface(client->compositor()));
    data->augmented_surface = surface_augmenter_get_augmented_surface(
        client->surface_augmenter(), data->child2_wl_surface.get());
    child2_surface_key =
        test::client_util::GetResourceKey(data->child2_wl_surface.get());
  });
  Surface* child2_surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), child2_surface_key);
  ASSERT_TRUE(child2_surface);

  // Make it a subsurface of the first one.
  test::ResourceKey child2_subsurface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ShellClientData>();

    data->child2_wl_subsurface.reset(wl_subcompositor_get_subsurface(
        client->subcompositor(), data->child2_wl_surface.get(),
        data->parent_wl_surface.get()));

    child2_subsurface_key =
        test::client_util::GetResourceKey(data->child2_wl_subsurface.get());
  });
  SubSurface* child2_subsurface =
      test::server_util::GetUserDataForResource<SubSurface>(
          server_.get(), child2_subsurface_key);
  ASSERT_TRUE(child2_subsurface);

  //----------------------------------------------------------------
  //  Commit the surface hierarchy.
  //----------------------------------------------------------------
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ShellClientData>();
    wl_surface_commit(data->parent_wl_surface.get());
  });

  // Check that the last child's Layer is not attached.
  EXPECT_FALSE(
      child2_surface->window()->layer()->cc_layer_for_testing()->IsAttached());
}

}  // namespace
}  // namespace exo::wayland
