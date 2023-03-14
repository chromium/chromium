// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <cstdint>

#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"
#include "components/exo/wayland/wayland_display_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {

namespace {

class WaylandDisplayOutputTest : public test::WaylandServerTest {
 public:
  WaylandDisplayOutputTest()
      : test::WaylandServerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  WaylandDisplayOutputTest(const WaylandDisplayOutputTest&) = delete;
  WaylandDisplayOutputTest& operator=(const WaylandDisplayOutputTest&) = delete;
  ~WaylandDisplayOutputTest() override = default;

  void TearDown() override {
    LOG(INFO) << "In TearDown, running task environment until idle.";
    task_environment()->RunUntilIdle();
    LOG(INFO) << "Task environment is now idle.";

    test::WaylandServerTest::TearDown();
  }
};

}  // namespace

// TODO(crbug.com/1421232): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_DelayedSelfDestruct DISABLED_DelayedSelfDestruct
#else
#define MAYBE_DelayedSelfDestruct DelayedSelfDestruct
#endif
TEST_F(WaylandDisplayOutputTest, MAYBE_DelayedSelfDestruct) {
  class ClientData : public test::TestClient::CustomData {
   public:
    wl_output* output = nullptr;
    uint32_t output_name = 0;
    uint32_t output_version = 0;
  };

  // Start with 2 displays.
  UpdateDisplay("800x600,1024x786");

  // Store info about the 2nd display on the client.
  test::ResourceKey output_resource_key;
  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ClientData>();
    // This gets the latest bound output on the client side, which should be the
    // 2nd display here.
    data->output = client->output();
    data->output_name = client->globals().output.name();
    data->output_version =
        wl_proxy_get_version(reinterpret_cast<wl_proxy*>(client->output()));
    client->set_data(std::move(data));

    output_resource_key = test::client_util::GetResourceKey(client->output());
  });

  auto* display_handler =
      test::server_util::GetUserDataForResource<WaylandDisplayHandler>(
          server_.get(), output_resource_key);
  EXPECT_EQ(display_handler->id(), GetSecondaryDisplay().id());

  // Remove the 2nd display.
  UpdateDisplay("800x600");

  // Fast forward until at least one delete has been attempted.
  // TODO(crbug.com/1420468): For flakes debugging.
  auto ff_delta = WaylandDisplayOutput::kDeleteTaskDelay * 1.5;
  LOG(INFO) << "Want fastforward: " << ff_delta;
  auto start_time = task_environment()->NowTicks();
  task_environment()->FastForwardBy(ff_delta);
  LOG(INFO) << "Actual fastforward: "
            << task_environment()->NowTicks() - start_time;

  LOG(INFO) << "Flushing client";
  client_thread_->FlushForTesting();
  LOG(INFO) << "Flushed client";

  // Try releasing now and check for client error.
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data = client->GetDataAs<ClientData>();
    EXPECT_EQ(data->output, client->globals().output.get());
    // TODO(crbug.com/1420468): For flakes debugging.
    LOG(INFO) << "Sending wl_output_release for output";
    wl_output_release(client->globals().output.release());
    LOG(INFO) << "Calling client roundtrip";
    client->Roundtrip();
    LOG(INFO) << "After client roundtrip";
    EXPECT_EQ(wl_display_get_error(client->display()), 0);
  });

  LOG(INFO) << "End of test case";
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

  // Quickly add then remove a display while the client is blocked.
  UpdateDisplay("800x600,1024x786");
  UpdateDisplay("800x600");

  // Fast forward until at least one delete has been attempted.
  // TODO(crbug.com/1420468): For flakes debugging.
  auto ff_delta = WaylandDisplayOutput::kDeleteTaskDelay * 1.5;
  LOG(INFO) << "Want fastforward: " << ff_delta;
  auto start_time = task_environment()->NowTicks();
  task_environment()->FastForwardBy(ff_delta);
  LOG(INFO) << "Actual fastforward: "
            << task_environment()->NowTicks() - start_time;

  // Unblock client thread so the bind request happens now.
  LOG(INFO) << "Signaling client";
  block_bind_event.Signal();
  LOG(INFO) << "Flushing client";
  client_thread_->FlushForTesting();
  LOG(INFO) << "Flushed client";

  // Check for client error.
  PostToClientAndWait([&](test::TestClient* client) {
    LOG(INFO) << "Calling client roundtrip";
    client->Roundtrip();
    LOG(INFO) << "After client roundtrip";
    EXPECT_EQ(wl_display_get_error(client->display()), 0);
  });

  LOG(INFO) << "Fastforwarding for clean up";
  task_environment()->FastForwardBy(WaylandDisplayOutput::kDeleteTaskDelay *
                                    WaylandDisplayOutput::kDeleteRetries);
  LOG(INFO) << "End of test case";
}

}  // namespace exo::wayland
