// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_test_controller.h"

#include <stdint.h>
#include <test-controller-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include <memory>

#include "base/check.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "ui/events/base_event_utils.h"

namespace exo::wayland {

struct TestController::State {
  std::unique_ptr<base::SimpleTestTickClock> clock;
};

namespace {

void test_controller_mock_event_tick_clock_start(struct wl_client* client,
                                                 struct wl_resource* resource) {
  auto* state = GetUserDataAs<TestController::State>(resource);
  CHECK(state);
  state->clock = std::make_unique<base::SimpleTestTickClock>();
  state->clock->SetNowTicks(base::TimeTicks::Now());
  ui::SetEventTickClockForTesting(state->clock.get());
}

void test_controller_mock_event_tick_clock_advance(struct wl_client* client,
                                                   struct wl_resource* resource,
                                                   uint32_t milliseconds) {
  auto* state = GetUserDataAs<TestController::State>(resource);
  CHECK(state);
  if (state->clock) {
    state->clock->Advance(base::Milliseconds(milliseconds));
  }
}

const struct zcr_test_controller_v1_interface test_controller_implementation = {
    test_controller_mock_event_tick_clock_start,
    test_controller_mock_event_tick_clock_advance};

void destroy_test_controller_resource(struct wl_resource* resource) {
  ui::SetEventTickClockForTesting(nullptr);
}

void bind_test_controller(wl_client* client,
                          void* data,
                          uint32_t version,
                          uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_test_controller_v1_interface, version, id);

  wl_resource_set_implementation(resource, &test_controller_implementation,
                                 data, destroy_test_controller_resource);
}

}  // namespace

TestController::TestController(Server* server)
    : state_(std::make_unique<TestController::State>()) {
  wl_global_create(server->GetWaylandDisplay(),
                   &zcr_test_controller_v1_interface,
                   /*version=*/1, state_.get(), bind_test_controller);
}

TestController::~TestController() = default;

}  // namespace exo::wayland
