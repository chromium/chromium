// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

}  // namespace

// This demo testes subsurface features of exo.
// For the first 200 frames: we animate parent surface, child_surface and
// child_surface's position.
// For 200-400 frames: we animate parent surface and child_surface's position.
// For 400-600 frames: we only animate child_surface's position.
// After 600 frames: we still animate child_surface's position, but don't call
// commit on parent surface. So the window will stop updating.
class SubSurfaceClient : public ClientBase {
 public:
  SubSurfaceClient() = default;

  SubSurfaceClient(const SubSurfaceClient&) = delete;
  SubSurfaceClient& operator=(const SubSurfaceClient&) = delete;

  ~SubSurfaceClient() override = default;

  void Run(const ClientBase::InitParams& params);
};

void SubSurfaceClient::Run(const ClientBase::InitParams& params) {
  if (!ClientBase::Init(params))
    return;

  std::unique_ptr<wl_surface> child_surface(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));

  std::unique_ptr<wl_subsurface> subsurface(
      static_cast<wl_subsurface*>(wl_subcompositor_get_subsurface(
          globals_.subcompositor.get(), child_surface.get(), surface_.get())));

  if (!child_surface || !subsurface) {
    LOG(ERROR) << "Can't create subsurface";
    return;
  }

  constexpr int32_t kSubsurfaceWidth = 128;
  constexpr int32_t kSubsurfaceHeight = 128;
  auto subbuffer =
      CreateBuffer(gfx::Size(kSubsurfaceWidth, kSubsurfaceHeight),
                   params.drm_format, params.bo_usage,
                   /*add_buffer_listener=*/!params.use_release_fences);
  if (!subbuffer) {
    LOG(ERROR) << "Failed to create subbuffer";
    return;
  }

  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  size_t frame_count = 0;
  do {
    if (callback_pending && frame_count < 600)
      continue;

    // Only generate frames to child surface for the first 200 frames.
    if (frame_count < 200) {
      SkScalar half_width = SkScalarHalf(kSubsurfaceWidth);
      SkScalar half_height = SkScalarHalf(kSubsurfaceHeight);
      SkIRect rect = SkIRect::MakeXYWH(-SkScalarHalf(half_width),
                                       -SkScalarHalf(half_height), half_width,
                                       half_height);
      // Rotation speed (degrees/frame).
      const double kRotationSpeed = 5.;
      SkScalar rotation = frame_count * kRotationSpeed;
      SkCanvas* canvas = subbuffer->sk_surface->getCanvas();
      canvas->save();
      canvas->clear(SK_ColorBLACK);
      SkPaint paint;
      paint.setColor(SkColorSetA(SK_ColorYELLOW, 0xA0));
      canvas->translate(half_width, half_height);
      canvas->rotate(rotation);
      canvas->drawIRect(rect, paint);
      canvas->restore();
      if (gr_context_) {
        gr_context_->flushAndSubmit();
        glFinish();
      }
      wl_surface_damage(child_surface.get(), 0, 0, kSubsurfaceWidth,
                        kSubsurfaceHeight);
      wl_surface_attach(child_surface.get(), subbuffer->buffer.get(), 0, 0);
      wl_surface_commit(child_surface.get());
    }

    // Only generate frames to parent surface for the first 400 frames.
    if (frame_count < 400) {
      Buffer* buffer = buffers_.front().get();
      SkCanvas* canvas = buffer->sk_surface->getCanvas();
      static const SkColor kColors[] = {SK_ColorRED, SK_ColorBLACK};
      canvas->clear(kColors[frame_count % std::size(kColors)]);
      if (gr_context_) {
        gr_context_->flushAndSubmit();
        glFinish();
      }
      wl_surface_set_buffer_scale(surface_.get(), scale_);
      wl_surface_set_buffer_transform(surface_.get(), transform_);
      wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                        surface_size_.height());
      wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

      callback_pending = true;
      frame_callback.reset(wl_surface_frame(surface_.get()));
      wl_callback_add_listener(frame_callback.get(), &frame_listener,
                               &callback_pending);
    }

    // Always animate subsurface position.
    wl_subsurface_set_position(subsurface.get(), frame_count % 50,
                               frame_count % 50);

    // Only commit changes for the first 600 frames.
    if (frame_count < 600)
      wl_surface_commit(surface_.get());

    wl_display_flush(display_.get());
    ++frame_count;
  } while (wl_display_dispatch(display_.get()) != -1);
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  exo::wayland::clients::ClientBase::InitParams params;
  if (!params.FromCommandLine(*command_line))
    return 1;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::SubSurfaceClient client;
  client.Run(params);
  return 1;
}
