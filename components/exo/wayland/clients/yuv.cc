// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <sys/mman.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

}  // namespace

class YuvClient : public ClientBase {
 public:
  YuvClient() {}

  bool WriteSolidColor(gbm_bo* bo, SkColor color);

  void Run(const ClientBase::InitParams& params);
};

bool YuvClient::WriteSolidColor(gbm_bo* bo, SkColor color) {
  for (size_t i = 0; i < gbm_bo_get_plane_count(bo); ++i) {
    base::ScopedFD fd(gbm_bo_get_plane_fd(bo, i));
    uint32_t stride = gbm_bo_get_stride_for_plane(bo, i);
    uint32_t offset = gbm_bo_get_offset(bo, i);
    uint32_t map_size = gbm_bo_get_plane_size(bo, i) + offset;
    void* void_data = mmap(nullptr, map_size, (PROT_READ | PROT_WRITE),
                           MAP_SHARED, fd.get(), 0);
    if (void_data == MAP_FAILED) {
      LOG(ERROR) << "Failed mmap().";
      return false;
    }
    uint8_t* data = static_cast<uint8_t*>(void_data) + offset;
    uint8_t yuv[] = {
        (0.257 * SkColorGetR(color)) + (0.504 * SkColorGetG(color)) +
            (0.098 * SkColorGetB(color)) + 16,
        -(0.148 * SkColorGetR(color)) - (0.291 * SkColorGetG(color)) +
            (0.439 * SkColorGetB(color)) + 128,
        (0.439 * SkColorGetR(color)) - (0.368 * SkColorGetG(color)) -
            (0.071 * SkColorGetB(color)) + 128};
    if (i == 0) {
      for (int y = 0; y < size_.height(); ++y) {
        for (int x = 0; x < size_.width(); ++x) {
          data[stride * y + x] = yuv[0];
        }
      }
    } else {
      for (int y = 0; y < size_.height() / 2; ++y) {
        for (int x = 0; x < size_.width() / 2; ++x) {
          data[stride * y + x * 2] = yuv[1];
          data[stride * y + x * 2 + 1] = yuv[2];
        }
      }
    }
    int ret = munmap(void_data, map_size);
    if (ret) {
      LOG(ERROR) << "Failed munmap().";
      return false;
    }
  }
  return true;
}

void YuvClient::Run(const ClientBase::InitParams& params) {
  if (!ClientBase::Init(params))
    return;
  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  size_t frame_number = 0;
  do {
    if (callback_pending)
      continue;
    frame_number++;

    Buffer* buffer = DequeueBuffer();
    if (!buffer) {
      LOG(ERROR) << "Can't find free buffer";
      return;
    }
    const SkColor kColors[] = {SK_ColorBLUE,   SK_ColorGREEN, SK_ColorRED,
                               SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};
    if (!WriteSolidColor(buffer->bo.get(),
                         kColors[frame_number % buffers_.size()]))
      return;

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &callback_pending);
    callback_pending = true;
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
  params.use_drm = true;
  params.num_buffers = 8;  // Allow up to 8 buffers by default.
  if (!params.FromCommandLine(*command_line))
    return 1;

  if (!params.use_drm) {
    LOG(ERROR) << "Missing --use-drm parameter which is required for buffer "
                  "allocation";
    return 1;
  }

  // TODO(dcastagna): Support other YUV formats.
  params.drm_format = DRM_FORMAT_NV12;
  params.bo_usage =
      GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::YuvClient client;
  client.Run(params);
  return 0;
}
