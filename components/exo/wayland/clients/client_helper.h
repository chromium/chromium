// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_

#include <aura-shell-client-protocol.h>
#include <fullscreen-shell-unstable-v1-client-protocol.h>
#include <input-timestamps-unstable-v1-client-protocol.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <vsync-feedback-unstable-v1-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <memory>

#include "base/scoped_generic.h"

#if defined(USE_GBM)
#include <gbm.h>
#if defined(USE_VULKAN)
#include <vulkan/vulkan.h>
#endif  // defined(USE_GBM)
#endif  // defined(USE_VULKAN)

// Default deleters template specialization forward decl.
#define DEFAULT_DELETER_FDECL(TypeName) \
  namespace std {                       \
  template <>                           \
  struct default_delete<TypeName> {     \
    void operator()(TypeName* ptr);     \
  };                                    \
  }

DEFAULT_DELETER_FDECL(wl_buffer)
DEFAULT_DELETER_FDECL(wl_callback)
DEFAULT_DELETER_FDECL(wl_compositor)
DEFAULT_DELETER_FDECL(wl_display)
DEFAULT_DELETER_FDECL(wl_pointer)
DEFAULT_DELETER_FDECL(wl_region)
DEFAULT_DELETER_FDECL(wl_registry)
DEFAULT_DELETER_FDECL(wl_seat)
DEFAULT_DELETER_FDECL(wl_shell)
DEFAULT_DELETER_FDECL(wl_shell_surface)
DEFAULT_DELETER_FDECL(wl_shm)
DEFAULT_DELETER_FDECL(wl_shm_pool)
DEFAULT_DELETER_FDECL(wl_subcompositor)
DEFAULT_DELETER_FDECL(wl_subsurface)
DEFAULT_DELETER_FDECL(wl_surface)
DEFAULT_DELETER_FDECL(wl_touch)
DEFAULT_DELETER_FDECL(wl_output)
DEFAULT_DELETER_FDECL(wp_presentation)
DEFAULT_DELETER_FDECL(struct wp_presentation_feedback)
DEFAULT_DELETER_FDECL(zaura_shell)
DEFAULT_DELETER_FDECL(zaura_surface)
DEFAULT_DELETER_FDECL(zaura_output)
DEFAULT_DELETER_FDECL(zwp_linux_buffer_release_v1)
DEFAULT_DELETER_FDECL(zwp_fullscreen_shell_v1)
DEFAULT_DELETER_FDECL(zwp_input_timestamps_manager_v1)
DEFAULT_DELETER_FDECL(zwp_input_timestamps_v1)
DEFAULT_DELETER_FDECL(zwp_linux_buffer_params_v1)
DEFAULT_DELETER_FDECL(zwp_linux_dmabuf_v1)
DEFAULT_DELETER_FDECL(zwp_linux_explicit_synchronization_v1)
DEFAULT_DELETER_FDECL(zwp_linux_surface_synchronization_v1)
DEFAULT_DELETER_FDECL(zcr_vsync_feedback_v1)
DEFAULT_DELETER_FDECL(zcr_vsync_timing_v1)

#if defined(USE_GBM)
DEFAULT_DELETER_FDECL(gbm_bo)
DEFAULT_DELETER_FDECL(gbm_device)
#endif

namespace exo {
namespace wayland {
namespace clients {

#if defined(USE_GBM)
struct DeleteTextureTraits {
  static unsigned InvalidValue();
  static void Free(unsigned texture);
};
using ScopedTexture = base::ScopedGeneric<unsigned, DeleteTextureTraits>;

struct DeleteEglImageTraits {
  static void* InvalidValue();
  static void Free(void* image);
};
using ScopedEglImage = base::ScopedGeneric<void*, DeleteEglImageTraits>;

struct DeleteEglSyncTraits {
  static void* InvalidValue();
  static void Free(void* sync);
};
using ScopedEglSync = base::ScopedGeneric<void*, DeleteEglSyncTraits>;

#if defined(USE_VULKAN)
struct DeleteVkInstanceTraits {
  static VkInstance InvalidValue();
  static void Free(VkInstance instance);
};
using ScopedVkInstance =
    base::ScopedGeneric<VkInstance, DeleteVkInstanceTraits>;

struct DeleteVkDeviceTraits {
  static VkDevice InvalidValue();
  static void Free(VkDevice device);
};
using ScopedVkDevice = base::ScopedGeneric<VkDevice, DeleteVkDeviceTraits>;

struct DeleteVkCommandPoolTraits {
  VkDevice vk_device;
  static VkCommandPool InvalidValue();
  void Free(VkCommandPool command_pool);
};
using ScopedVkCommandPool =
    base::ScopedGeneric<VkCommandPool, DeleteVkCommandPoolTraits>;

struct DeleteVkRenderPassTraits {
  VkDevice vk_device;
  static VkRenderPass InvalidValue();
  void Free(VkRenderPass render_pass);
};
using ScopedVkRenderPass =
    base::ScopedGeneric<VkRenderPass, DeleteVkRenderPassTraits>;

struct DeleteVkDeviceMemoryTraits {
  VkDevice vk_device;
  static VkDeviceMemory InvalidValue();
  void Free(VkDeviceMemory device_memory);
};
using ScopedVkDeviceMemory =
    base::ScopedGeneric<VkDeviceMemory, DeleteVkDeviceMemoryTraits>;

struct DeleteVkImageTraits {
  VkDevice vk_device;
  static VkImage InvalidValue();
  void Free(VkImage image);
};
using ScopedVkImage = base::ScopedGeneric<VkImage, DeleteVkImageTraits>;

struct DeleteVkImageViewTraits {
  VkDevice vk_device;
  static VkImageView InvalidValue();
  void Free(VkImageView image_view);
};
using ScopedVkImageView =
    base::ScopedGeneric<VkImageView, DeleteVkImageViewTraits>;

struct DeleteVkFramebufferTraits {
  VkDevice vk_device;
  static VkFramebuffer InvalidValue();
  void Free(VkFramebuffer framebuffer);
};
using ScopedVkFramebuffer =
    base::ScopedGeneric<VkFramebuffer, DeleteVkFramebufferTraits>;

#endif  // defined(USE_VULKAN)
#endif  // defined(USE_GBM)

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_
