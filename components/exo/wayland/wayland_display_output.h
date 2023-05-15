// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

struct wl_client;
struct wl_global;
struct wl_resource;

namespace exo {
namespace wayland {

// Class that represents a wayland output. Tied to a specific display ID and
// associated with a global, and wl_outputs created by clients.  This object
// will self destruct upon the display removal after delays up to 12 seconds
// (3 seconds x (1 initial delay + 3 retries)) to give time for clients to
// release the output they created, except for the shutdown scenario where
// they're removed immediately.
class WaylandDisplayOutput {
 public:
  explicit WaylandDisplayOutput(int64_t display_id);

  WaylandDisplayOutput(const WaylandDisplayOutput&) = delete;
  WaylandDisplayOutput& operator=(const WaylandDisplayOutput&) = delete;

  ~WaylandDisplayOutput();

  // Delay interval between delete attempts.
  static constexpr base::TimeDelta kDeleteTaskDelay = base::Seconds(3);
  // Number of times to retry deletion.
  static constexpr int kDeleteRetries = 3;

  int64_t id() const;
  void set_global(wl_global* global);

  // Register/Unregister output resources, which will be used to
  // notify surface when enter/leave the output.
  void UnregisterOutput(wl_resource* output_resource);
  void RegisterOutput(wl_resource* output_resource);

  wl_resource* GetOutputResourceForClient(wl_client* client);

  // Self destruct in 5 seconeds.
  // Caller must not access this object after calling this.
  void OnDisplayRemoved();

  int output_counts() const { return output_ids_.size(); }
  bool had_registered_output() const { return had_registered_output_; }

 private:
  const int64_t id_;
  raw_ptr<wl_global, ExperimentalAsh> global_ = nullptr;
  base::flat_map<wl_client*, wl_resource*> output_ids_;
  bool had_registered_output_ = false;
  bool is_destructing_ = false;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
