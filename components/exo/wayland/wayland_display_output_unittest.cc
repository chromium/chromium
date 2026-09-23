// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <cstdint>
#include <memory>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/exo/wayland/output_controller_test_api.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "components/exo/wayland/wl_output.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {

using WaylandDisplayOutputTest = test::WaylandServerTest;

TEST_F(WaylandDisplayOutputTest, DelayedSelfDestruct) {
  class ClientData : public test::TestClient::CustomData {
   public:
    raw_ptr<wl_output, DanglingUntriaged> output = nullptr;
  };

  // Start with 2 displays.
  UpdateDisplay("800x600,1024x786");

  // Store info about the 2nd display on the client.
  test::ResourceKey output_resource_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();
    // This gets the latest bound output on the client side, which should be the
    // 2nd display here.
    ASSERT_EQ(client->globals().outputs.size(), 2u);
    data->output = client->globals().outputs.back().get();
    output_resource_key = test::client_util::GetResourceKey(data->output);
    client->set_data(std::move(data));
  });

  auto* display_handler =
      test::server_util::GetUserDataForResource<WaylandDisplayHandler>(
          server_.get(), output_resource_key);
  EXPECT_EQ(display_handler->id(), GetSecondaryDisplay().id());

  // Remove the 2nd display.
  UpdateDisplay("800x600");

  // Fast forward until at least one delete has been attempted.
  task_environment()->FastForwardBy(WaylandDisplayOutput::kDeleteTaskDelay *
                                    1.5);

  // Try releasing now and check for client error.
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ClientData>();
    ASSERT_EQ(client->globals().outputs.size(), 2u);
    EXPECT_EQ(data->output, client->globals().outputs.back().get());
    wl_output_release(client->globals().outputs.back().release());
    client->Roundtrip();
    EXPECT_EQ(wl_display_get_error(client->display()), 0);
  });
}

// Verify that in the case where an output is added and removed quickly before
// the client's initial bind, the server still waits for the full amount of
// delete delays before deleting the global resource.
TEST_F(WaylandDisplayOutputTest, DelayedSelfDestructBeforeFirstBind) {
  UpdateDisplay("800x600");

  // Block client thread so the initial bind request doesn't happen yet.
  base::WaitableEvent block_bind_event;
  ASSERT_TRUE(client_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] { block_bind_event.Wait(); })));

  // Quickly add then remove a 2nd display while the client is blocked.
  UpdateDisplay("800x600,1024x786");
  UpdateDisplay("800x600");

  // Fast forward until at least one delete has been attempted.
  task_environment()->FastForwardBy(WaylandDisplayOutput::kDeleteTaskDelay *
                                    1.5);

  // Unblock client thread so the bind request happens now.
  block_bind_event.Signal();
  client_thread_->FlushForTesting();

  // Check for client error.
  PostToClientAndWait([&](test::TestClient* client) {
    client->Roundtrip();
    EXPECT_EQ(wl_display_get_error(client->display()), 0);
  });

  // Clean up client's 2nd output object that was removed.
  PostToClientAndWait([&](test::TestClient* client) {
    wl_output_release(client->globals().outputs.back().release());
  });
}

// Tests to ensure exo processes added displays before removed displays for
// display configuration updates. This ensures exo's clients always see a valid
// Output during such configuration updates.
TEST_F(WaylandDisplayOutputTest, MaintainsNonEmptyOutputList) {
  // Start with 2 displays.
  UpdateDisplay("300x400,500x600");

  // Update to a new display configuration. The total global Outputs maintained
  // by exo should remain non-zero while processing the change (exo will CHECK
  // crash if it enters a zero output state).
  UpdateDisplay("700x800,900x1000", /*from_native_platform=*/false,
                /*generate_new_ids=*/true);
}

// Ensures metrics are correctly initialized and updated.
TEST_F(WaylandDisplayOutputTest, InitializesAndUpdatesMetrics) {
  // Start with a typical display configuration and confirm dimensions are
  // reflected in the metrics.
  UpdateDisplay("800x600");
  const int64_t display_id = display::Screen::Get()->GetAllDisplays()[0].id();
  OutputControllerTestApi output_controller_test_api(
      *server_->output_controller_for_testing());
  WaylandDisplayOutput* display_output =
      output_controller_test_api.GetWaylandDisplayOutput(display_id);
  EXPECT_EQ(gfx::Size(800, 600), display_output->metrics().logical_size);

  // Update display dimensions, this should be reflected in the metrics.
  UpdateDisplay("1200x800");
  EXPECT_EQ(gfx::Size(1200, 800), display_output->metrics().logical_size);
}

// Regression test for b/557391265. The wayland protocol allows a client to bind
// the same global more than once, so each wl_output binding must be tracked
// independently. Otherwise WaylandDisplayOutput can self destruct while an
// untracked WaylandDisplayHandler still holds a pointer to it, which results in
// a use-after-free when that handler is destroyed.
TEST_F(WaylandDisplayOutputTest, DuplicateOutputBindings) {
  class ClientData : public test::TestClient::CustomData {
   public:
    std::unique_ptr<wl_output> duplicate_output;
  };

  // Start with 2 displays.
  UpdateDisplay("800x600,1024x786");

  // Wait for the client to bind the newly added display's global.
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_EQ(client->globals().outputs.size(), 2u);
  });

  OutputControllerTestApi output_controller_test_api(
      *server_->output_controller_for_testing());
  WaylandDisplayOutput* display_output =
      output_controller_test_api.GetWaylandDisplayOutput(
          GetSecondaryDisplay().id());
  ASSERT_TRUE(display_output);
  EXPECT_EQ(display_output->output_counts(), 1);

  // Bind the 2nd display's global a second time from the same client.
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_EQ(client->globals().outputs.size(), 2u);
    auto data = std::make_unique<ClientData>();
    data->duplicate_output.reset(static_cast<wl_output*>(
        wl_registry_bind(client->globals().registry.get(),
                         client->globals().outputs.back().name(),
                         &wl_output_interface, kWlOutputVersion)));
    client->set_data(std::move(data));
    client->Roundtrip();
  });

  // Both bindings must be accounted for, including the duplicate one.
  EXPECT_EQ(display_output->output_counts(), 2);

  // Remove the 2nd display and let at least one delete attempt run.
  UpdateDisplay("800x600");
  task_environment()->FastForwardBy(WaylandDisplayOutput::kDeleteTaskDelay *
                                    1.5);

  // Release the first binding. The duplicate binding must keep the output
  // alive.
  PostToClientAndWait([&](test::TestClient* client) {
    wl_output_release(client->globals().outputs.back().release());
    client->Roundtrip();
    EXPECT_EQ(wl_display_get_error(client->display()), 0);
  });

  // Releasing the duplicate binding must not touch a destroyed
  // WaylandDisplayOutput.
  PostToClientAndWait([&](test::TestClient* client) {
    wl_output_release(
        client->GetDataAs<ClientData>()->duplicate_output.release());
    client->Roundtrip();
    EXPECT_EQ(wl_display_get_error(client->display()), 0);
  });
}

}  // namespace exo::wayland
