// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_CLIENT_HELPER_H_

#include <alpha-compositing-unstable-v1-client-protocol.h>
#include <aura-output-management-client-protocol.h>
#include <aura-shell-client-protocol.h>
#include <chrome-color-management-client-protocol.h>
#include <content-type-v1-client-protocol.h>
#include <cursor-shapes-unstable-v1-client-protocol.h>
#include <extended-drag-unstable-v1-client-protocol.h>
#include <fractional-scale-v1-client-protocol.h>
#include <fullscreen-shell-unstable-v1-client-protocol.h>
#include <gaming-input-unstable-v2-client-protocol.h>
#include <idle-inhibit-unstable-v1-client-protocol.h>
#include <input-timestamps-unstable-v1-client-protocol.h>
#include <keyboard-configuration-unstable-v1-client-protocol.h>
#include <keyboard-extension-unstable-v1-client-protocol.h>
#include <keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <notification-shell-unstable-v1-client-protocol.h>
#include <overlay-prioritizer-client-protocol.h>
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <pointer-gestures-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#include <remote-shell-unstable-v1-client-protocol.h>
#include <remote-shell-unstable-v2-client-protocol.h>
#include <secure-output-unstable-v1-client-protocol.h>
#include <single-pixel-buffer-v1-client-protocol.h>
#include <stylus-tools-unstable-v1-client-protocol.h>
#include <stylus-unstable-v2-client-protocol.h>
#include <surface-augmenter-client-protocol.h>
#include <text-input-extension-unstable-v1-client-protocol.h>
#include <text-input-unstable-v1-client-protocol.h>
#include <touchpad-haptics-unstable-v1-client-protocol.h>
#include <ui-controls-unstable-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
#include <vsync-feedback-unstable-v1-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-output-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>
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

