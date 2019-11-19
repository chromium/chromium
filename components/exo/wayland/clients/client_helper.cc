// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/client_helper.h"

#include <input-timestamps-unstable-v1-client-protocol.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_enums.h"

#if defined(USE_GBM)
#include <gbm.h>
#if defined(USE_VULKAN)
#include "gpu/vulkan/vulkan_function_pointers.h"
#endif  // defined(USE_VULKAN)
#endif  // defined(USE_GBM)

// Convenient macro that is used to define default deleters for object
// types allowing them to be used with std::unique_ptr.
#define DEFAULT_DELETER(TypeName, DeleteFunction)            \
  namespace std {                                            \
  void default_delete<TypeName>::operator()(TypeName* ptr) { \
    DeleteFunction(ptr);                                     \
  }                                                          \
  }

DEFAULT_DELETER(wl_buffer, wl_buffer_destroy)
DEFAULT_DELETER(wl_callback, wl_callback_destroy)
DEFAULT_DELETER(wl_compositor, wl_compositor_destroy)
DEFAULT_DELETER(wl_display, wl_display_disconnect)
DEFAULT_DELETER(wl_pointer, wl_pointer_destroy)
DEFAULT_DELETER(wl_region, wl_region_destroy)
DEFAULT_DELETER(wl_registry, wl_registry_destroy)
DEFAULT_DELETER(wl_seat, wl_seat_destroy)
DEFAULT_DELETER(wl_shell, wl_shell_destroy)
DEFAULT_DELETER(wl_shell_surface, wl_shell_surface_destroy)
DEFAULT_DELETER(wl_shm, wl_shm_destroy)
DEFAULT_DELETER(wl_shm_pool, wl_shm_pool_destroy)
DEFAULT_DELETER(wl_subcompositor, wl_subcompositor_destroy)
DEFAULT_DELETER(wl_subsurface, wl_subsurface_destroy)
DEFAULT_DELETER(wl_surface, wl_surface_destroy)
DEFAULT_DELETER(wl_touch, wl_touch_destroy)
DEFAULT_DELETER(wl_output, wl_output_destroy)
DEFAULT_DELETER(wp_presentation, wp_presentation_destroy)
DEFAULT_DELETER(struct wp_presentation_feedback,
                wp_presentation_feedback_destroy)
DEFAULT_DELETER(zaura_shell, zaura_shell_destroy)
DEFAULT_DELETER(zaura_surface, zaura_surface_destroy)
DEFAULT_DELETER(zaura_output, zaura_output_destroy)
DEFAULT_DELETER(zwp_linux_buffer_release_v1,
                zwp_linux_buffer_release_v1_destroy)
DEFAULT_DELETER(zwp_fullscreen_shell_v1, zwp_fullscreen_shell_v1_destroy)
DEFAULT_DELETER(zwp_input_timestamps_manager_v1,
                zwp_input_timestamps_manager_v1_destroy)
DEFAULT_DELETER(zwp_input_timestamps_v1, zwp_input_timestamps_v1_destroy)
DEFAULT_DELETER(zwp_linux_buffer_params_v1, zwp_linux_buffer_params_v1_destroy)
DEFAULT_DELETER(zwp_linux_dmabuf_v1, zwp_linux_dmabuf_v1_destroy)
DEFAULT_DELETER(zwp_linux_explicit_synchronization_v1,
                zwp_linux_explicit_synchronization_v1_destroy)
DEFAULT_DELETER(zwp_linux_surface_synchronization_v1,
                zwp_linux_surface_synchronization_v1_destroy)
DEFAULT_DELETER(zcr_vsync_feedback_v1, zcr_vsync_feedback_v1_destroy)
DEFAULT_DELETER(zcr_vsync_timing_v1, zcr_vsync_timing_v1_destroy)

#if defined(USE_GBM)
DEFAULT_DELETER(gbm_bo, gbm_bo_destroy)
DEFAULT_DELETER(gbm_device, gbm_device_destroy)
#endif

namespace exo {
namespace wayland {
namespace clients {

#if defined(USE_GBM)
GLuint DeleteTextureTraits::InvalidValue() {
  return 0;
}
void DeleteTextureTraits::Free(GLuint texture) {
  glDeleteTextures(1, &texture);
}

EGLImageKHR DeleteEglImageTraits::InvalidValue() {
  return 0;
}
void DeleteEglImageTraits::Free(EGLImageKHR image) {
  eglDestroyImageKHR(eglGetCurrentDisplay(), image);
}

EGLSyncKHR DeleteEglSyncTraits::InvalidValue() {
  return 0;
}
void DeleteEglSyncTraits::Free(EGLSyncKHR sync) {
  eglDestroySyncKHR(eglGetCurrentDisplay(), sync);
}

#if defined(USE_VULKAN)
VkInstance DeleteVkInstanceTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkInstanceTraits::Free(VkInstance instance) {
  vkDestroyInstance(instance, nullptr);
}

VkDevice DeleteVkDeviceTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkDeviceTraits::Free(VkDevice device) {
  vkDestroyDevice(device, nullptr);
}

VkCommandPool DeleteVkCommandPoolTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkCommandPoolTraits::Free(VkCommandPool command_pool) {
  vkDestroyCommandPool(vk_device, command_pool, nullptr);
}

VkRenderPass DeleteVkRenderPassTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkRenderPassTraits::Free(VkRenderPass render_pass) {
  vkDestroyRenderPass(vk_device, render_pass, nullptr);
}

VkDeviceMemory DeleteVkDeviceMemoryTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkDeviceMemoryTraits::Free(VkDeviceMemory device_memory) {
  vkFreeMemory(vk_device, device_memory, nullptr);
}

VkImage DeleteVkImageTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkImageTraits::Free(VkImage image) {
  vkDestroyImage(vk_device, image, nullptr);
}

VkImageView DeleteVkImageViewTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkImageViewTraits::Free(VkImageView image_view) {
  vkDestroyImageView(vk_device, image_view, nullptr);
}

VkFramebuffer DeleteVkFramebufferTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkFramebufferTraits::Free(VkFramebuffer framebuffer) {
  vkDestroyFramebuffer(vk_device, framebuffer, nullptr);
}

#endif  // defined(USE_VULKAN)
#endif  // defined(USE_GBM)

}  // namespace clients
}  // namespace wayland
}  // namespace exo
