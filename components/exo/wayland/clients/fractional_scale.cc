// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fractional-scale-v1-client-protocol.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace exo {
namespace wayland {
namespace clients {

class FractionalScaleClient : public ClientBase {
 public:
  FractionalScaleClient() = default;

  void Run(const ClientBase::InitParams& params);

  void SetFractionalScale(double scale);

 private:
  void Paint();

  bool needs_repaint_ = true;
  double fractional_scale_ = 0.0;

  int32_t drm_format_ = 0;
  int32_t bo_usage_ = 0;
  bool use_release_fences_ = false;
};

namespace {

void HandlePreferredScale(void* data,
                          struct wp_fractional_scale_v1* info,
                          uint32_t wire_scale) {
  FractionalScaleClient* client = static_cast<FractionalScaleClient*>(data);
  client->SetFractionalScale(wire_scale / 120.0);
}

}  // namespace

void FractionalScaleClient::Run(const InitParams& params) {
  if (!ClientBase::Init(params)) {
    return;
  }

  drm_format_ = params.drm_format;
  bo_usage_ = params.bo_usage;
  use_release_fences_ = params.use_release_fences;

  wp_fractional_scale_v1_listener fractional_scale_listener = {
      HandlePreferredScale};
  auto* fractional_scale_obj =
      wp_fractional_scale_manager_v1_get_fractional_scale(
          globals_.wp_fractional_scale_manager_v1.get(), surface_.get());
  wp_fractional_scale_v1_add_listener(fractional_scale_obj,
                                      &fractional_scale_listener, this);
  auto* viewport =
      wp_viewporter_get_viewport(globals_.wp_viewporter.get(), surface_.get());
  wp_viewport_set_destination(viewport, surface_size_.width(),
                              surface_size_.height());

  // Wait for initial fractional scale
  while (wl_display_dispatch(display_.get()) != -1) {
    if (fractional_scale_ > 0.0) {
      break;
    }
  }

  do {
    if (needs_repaint_) {
      Paint();
    }
  } while (wl_display_dispatch(display_.get()) != -1);
}

void FractionalScaleClient::SetFractionalScale(double scale) {
  fractional_scale_ = scale;
  needs_repaint_ = true;
}

void FractionalScaleClient::Paint() {
  const gfx::Size buffer_size = {
      static_cast<int>(round(size_.width() * fractional_scale_)),
      static_cast<int>(round(size_.height() * fractional_scale_))};
  auto buffer = CreateBuffer(buffer_size, drm_format_, bo_usage_,
                             /*add_buffer_listener=*/!use_release_fences_);
  if (!buffer) {
    return;
  }

  SkCanvas* canvas = buffer->sk_surface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  gfx::Point point = {0, 0};
  const gfx::Size square_size = {
      static_cast<int>(round(buffer_size.width() / 8.0)),
      static_cast<int>(round(buffer_size.height() / 8.0))};

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      const SkRect rect = gfx::RectToSkRect(gfx::Rect{point, square_size});

      SkColor color;
      if ((i + j) % 2) {
        color = SK_ColorRED;
      } else {
        color = SK_ColorBLUE;
      }

      SkPaint paint;
      paint.setColor(color);
      canvas->drawRect(rect, paint);

      point.SetPoint(point.x() + square_size.width(), point.y());
    }
    point.SetPoint(0, point.y() + square_size.height());
  }

  wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                    surface_size_.height());
  wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

  wl_surface_commit(surface_.get());
  wl_display_flush(display_.get());
  needs_repaint_ = false;
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  exo::wayland::clients::ClientBase::InitParams params;
  if (!params.FromCommandLine(*command_line)) {
    return 1;
  }

  if (params.use_drm) {
    LOG(ERROR) << "Unsupported parameter --use-drm";
    return 1;
  }

  if (params.scale != 1) {
    LOG(ERROR) << "Unsupported parameter --scale";
    return 1;
  }

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::FractionalScaleClient client;
  client.Run(params);
  return 0;
}
