// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "components/exo/wayland/clients/globals.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/insets.h"
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

class GrDirectContext;

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
    bool has_transform = false;
    bool fullscreen = false;
    bool transparent_background = false;
    bool use_drm = false;
    std::string use_drm_value;
    int32_t drm_format = 0;
    int32_t bo_usage = 0;
    bool y_invert = false;
    bool allocate_buffers_with_output_mode = false;
    bool use_fullscreen_shell = false;
    bool use_memfd = false;
    bool use_touch = false;
    bool use_vulkan = false;
    bool use_wl_shell = false;
    bool use_release_fences = false;
    bool use_stylus = false;
    std::optional<std::string> wayland_socket = {};
    uint32_t linux_dmabuf_version = ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION;
    bool enable_vulkan_debug = false;
    // by default clients only clear buffers with a solid color, this flag is
    // meant to ensure that the rendering actually draws a textured quad.
    bool use_vulkan_texture = false;
    bool use_vulkan_blitter = false;
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
    base::SharedMemoryMapping shared_memory_mapping;
    sk_sp<SkSurface> sk_surface;
  };

  ClientBase(const ClientBase&) = delete;
  ClientBase& operator=(const ClientBase&) = delete;

  bool Init(const InitParams& params);

 protected:
  ClientBase();
  virtual ~ClientBase();
  std::unique_ptr<Buffer> CreateBuffer(const gfx::Size& size,
                                       int32_t drm_format,
                                       int32_t bo_usage,
                                       bool add_buffer_listener = true,
                                       bool use_vulkan = false);
  std::unique_ptr<Buffer> CreateDrmBuffer(const gfx::Size& size,
                                          int32_t drm_format,
                                          const uint64_t* modifiers,
                                          const unsigned int modifiers_count,
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

  // zwp_linux_dmabuf_v1_listener
  virtual void HandleDmabufFormat(
      void* data,
      struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
      uint32_t format);
  virtual void HandleDmabufModifier(
      void* data,
      struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
      uint32_t format,
      uint32_t modifier_hi,
      uint32_t modifier_lo);

  // zaura_output_listener
  virtual void HandleInsets(const gfx::Insets& insets);
  virtual void HandleLogicalTransform(int32_t transform);

  gfx::Size size_ = gfx::Size(256, 256);
  int scale_ = 1;
  int transform_ = WL_OUTPUT_TRANSFORM_NORMAL;
  bool has_transform_ = false;
  gfx::Size surface_size_ = gfx::Size(256, 256);
  bool fullscreen_ = false;
  bool use_memfd_ = false;
  bool transparent_background_ = false;
  bool y_invert_ = false;

  std::unique_ptr<wl_display> display_;
  std::unique_ptr<wl_surface> surface_;
  std::unique_ptr<wl_shell_surface> shell_surface_;
  std::unique_ptr<xdg_surface> xdg_surface_;
  std::unique_ptr<xdg_toplevel> xdg_toplevel_;
  std::unique_ptr<wl_pointer> wl_pointer_;
  std::unique_ptr<zcr_pointer_stylus_v2> zcr_pointer_stylus_;
  Globals globals_;
#if defined(USE_GBM)
  base::ScopedFD drm_fd_;
  std::unique_ptr<gbm_device> device_;
  raw_ptr<gl::GLDisplayEGL> egl_display_ = nullptr;
#if defined(USE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vk_implementation_;
  std::unique_ptr<ScopedVkInstance> vk_instance_;
  std::unique_ptr<ScopedVkDevice> vk_device_;
  std::unique_ptr<ScopedVkDescriptorPool> vk_descriptor_pool_;
  std::unique_ptr<ScopedVkPipeline> vk_pipeline_;
  std::unique_ptr<ScopedVkDescriptorSetLayout> vk_descriptor_set_layout_;
  std::unique_ptr<ScopedVkCommandPool> vk_command_pool_;
  std::unique_ptr<ScopedVkRenderPass> vk_render_pass_;
  std::unique_ptr<ScopedVkPipelineLayout> vk_pipeline_layout_;
  std::unique_ptr<ScopedVkImage> vk_texture_image_;
  std::unique_ptr<ScopedVkDeviceMemory> vk_texture_image_memory_;
  std::unique_ptr<ScopedVkImageView> vk_texture_image_view_;
  std::unique_ptr<ScopedVkSampler> vk_texture_sampler_;
  std::vector<VkDescriptorSet> vk_descriptor_sets_;
  // A command pool for the transfer queue (blitter)
  std::unique_ptr<ScopedVkCommandPool> vk_command_pool_transfer_;
  VkQueue vk_queue_;
  VkQueue vk_queue_transfer_;
#endif
#endif
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  std::unique_ptr<ui::ScopedMakeCurrent> make_current_;
  unsigned egl_sync_type_ = 0;
  std::vector<std::unique_ptr<Buffer>> buffers_;
  sk_sp<GrDirectContext> gr_context_;
  base::flat_set<uint32_t> bug_fix_ids_;

 private:
  void SetupAuraShellIfAvailable();
  void SetupPointerStylus();
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_BASE_H_
