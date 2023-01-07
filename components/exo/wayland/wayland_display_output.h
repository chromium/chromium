// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_

#include <stdint.h>

#include "base/containers/flat_map.h"

struct wl_client;
struct wl_global;
struct wl_resource;

namespace exo {
namespace wayland {

// Class that represent a wayland output. Tied to a specific display ID and
// associated with a global, and wl_outputs created by clients.  This object
// will self destruct upon the display removal aftrer delays up to 9 seconds (3
// seconds x 3 times) to give time for clients to release the output they
// created, except for the shutdown scenario where they're removed immediately.
class WaylandDisplayOutput {
 public:
  explicit WaylandDisplayOutput(int64_t display_id);

  WaylandDisplayOutput(const WaylandDisplayOutput&) = delete;
  WaylandDisplayOutput& operator=(const WaylandDisplayOutput&) = delete;

  ~WaylandDisplayOutput();

  int64_t id() const;
  void set_global(wl_global* global);

  // Register/Unregister output resources, which will be used to
  // notify surface when enter/leave the output.
  void UnregisterOutput(wl_resource* output_resource);
  void RegisterOutput(wl_resource* output_resource);

  wl_resource* GetOutputResourceForClient(wl_client* client);

  // Self destruct in 5 seconeds.
  void OnDisplayRemoved();

  int output_counts() const { return output_ids_.size(); }

 private:
  const int64_t id_;
  wl_global* global_ = nullptr;
  base::flat_map<wl_client*, wl_resource*> output_ids_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