DEFAULT_DELETER_FDECL(surface_augmenter)
DEFAULT_DELETER_FDECL(overlay_prioritizer)
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
DEFAULT_DELETER_FDECL(zaura_output_manager)
DEFAULT_DELETER_FDECL(zaura_output_manager_v2)
DEFAULT_DELETER_FDECL(zaura_shell)
DEFAULT_DELETER_FDECL(zaura_surface)
DEFAULT_DELETER_FDECL(zaura_toplevel)
DEFAULT_DELETER_FDECL(zaura_output)
DEFAULT_DELETER_FDECL(zcr_color_manager_v1)
DEFAULT_DELETER_FDECL(zcr_color_management_output_v1)
DEFAULT_DELETER_FDECL(zcr_color_management_surface_v1)
DEFAULT_DELETER_FDECL(zcr_color_space_creator_v1)
DEFAULT_DELETER_FDECL(zcr_color_space_v1)
DEFAULT_DELETER_FDECL(zwp_linux_buffer_release_v1)
DEFAULT_DELETER_FDECL(zwp_fullscreen_shell_v1)
DEFAULT_DELETER_FDECL(zwp_input_timestamps_manager_v1)
DEFAULT_DELETER_FDECL(zwp_input_timestamps_v1)
DEFAULT_DELETER_FDECL(zwp_linux_buffer_params_v1)
DEFAULT_DELETER_FDECL(zwp_linux_dmabuf_v1)
DEFAULT_DELETER_FDECL(zwp_linux_explicit_synchronization_v1)
DEFAULT_DELETER_FDECL(zwp_linux_surface_synchronization_v1)
DEFAULT_DELETER_FDECL(wp_single_pixel_buffer_manager_v1)
DEFAULT_DELETER_FDECL(zcr_vsync_feedback_v1)
DEFAULT_DELETER_FDECL(zcr_vsync_timing_v1)
DEFAULT_DELETER_FDECL(wl_data_device_manager)
DEFAULT_DELETER_FDECL(wl_data_device)
DEFAULT_DELETER_FDECL(wl_data_source)
DEFAULT_DELETER_FDECL(wl_data_offer)
DEFAULT_DELETER_FDECL(wp_content_type_manager_v1)
DEFAULT_DELETER_FDECL(wp_content_type_v1)
DEFAULT_DELETER_FDECL(wp_fractional_scale_manager_v1)
DEFAULT_DELETER_FDECL(wp_viewporter)
DEFAULT_DELETER_FDECL(xdg_wm_base)
DEFAULT_DELETER_FDECL(zwp_text_input_manager_v1)
DEFAULT_DELETER_FDECL(zcr_secure_output_v1)
DEFAULT_DELETER_FDECL(zcr_alpha_compositing_v1)
DEFAULT_DELETER_FDECL(zcr_stylus_v2)
DEFAULT_DELETER_FDECL(zcr_pointer_stylus_v2)
DEFAULT_DELETER_FDECL(zcr_cursor_shapes_v1)
DEFAULT_DELETER_FDECL(zcr_gaming_input_v2)
DEFAULT_DELETER_FDECL(zcr_keyboard_configuration_v1)
DEFAULT_DELETER_FDECL(zcr_keyboard_extension_v1)
DEFAULT_DELETER_FDECL(zwp_keyboard_shortcuts_inhibit_manager_v1)
DEFAULT_DELETER_FDECL(zcr_notification_shell_v1)
DEFAULT_DELETER_FDECL(zcr_remote_shell_v1)
DEFAULT_DELETER_FDECL(zcr_remote_shell_v2)
DEFAULT_DELETER_FDECL(zcr_stylus_tools_v1)
DEFAULT_DELETER_FDECL(zcr_text_input_extension_v1)
DEFAULT_DELETER_FDECL(zcr_touchpad_haptics_v1)
DEFAULT_DELETER_FDECL(zwp_pointer_gestures_v1)
DEFAULT_DELETER_FDECL(zwp_pointer_constraints_v1)
DEFAULT_DELETER_FDECL(zwp_relative_pointer_manager_v1)
DEFAULT_DELETER_FDECL(zxdg_decoration_manager_v1)
DEFAULT_DELETER_FDECL(zcr_extended_drag_v1)
DEFAULT_DELETER_FDECL(xdg_surface)
DEFAULT_DELETER_FDECL(xdg_toplevel)
DEFAULT_DELETER_FDECL(zxdg_output_manager_v1)
DEFAULT_DELETER_FDECL(zwp_idle_inhibit_manager_v1)
DEFAULT_DELETER_FDECL(zcr_remote_surface_v1)
DEFAULT_DELETER_FDECL(zcr_remote_surface_v2)
DEFAULT_DELETER_FDECL(zcr_ui_controls_v1)

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

struct DeleteVkPipelineTraits {
  VkDevice vk_device;
  static VkPipeline InvalidValue();
  void Free(VkPipeline pipeline);
};
using ScopedVkPipeline =
    base::ScopedGeneric<VkPipeline, DeleteVkPipelineTraits>;

struct DeleteVkPipelineLayoutTraits {
  VkDevice vk_device;
  static VkPipelineLayout InvalidValue();
  void Free(VkPipelineLayout pipeline_layout);
};
using ScopedVkPipelineLayout =
    base::ScopedGeneric<VkPipelineLayout, DeleteVkPipelineLayoutTraits>;

struct DeleteVkSamplerTraits {
  VkDevice vk_device;
  static VkSampler InvalidValue();
  void Free(VkSampler sampler);
};
using ScopedVkSampler = base::ScopedGeneric<VkSampler, DeleteVkSamplerTraits>;

struct DeleteVkDescriptorSetLayoutTraits {
  VkDevice vk_device;
  static VkDescriptorSetLayout InvalidValue();
  void Free(VkDescriptorSetLayout descriptor_set_layout);
};
using ScopedVkDescriptorSetLayout =
    base::ScopedGeneric<VkDescriptorSetLayout,
                        DeleteVkDescriptorSetLayoutTraits>;

struct DeleteVkDescriptorPoolTraits {
  VkDevice vk_device;
  static VkDescriptorPool InvalidValue();
  void Free(VkDescriptorPool descriptor_pool);
};
using ScopedVkDescriptorPool =
    base::ScopedGeneric<VkDescriptorPool, DeleteVkDescriptorPoolTraits>;

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
