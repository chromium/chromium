// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/client_tracker.h"

#include <sys/socket.h>
#include <wayland-server-protocol-core.h>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {

class WaylandClientTrackerTest : public testing::Test {
 protected:
  struct ClientData {
    raw_ptr<wl_client> client = nullptr;
    int fds[2] = {0, 0};
  };

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    wayland_display_ = wl_display_create();
    client_tracker_ = std::make_unique<ClientTracker>(wayland_display_);
  }
  void TearDown() override {
    while (clients_.size() > 0) {
      DestroyClient(clients_.back().client);
    }
    client_tracker_.reset();
    wl_display_destroy(wayland_display_.ExtractAsDangling());
    testing::Test::TearDown();
  }

  // Creates a wl_client for this test's display.
  wl_client* CreateClient() {
    ClientData client_data;
    socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, client_data.fds);
    client_data.client = wl_client_create(wayland_display_, client_data.fds[0]);
    clients_.push_back(std::move(client_data));
    return clients_.back().client;
  }

  // Destroys the wl_client.
  void DestroyClient(wl_client* client) {
    auto it = std::find_if(
        clients_.begin(), clients_.end(),
        [&](const ClientData& data) { return data.client == client; });

    if (it != clients_.end()) {
      wl_client_destroy(it->client.ExtractAsDangling());
      close(it->fds[1]);
      clients_.erase(it);
    }
  }

  raw_ptr<wl_display> wayland_display_ = nullptr;
  std::unique_ptr<ClientTracker> client_tracker_;
  std::vector<ClientData> clients_;
};

// Tests that clients are tracked correctly when they are created and destroyed.
TEST_F(WaylandClientTrackerTest, ClientRegistersAndDeregistersSuccessfully) {
  EXPECT_EQ(0, client_tracker_->NumClientsTrackedForTesting());

  // Create two client, both should report as not destroyed.
  wl_client* client_1 = CreateClient();
  EXPECT_TRUE(client_1);
  EXPECT_EQ(1, client_tracker_->NumClientsTrackedForTesting());
  EXPECT_FALSE(client_tracker_->IsClientDestroyed(client_1));

  wl_client* client_2 = CreateClient();
  EXPECT_TRUE(client_2);
  EXPECT_EQ(2, client_tracker_->NumClientsTrackedForTesting());
  EXPECT_FALSE(client_tracker_->IsClientDestroyed(client_2));

  // Destroy the second client. It should have been removed from the client
  // tracker and the first client should still be tracked.
  DestroyClient(client_2);
  EXPECT_EQ(1, client_tracker_->NumClientsTrackedForTesting());
  EXPECT_FALSE(client_tracker_->IsClientDestroyed(client_1));

  // Destroy the first client, it should have been removed from the client
  // tracker.
  DestroyClient(client_1);
  EXPECT_EQ(0, client_tracker_->NumClientsTrackedForTesting());
}

// Simulate a situation where wayland will call back into test code after client
// destruction has started, but before the client destruction has finished.
// Assert that the client is reported as destroted after destruction has begun.
TEST_F(WaylandClientTrackerTest, ClientTaggedDestroyedAfterDestructionStarted) {
  wl_client* client = CreateClient();
  EXPECT_FALSE(client_tracker_->IsClientDestroyed(client));

  // Create a resource associated with the client. Attach user data and a
  // destructor to the resource.
  struct UserData {
    raw_ptr<ClientTracker> client_tracker = nullptr;
    raw_ptr<wl_client> client = nullptr;
  };
  UserData data = {.client_tracker = client_tracker_.get(), .client = client};

  wl_resource* output_resource =
      wl_resource_create(client, &wl_output_interface, 2, 0);
  wl_resource_set_user_data(output_resource, &data);

  // Assert that the client is no longer valid once destruction has begun and
  // the client's resources subsequently begin destruction.
  auto wl_resource_callback = [](wl_resource* resource) {
    UserData* data =
        static_cast<UserData*>(wl_resource_get_user_data(resource));
    EXPECT_TRUE(data->client_tracker->IsClientDestroyed(
        data->client.ExtractAsDangling()));
  };
  wl_resource_set_destructor(output_resource, wl_resource_callback);

  DestroyClient(client);
}

}  // namespace wayland
}  // namespace exo
