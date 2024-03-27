// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/test_buffer.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/size_f.h"

namespace exo::wayland {
namespace {

class WaylandSurfaceTest : public test::WaylandServerTest,
                           public ::testing::WithParamInterface<float> {
 public:
  WaylandSurfaceTest() = default;

  WaylandSurfaceTest(const WaylandSurfaceTest&) = delete;
  WaylandSurfaceTest& operator=(const WaylandSurfaceTest&) = delete;

  ~WaylandSurfaceTest() override = default;

  // test::WaylandServerTest:
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Set the device scale factor.
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", device_scale_factor()));

    WaylandServerTest::SetUp();
  }

  void TearDown() override {
    WaylandServerTest::TearDown();
    display::Display::ResetForceDeviceScaleFactorForTesting();
  }

 private:
  float device_scale_factor() const { return GetParam(); }
};

class TestBufferListener : public test::BufferListener {
 public:
  int32_t release_buffer_call_count() const {
    return release_buffer_call_count_;
  }

  // test::BufferListener:
  void OnRelease(wl_buffer* resource) override { release_buffer_call_count_++; }

 private:
  int32_t release_buffer_call_count_ = 0;
};

// Instantiate the values of device scale factor in the parameterized tests.
INSTANTIATE_TEST_SUITE_P(All,
                         WaylandSurfaceTest,
                         testing::Values(1.0f, 1.25f, 2.0f));

TEST_P(WaylandSurfaceTest, Attach) {
  class ClientData : public test::TestClient::CustomData {
   public:
    std::unique_ptr<test::TestBuffer> buffer;
    std::unique_ptr<wl_surface> surface;
    TestBufferListener buffer_listener;
  };

  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<ClientData>();

    data->buffer = client->shm_buffer_factory()->CreateBuffer(0, 256, 256);
    data->buffer->SetListener(&data->buffer_listener);
    data->surface.reset(wl_compositor_create_surface(client->compositor()));
    wl_surface_attach(data->surface.get(), data->buffer->resource(), 0, 0);

    surface_key = test::client_util::GetResourceKey(data->surface.get());
    client->set_data(std::move(data));
  });

  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);

  ASSERT_TRUE(surface);
  EXPECT_TRUE(surface->HasPendingAttachedBuffer());

  PostToClientAndWait([&](test::TestClient* client) {
    ClientData* data = client->GetDataAs<ClientData>();
    wl_surface_commit(data->surface.get());
  });
  EXPECT_EQ(gfx::SizeF(256, 256), surface->content_size());

  PostToClientAndWait([&](test::TestClient* client) {
    ClientData* data = client->GetDataAs<ClientData>();

    // Commit without calling Attach() should have no effect.
    wl_surface_commit(data->surface.get());

    client->Roundtrip();

    EXPECT_EQ(0, data->buffer_listener.release_buffer_call_count());

    // Attach a null buffer to surface, this should release the previously
    // attached buffer.
    wl_surface_attach(data->surface.get(), nullptr, 0, 0);
  });
  EXPECT_FALSE(surface->HasPendingAttachedBuffer());

  PostToClientAndWait([&](test::TestClient* client) {
    ClientData* data = client->GetDataAs<ClientData>();
    wl_surface_commit(data->surface.get());

    client->Roundtrip();

    EXPECT_EQ(1, data->buffer_listener.release_buffer_call_count());
  });
  EXPECT_TRUE(surface->content_size().IsEmpty());
}

}  // namespace
}  // namespace exo::wayland
