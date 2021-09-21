// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gbm.h>
#include <cstdint>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/drm_util_linux.h"

namespace {

std::string DrmCodeToString(uint64_t drm_format) {
  return std::string{static_cast<char>(drm_format),
                     static_cast<char>(drm_format >> 8),
                     static_cast<char>(drm_format >> 16),
                     static_cast<char>(drm_format >> 24), 0};
}

std::string DrmCodeToBufferFormatString(uint64_t drm_format) {
  return gfx::BufferFormatToString(
      ui::GetBufferFormatFromFourCCFormat(drm_format));
}

}  // namespace

namespace exo {
namespace wayland {
namespace clients {

namespace {

constexpr uint32_t kBufferUsage =
    GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING;

class BufferCheckerClient : public ClientBase {
 public:
  explicit BufferCheckerClient() = default;
  ~BufferCheckerClient() override = default;

  int Run(const ClientBase::InitParams& params) {
    CHECK(ClientBase::Init(params));

    bool callback_pending = false;
    std::unique_ptr<wl_callback> frame_callback;
    wl_callback_listener frame_listener = {
        [](void* data, struct wl_callback*, uint32_t) {
          *(static_cast<bool*>(data)) = false;
        }};

    base::queue<uint32_t> formats_to_test;
    for (auto format : reported_formats)
      formats_to_test.push(format);

    uint32_t current_format;
    std::unique_ptr<Buffer> current_buffer;
    do {
      if (callback_pending)
        continue;

      if (current_buffer) {
        LOG(ERROR) << "Successfully used buffer with format drm: "
                   << DrmCodeToString(current_format) << " gfx::BufferFormat: "
                   << DrmCodeToBufferFormatString(current_format);
      }

      if (wl_display_get_error(display_.get())) {
        LOG(ERROR) << "Wayland error encountered";
        return -1;
      }

      // Buffers may fail to be created, so loop until we get one or return
      // early if we run out. We can't loop in the outer loop because, without
      // doing the rest of the code, we won't be dispatched to again.
      do {
        if (formats_to_test.size() == 0)
          return 0;

        current_format = formats_to_test.front();
        formats_to_test.pop();

        current_buffer = CreateDrmBuffer(
            gfx::Size(surface_size_.width(), surface_size_.height()),
            current_format, kBufferUsage, /*y_invert=*/false);
        if (!current_buffer) {
          LOG(ERROR) << "Unable to create buffer for drm: "
                     << DrmCodeToString(current_format)
                     << " gfx::BufferFormat: "
                     << DrmCodeToBufferFormatString(current_format);
        }
      } while (current_buffer == nullptr);

      LOG(ERROR) << "Attempting to use buffer with format drm: "
                 << DrmCodeToString(current_format) << " gfx::BufferFormat: "
                 << DrmCodeToBufferFormatString(current_format);

      wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                        surface_size_.height());
      wl_surface_attach(surface_.get(), current_buffer->buffer.get(), 0, 0);

      frame_callback.reset(wl_surface_frame(surface_.get()));
      wl_callback_add_listener(frame_callback.get(), &frame_listener,
                               &callback_pending);
      callback_pending = true;
      wl_surface_commit(surface_.get());

      wl_display_flush(display_.get());
    } while (wl_display_dispatch(display_.get()) != -1);

    LOG(ERROR)
        << "Expected to return from inside the loop. Wayland disconnected?";
    return -1;
  }

  std::vector<uint32_t> reported_formats;
  base::flat_map<uint32_t, std::vector<uint64_t>> reported_format_modifer_map;

 protected:
  void HandleDmabufFormat(void* data,
                          struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                          uint32_t format) override {
    reported_formats.push_back(format);
  }

  void HandleDmabufModifier(void* data,
                            struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
                            uint32_t format,
                            uint32_t modifier_hi,
                            uint32_t modifier_lo) override {
    if (!reported_format_modifer_map.contains(format)) {
      reported_format_modifer_map[format] = std::vector<uint64_t>();
    }

    uint64_t modifier = static_cast<uint64_t>(modifier_hi) << 32 | modifier_lo;
    reported_format_modifer_map[format].push_back(modifier);
  }
};

}  // namespace
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

  if (!params.use_drm) {
    LOG(ERROR) << "Missing --use-drm parameter which is required for gbm "
                  "buffer allocation";
    return 1;
  }

  // Initialize no buffers when we start (wait until we've gotten the list from
  // dmabuf)
  params.num_buffers = 0;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::BufferCheckerClient client;
  int ret = client.Run(params);

  std::vector<std::string> drm_names;
  std::vector<std::string> buffer_names;
  for (auto reported_format : client.reported_formats) {
    drm_names.push_back(DrmCodeToString(reported_format));
    buffer_names.push_back(DrmCodeToBufferFormatString(reported_format));
  }

  LOG(ERROR) << "zwp_linux_dmabuf_v1 reported supported DRM formats: "
             << base::JoinString(drm_names, ", ");
  LOG(ERROR) << "zwp_linux_dmabuf_v1 reported supported gfx::BufferFormats: "
             << base::JoinString(buffer_names, ", ");

  return ret;
}
