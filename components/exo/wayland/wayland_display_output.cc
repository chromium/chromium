// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <cstring>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_display_observer.h"

namespace exo {
namespace wayland {
namespace {

void DoDelete(WaylandDisplayOutput* output, int retry_count) {
  // Retry if a client hasn't released the output yet, or if no client has
  // even made the initial binding yet.
  if (output->output_counts() > 0 || !output->had_registered_output()) {
    if (retry_count > 0) {
      // If we can't post the task successfully, just delete the output
      // resource now, otherwise we would leak memory.
      if (base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE, base::BindOnce(&DoDelete, output, retry_count - 1),
              WaylandDisplayOutput::kDeleteTaskDelay)) {
        return;
      } else {
        LOG(WARNING) << "Failed to post delayed deletion task for "
                        "WaylandDisplayOutput with display id="
                     << output->id()
                     << " and remaining retry count: " << retry_count;
      }
    } else {
      LOG(WARNING)
          << "Timed out waiting for clients to unbind registered output for id="
          << output->id()
          << " with remaining bound outputs=" << output->output_counts();
    }
  }
  delete output;
}

}  // namespace

WaylandDisplayOutput::WaylandDisplayOutput(const display::Display& display)
    : id_(display.id()), metrics_(display) {}

WaylandDisplayOutput::~WaylandDisplayOutput() {
  // Empty the output_resources_ so that Unregister will be no op.
  auto resources = std::move(output_resources_);
  for (wl_resource* resource : resources) {
    if (wl_resource_get_version(resource) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
      // At version >= 3, clients should send wl_output.release to let server
      // know that an output object will be unused. Remove the user_data and
      // destructor, keep wl_resource alive as there could be other requests
      // referencing it asynchronously.
      DestroyUserData<WaylandDisplayHandler>(resource);
      wl_resource_set_user_data(resource, nullptr);
      wl_resource_set_destructor(resource, nullptr);
    } else {
      wl_resource_destroy(resource);
    }
  }

  if (global_) {
    wl_global_destroy(global_);
  }
}

void WaylandDisplayOutput::OnDisplayRemoved() {
  if (global_) {
    wl_global_remove(global_);
  }

  is_destructing_ = true;

  if (!base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&DoDelete, this, kDeleteRetries),
          kDeleteTaskDelay)) {
    // If we can't schedule the delete task, just delete now to not leak memory.
    LOG(WARNING) << "Failed to post initial delayed deletion task for "
                    "WaylandDisplayOutput with display id="
                 << id();
    delete this;
  }
}

void WaylandDisplayOutput::UnregisterOutput(wl_resource* output_resource) {
  output_resources_.erase(output_resource);
}

void WaylandDisplayOutput::RegisterOutput(wl_resource* output_resource) {
  auto* client = wl_resource_get_client(output_resource);
  // Ensure every binding must be tracked, to prevent untracked resource
  // dangling.
  CHECK(output_resources_.insert(output_resource).second);
  had_registered_output_ = true;

  if (is_destructing_) {
    return;
  }

  // Notify All wl surfaces that a new output was added.
  wl_client_for_each_resource(
      client,
      [](wl_resource* resource, void*) {
        if (std::string_view(wl_resource_get_class(resource)) == "wl_surface") {
          if (auto* surface = GetUserDataAs<Surface>(resource)) {
            surface->OnNewOutputAdded();
          }
        }
        return WL_ITERATOR_CONTINUE;
      },
      nullptr);
}

wl_resource* WaylandDisplayOutput::GetOutputResourceForClient(
    wl_client* client) {
  for (wl_resource* resource : output_resources_) {
    // This returns the first `resource` in iteration for `client`, if the
    // `client` has multiple `resource`s created for this output, it is up to
    // the client to be aware of the duplication.
    if (wl_resource_get_client(resource) == client) {
      return resource;
    }
  }
  return nullptr;
}

void WaylandDisplayOutput::SendDisplayMetricsChanges(
    const display::Display& display,
    uint32_t changed_metrics) {
  CHECK_EQ(display.id(), id_);
  // Update output metrics before propagating display changes.
  metrics_ = OutputMetrics(display);

  for (wl_resource* resource : output_resources_) {
    if (auto* handler = GetUserDataAs<WaylandDisplayHandler>(resource)) {
      handler->SendDisplayMetricsChanges(display, changed_metrics);
    }
  }
}

void WaylandDisplayOutput::SendOutputActivated() {
  for (wl_resource* resource : output_resources_) {
    auto* handler = GetUserDataAs<WaylandDisplayHandler>(resource);
    CHECK(handler);
    handler->SendDisplayActivated();
  }
}

}  // namespace wayland
}  // namespace exo
