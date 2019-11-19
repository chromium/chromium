// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/client_base.h"

#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* frame_callback_pending = static_cast<bool*>(data);
  *frame_callback_pending = false;
}

}  // namespace

// This client exercises the zwp_linux_explicit_synchronization_unstable_v1
// wayland protocol, which enables buffer acquire and release synchronization
// between client and server using dma-fences.
class ExplicitSynchronizationClient : public ClientBase {
 public:
  ExplicitSynchronizationClient() = default;

  // Initialize and run client main loop.
  void Run();

 private:
  DISALLOW_COPY_AND_ASSIGN(ExplicitSynchronizationClient);
};

void ExplicitSynchronizationClient::Run() {
  wl_callback_listener frame_listener = {FrameCallback};

  /* With a 60Hz redraw rate this completes a half-oscillation in 3 seconds */
  static const int kMaxDrawStep = 180;
  int draw_step = 0;
  int draw_step_dir = 1;

  // A valid GL context with support for fd fences is required for this client.
  CHECK(gr_context_)
      << "A valid GL context is required. Try running with --use-drm.";
  CHECK_EQ(egl_sync_type_, static_cast<unsigned>(EGL_SYNC_NATIVE_FENCE_ANDROID))
      << "EGL doesn't support the EGL_ANDROID_native_fence_sync extension.";

  // The server needs to support the linux_explicit_synchronization protocol.
  CHECK(globals_.linux_explicit_synchronization)
      << "Server doesn't support zwp_linux_explicit_synchronization_v1.";
  std::unique_ptr<zwp_linux_surface_synchronization_v1> surface_synchronization(
      zwp_linux_explicit_synchronization_v1_get_synchronization(
          globals_.linux_explicit_synchronization.get(), surface_.get()));
  DCHECK(surface_synchronization);

  std::unique_ptr<wl_callback> frame_callback;
  bool frame_callback_pending = false;

  do {
    if (frame_callback_pending)
      continue;

    Buffer* buffer = DequeueBuffer();
    if (!buffer)
      continue;

    /* Oscillate between 0 and kMaxDrawStep */
    draw_step += draw_step_dir;
    if (draw_step == 0 || draw_step == kMaxDrawStep)
      draw_step_dir = -draw_step_dir;

    SkCanvas* canvas = buffer->sk_surface->getCanvas();
    float draw_step_percent = static_cast<float>(draw_step) / kMaxDrawStep;
    canvas->clear(
        SkColor4f{0.0, draw_step_percent, 1.0 - draw_step_percent, 1.0}
            .toSkColor());

    // Create an EGLSyncKHR object to signal when rendering is done.
    gr_context_->flush();
    buffer->egl_sync.reset(new ScopedEglSync(
        eglCreateSyncKHR(eglGetCurrentDisplay(), egl_sync_type_, nullptr)));
    DCHECK(buffer->egl_sync->is_valid());
    glFlush();

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    // Get a fence fd from from EGLSyncKHR and use it as the
    // acquire fence for the commit.
    base::ScopedFD fence_fd(eglDupNativeFenceFDANDROID(
        eglGetCurrentDisplay(), buffer->egl_sync->get()));
    DCHECK_GE(fence_fd.get(), 0);
    zwp_linux_surface_synchronization_v1_set_acquire_fence(
        surface_synchronization.get(), fence_fd.get());

    // Set up the frame callback.
    frame_callback_pending = true;
    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &frame_callback_pending);

    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
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
  exo::wayland::clients::ExplicitSynchronizationClient client;
  if (!client.Init(params))
    return 1;

  client.Run();

  return 0;
}
