// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_SIMPLE_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_SIMPLE_H_

#include "base/time/time.h"
#include "components/exo/wayland/clients/client_base.h"

namespace exo {
namespace wayland {
namespace clients {

class Simple : public wayland::clients::ClientBase {
 public:
  Simple();

  Simple(const Simple&) = delete;
  Simple& operator=(const Simple&) = delete;

  struct PresentationFeedback {
    // Total presentation latency of all presented frames.
    base::TimeDelta total_presentation_latency;

    // Number of presented frames.
    uint32_t num_frames_presented = 0;
  };
  struct RunParam {
    // Whether the client should log vsync.
    bool log_vsync_timing_updates = false;

    // Whether the client should use single pixel buffer.
    bool single_pixel_buffer = false;
  };

  void Run(int frames,
           const RunParam& run_param,
           PresentationFeedback* feedback = nullptr);
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_SIMPLE_H_
