// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <cstring>

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/containers/cxx20_erase.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {
base::TimeDelta kDeleteTaskDelay = base::Seconds(3);

void DoDelete(WaylandDisplayOutput* output, int retry_count) {
  if (retry_count > 0 && output->output_counts() > 0) {
    // Try a few times to give a client chance to release it.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&DoDelete, output, retry_count - 1),
        kDeleteTaskDelay);
  } else {
    delete output;
  }
}

}  // namespace

WaylandDisplayOutput::WaylandDisplayOutput(int64_t id) : id_(id) {}

WaylandDisplayOutput::~WaylandDisplayOutput() {
  // Empty the output_ids_ so that Unregister will be no op.
  auto ids = std::move(output_ids_);
  for (auto pair : ids)
    wl_resource_destroy(pair.second);

  if (global_)
    wl_global_destroy(global_);
}

void WaylandDisplayOutput::OnDisplayRemoved() {
  if (global_)
    wl_global_remove(global_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&DoDelete, this, /*retry_count=*/3),
      kDeleteTaskDelay);
}

int64_t WaylandDisplayOutput::id() const {
  return id_;
}

void WaylandDisplayOutput::set_global(wl_global* global) {
  global_ = global;
}

void WaylandDisplayOutput::UnregisterOutput(wl_resource* output_resource) {
  base::EraseIf(output_ids_, [output_resource](auto& pair) {
    return pair.second == output_resource;
  });
}

void WaylandDisplayOutput::RegisterOutput(wl_resource* output_resource) {
  auto* client = wl_resource_get_client(output_resource);
  output_ids_.insert(std::make_pair(client, output_resource));

  // Notify All wl surfaces that a new output was added.
  wl_client_for_each_resource(
      client,
      [](wl_resource* resource, void*) {
        constexpr char kWlSurfaceClass[] = "wl_surface";

        const char* class_name = wl_resource_get_class(resource);
        if (std::strcmp(kWlSurfaceClass, class_name) == 0) {
          auto* surface = GetUserDataAs<Surface>(resource);
          if (surface)
            surface->OnNewOutputAdded();
        }
        return WL_ITERATOR_CONTINUE;
      },
      nullptr);
}

wl_resource* WaylandDisplayOutput::GetOutputResourceForClient(
    wl_client* client) {
  auto iter = output_ids_.find(client);
  if (iter == output_ids_.end())
    return nullptr;
  return iter->second;
}

}  // namespace wayland
}  // namespace exo
