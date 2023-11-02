// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_FULLSCREEN_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_FULLSCREEN_SHELL_H_

#include "components/exo/wayland/clients/client_base.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
namespace wayland {
namespace clients {

// Sample Wayland client that uses the Fullscreen Shell protocol
// to display a fullscreen application with touch support.
class FullscreenClient : public ClientBase {
 public:
  FullscreenClient();

  FullscreenClient(const FullscreenClient&) = delete;
  FullscreenClient& operator=(const FullscreenClient&) = delete;

  ~FullscreenClient() override;
  bool Run(const InitParams& params);

 private:
  void AllocateBuffers(const InitParams& params);
  void Paint(const wl_callback_listener& frame_listener);

  // Overridden from ClientBase
  void HandleDown(void* data,
                  struct wl_touch* wl_touch,
                  uint32_t serial,
                  uint32_t time,
                  struct wl_surface* surface,
                  int32_t id,
                  wl_fixed_t x,
                  wl_fixed_t y) override;
  void HandleMode(void* data,
                  struct wl_output* wl_output,
                  uint32_t flags,
                  int32_t width,
                  int32_t height,
                  int32_t refresh) override;
  void HandleDone(void* data, struct wl_output* wl_output) override;

  bool has_mode_ = false;
  bool done_receiving_modes_ = false;
  int frame_count_ = 0;
  int frames_ = 300;

  gfx::Point point_ = {100, 100};
  const gfx::Size square_size_ = {100, 100};
  int dir_x_ = 1;
  int dir_y_ = 1;
  const int step_size_ = 20;
  SkColor color_ = SK_ColorBLUE;

  std::unique_ptr<wl_callback> frame_callback_;
  bool frame_callback_pending_ = false;
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_FULLSCREEN_SHELL_H_
