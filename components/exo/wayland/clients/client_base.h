// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/shared_memory_mapping.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_make_current.h"

#if defined(USE_GBM)
#include <gbm.h>
#if defined(USE_VULKAN)
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // defined(USE_VULKAN)
#endif  // defined(USE_GBM)

namespace base {
class CommandLine;
}

namespace exo {
namespace wayland {
namespace clients {

class ClientBase {
 public:
  struct InitParams {
    InitParams();
    ~InitParams();
    InitParams(const InitParams& params);

    bool FromCommandLine(const base::CommandLine& command_line);

    std::string title = "Wayland Client";
    size_t num_buffers = 2;
    size_t width = 256;
    size_t height = 256;
    int scale = 1;
    int transform = WL_OUTPUT_TRANSFORM_NORMAL;
    bool fullscreen = false;
    bool transparent_background = false;
    bool use_drm = false;
    std::string use_drm_value;
    int32_t drm_format = 0;
    int32_t bo_usage = 0;
    bool y_invert = false;
    bool allocate_buffers_with_output_mode = false;
    bool use_fullscreen_shell = false;
    bool use_touch = false;
    bool use_vulkan = false;
  };

  struct Globals {
    Globals();
    ~Globals();

    std::unique_ptr<wl_output> output;
    std::unique_ptr<wl_compositor> compositor;
    std::unique_ptr<wl_shm> shm;
    std::unique_ptr<wp_presentation> presentation;
    std::unique_ptr<zwp_linux_dmabuf_v1> linux_dmabuf;
    std::unique_ptr<wl_shell> shell;
    std::unique_ptr<wl_seat> seat;
    std::unique_ptr<wl_subcompositor> subcompositor;
    std::unique_ptr<wl_touch> touch;
    std::unique_ptr<zaura_shell> aura_shell;
    std::unique_ptr<zwp_fullscreen_shell_v1> fullscreen_shell;
    std::unique_ptr<zwp_input_timestamps_manager_v1> input_timestamps_manager;
    std::unique_ptr<zwp_linux_explicit_synchronization_v1>
        linux_explicit_synchronization;
    std::unique_ptr<zcr_vsync_feedback_v1> vsync_feedback;
  };

  struct Buffer {
    Buffer();
    ~Buffer();

    std::unique_ptr<wl_buffer> buffer;
    bool busy = false;
#if defined(USE_GBM)
    std::unique_ptr<gbm_bo> bo;
    std::unique_ptr<ScopedEglImage> egl_image;
    std::unique_ptr<ScopedEglSync> egl_sync;
    std::unique_ptr<ScopedTexture> texture;
#if defined(USE_VULKAN)
    std::unique_ptr<ScopedVkDeviceMemory> vk_memory;
    std::unique_ptr<ScopedVkImage> vk_image;
    std::unique_ptr<ScopedVkImageView> vk_image_view;
    std::unique_ptr<ScopedVkFramebuffer> vk_framebuffer;
#endif  // defined(USE_VULKAN)
#endif  // defined(USE_GBM)
    std::unique_ptr<zwp_linux_buffer_params_v1> params;
    std::unique_ptr<wl_shm_pool> shm_pool;
    base::WritableSharedMemoryMapping shared_memory_mapping;
    sk_sp<SkSurface> sk_surface;
  };

  bool Init(const InitParams& params);

 protected:
  ClientBase();
  virtual ~ClientBase();
  std::unique_ptr<Buffer> CreateBuffer(const gfx::Size& size,
                                       int32_t drm_format,
                                       int32_t bo_usage);
  std::unique_ptr<Buffer> CreateDrmBuffer(const gfx::Size& size,
                                          int32_t drm_format,
                                          int32_t bo_usage,
                                          bool y_invert);
  ClientBase::Buffer* DequeueBuffer();

  // wl_output_listener
  virtual void HandleGeometry(void* data,
                              struct wl_output* wl_output,
                              int32_t x,
                              int32_t y,
                              int32_t physical_width,
                              int32_t physical_height,
                              int32_t subpixel,
                              const char* make,
                              const char* model,
                              int32_t transform);
  virtual void HandleMode(void* data,
                          struct wl_output* wl_output,
                          uint32_t flags,
                          int32_t width,
                          int32_t height,
                          int32_t refresh);
  virtual void HandleDone(void* data, struct wl_output* wl_output);
  virtual void HandleScale(void* data,
                           struct wl_output* wl_output,
                           int32_t factor);

  // wl_touch_listener
  virtual void HandleDown(void* data,
                          struct wl_touch* wl_touch,
                          uint32_t serial,
                          uint32_t time,
                          struct wl_surface* surface,
                          int32_t id,
                          wl_fixed_t x,
                          wl_fixed_t y);
  virtual void HandleUp(void* data,
                        struct wl_touch* wl_touch,
                        uint32_t serial,
                        uint32_t time,
                        int32_t id);
  virtual void HandleMotion(void* data,
                            struct wl_touch* wl_touch,
                            uint32_t time,
                            int32_t id,
                            wl_fixed_t x,
                            wl_fixed_t y);
  virtual void HandleFrame(void* data, struct wl_touch* wl_touch);
  virtual void HandleCancel(void* data, struct wl_touch* wl_touch);
  virtual void HandleShape(void* data,
                           struct wl_touch* wl_touch,
                           int32_t id,
                           wl_fixed_t major,
                           wl_fixed_t minor);
  virtual void HandleOrientation(void* data,
                                 struct wl_touch* wl_touch,
                                 int32_t id,
                                 wl_fixed_t orientation);

  gfx::Size size_ = gfx::Size(256, 256);
  int scale_ = 1;
  int transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  gfx::Size surface_size_ = gfx::Size(256, 256);
  bool fullscreen_ = false;
  bool transparent_background_ = false;
  bool y_invert_ = false;

  std::unique_ptr<wl_display> display_;
  std::unique_ptr<wl_registry> registry_;
  std::unique_ptr<wl_surface> surface_;
  std::unique_ptr<wl_shell_surface> shell_surface_;
  Globals globals_;
#if defined(USE_GBM)
  base::ScopedFD drm_fd_;
  std::unique_ptr<gbm_device> device_;
#if defined(USE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vk_implementation_;
  std::unique_ptr<ScopedVkInstance> vk_instance_;
  std::unique_ptr<ScopedVkDevice> vk_device_;
  std::unique_ptr<ScopedVkCommandPool> vk_command_pool_;
  std::unique_ptr<ScopedVkRenderPass> vk_render_pass_;
  VkQueue vk_queue_;
#endif
#endif
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  std::unique_ptr<ui::ScopedMakeCurrent> make_current_;
  unsigned egl_sync_type_ = 0;
  std::vector<std::unique_ptr<Buffer>> buffers_;
  sk_sp<GrContext> gr_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientBase);
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_
