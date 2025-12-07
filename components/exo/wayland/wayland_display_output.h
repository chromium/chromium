// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/exo/wayland/output_metrics.h"

struct wl_client;
struct wl_global;
struct wl_resource;

namespace display {
class Display;
}  // namespace display

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
  explicit WaylandDisplayOutput(const display::Display& display);

  WaylandDisplayOutput(const WaylandDisplayOutput&) = delete;
  WaylandDisplayOutput& operator=(const WaylandDisplayOutput&) = delete;

  ~WaylandDisplayOutput();

  // Delay interval between delete attempts.
  static constexpr base::TimeDelta kDeleteTaskDelay = base::Seconds(3);
  // Number of times to retry deletion.
  static constexpr int kDeleteRetries = 3;

  int64_t id() const { return id_; }
  const OutputMetrics& metrics() const { return metrics_; }

  void set_global(wl_global* global) { global_ = global; }
  const wl_global* global() const { return global_; }

  // Register/Unregister output resources, which will be used to
  // notify surface when enter/leave the output.
  void UnregisterOutput(wl_resource* output_resource);
  void RegisterOutput(wl_resource* output_resource);

  // Dispatches updated metrics to all clients currently bound to the wrapped
  // wl_output global.
  // TODO(tluk): `display` is only used by WaylandDisplayObservers to construct
  // wayland output metrics to send to the client. Wrap any remaining metrics
  // into OutputMetrics and propagate this instead.
  void SendDisplayMetricsChanges(const display::Display& display,
                                 uint32_t changed_metrics);

  // Notifies clients of the activation of this output.
  void SendOutputActivated();

  wl_resource* GetOutputResourceForClient(wl_client* client);

  // Self destruct in 5 seconeds.
  // Caller must not access this object after calling this.
  void OnDisplayRemoved();

  int output_counts() const { return output_ids_.size(); }
  bool had_registered_output() const { return had_registered_output_; }

 private:
  const int64_t id_;
  OutputMetrics metrics_;
  raw_ptr<wl_global, DanglingUntriaged> global_ = nullptr;
  base::flat_map<wl_client*, raw_ptr<wl_resource, CtnExperimental>> output_ids_;
  bool had_registered_output_ = false;
  bool is_destructing_ = false;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
