// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/client_helper.h"

#include <chrome-color-management-client-protocol.h>
#include <content-type-v1-client-protocol.h>
#include <input-timestamps-unstable-v1-client-protocol.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <single-pixel-buffer-v1-client-protocol.h>
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

DEFAULT_DELETER(surface_augmenter, surface_augmenter_destroy)
DEFAULT_DELETER(overlay_prioritizer, overlay_prioritizer_destroy)
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
DEFAULT_DELETER(zaura_output_manager, zaura_output_manager_destroy)
DEFAULT_DELETER(zaura_output_manager_v2, zaura_output_manager_v2_destroy)
DEFAULT_DELETER(zaura_shell, zaura_shell_destroy)
DEFAULT_DELETER(zaura_surface, zaura_surface_destroy)
DEFAULT_DELETER(zaura_toplevel, zaura_toplevel_destroy)
DEFAULT_DELETER(zaura_output, zaura_output_destroy)
DEFAULT_DELETER(zcr_color_manager_v1, zcr_color_manager_v1_destroy)
DEFAULT_DELETER(zcr_color_management_output_v1,
                zcr_color_management_output_v1_destroy)
DEFAULT_DELETER(zcr_color_management_surface_v1,
                zcr_color_management_surface_v1_destroy)
DEFAULT_DELETER(zcr_color_space_creator_v1, zcr_color_space_creator_v1_destroy)
DEFAULT_DELETER(zcr_color_space_v1, zcr_color_space_v1_destroy)
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
DEFAULT_DELETER(wl_data_device_manager, wl_data_device_manager_destroy)
DEFAULT_DELETER(wl_data_device, wl_data_device_destroy)
DEFAULT_DELETER(wl_data_offer, wl_data_offer_destroy)
DEFAULT_DELETER(wl_data_source, wl_data_source_destroy)
DEFAULT_DELETER(wp_content_type_manager_v1, wp_content_type_manager_v1_destroy)
DEFAULT_DELETER(wp_content_type_v1, wp_content_type_v1_destroy)
DEFAULT_DELETER(wp_fractional_scale_manager_v1,
                wp_fractional_scale_manager_v1_destroy)
DEFAULT_DELETER(wp_single_pixel_buffer_manager_v1,
                wp_single_pixel_buffer_manager_v1_destroy)
DEFAULT_DELETER(wp_viewporter, wp_viewporter_destroy)
DEFAULT_DELETER(xdg_wm_base, xdg_wm_base_destroy)
DEFAULT_DELETER(zwp_text_input_manager_v1, zwp_text_input_manager_v1_destroy)
DEFAULT_DELETER(zcr_secure_output_v1, zcr_secure_output_v1_destroy)
DEFAULT_DELETER(zcr_alpha_compositing_v1, zcr_alpha_compositing_v1_destroy)
DEFAULT_DELETER(zcr_stylus_v2, zcr_stylus_v2_destroy)
DEFAULT_DELETER(zcr_pointer_stylus_v2, zcr_pointer_stylus_v2_destroy)
DEFAULT_DELETER(zcr_cursor_shapes_v1, zcr_cursor_shapes_v1_destroy)
DEFAULT_DELETER(zcr_gaming_input_v2, zcr_gaming_input_v2_destroy)
DEFAULT_DELETER(zcr_keyboard_configuration_v1,
                zcr_keyboard_configuration_v1_destroy)
DEFAULT_DELETER(zcr_keyboard_extension_v1, zcr_keyboard_extension_v1_destroy)
DEFAULT_DELETER(zwp_keyboard_shortcuts_inhibit_manager_v1,
                zwp_keyboard_shortcuts_inhibit_manager_v1_destroy)
DEFAULT_DELETER(zcr_notification_shell_v1, zcr_notification_shell_v1_destroy)
DEFAULT_DELETER(zcr_remote_shell_v1, zcr_remote_shell_v1_destroy)
DEFAULT_DELETER(zcr_remote_shell_v2, zcr_remote_shell_v2_destroy)
DEFAULT_DELETER(zcr_stylus_tools_v1, zcr_stylus_tools_v1_destroy)
DEFAULT_DELETER(zcr_text_input_extension_v1,
                zcr_text_input_extension_v1_destroy)
DEFAULT_DELETER(zcr_touchpad_haptics_v1, zcr_touchpad_haptics_v1_destroy)
DEFAULT_DELETER(zwp_pointer_gestures_v1, zwp_pointer_gestures_v1_destroy)
DEFAULT_DELETER(zwp_pointer_constraints_v1, zwp_pointer_constraints_v1_destroy)
DEFAULT_DELETER(zwp_relative_pointer_manager_v1,
                zwp_relative_pointer_manager_v1_destroy)
DEFAULT_DELETER(zxdg_decoration_manager_v1, zxdg_decoration_manager_v1_destroy)
DEFAULT_DELETER(zcr_extended_drag_v1, zcr_extended_drag_v1_destroy)
DEFAULT_DELETER(xdg_surface, xdg_surface_destroy)
DEFAULT_DELETER(xdg_toplevel, xdg_toplevel_destroy)
DEFAULT_DELETER(zxdg_output_manager_v1, zxdg_output_manager_v1_destroy)
DEFAULT_DELETER(zwp_idle_inhibit_manager_v1,
                zwp_idle_inhibit_manager_v1_destroy)
DEFAULT_DELETER(zcr_remote_surface_v1, zcr_remote_surface_v1_destroy)
DEFAULT_DELETER(zcr_remote_surface_v2, zcr_remote_surface_v2_destroy)
DEFAULT_DELETER(zcr_ui_controls_v1, zcr_ui_controls_v1_destroy)

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

VkPipeline DeleteVkPipelineTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkPipelineTraits::Free(VkPipeline pipeline) {
  vkDestroyPipeline(vk_device, pipeline, nullptr);
}

VkPipelineLayout DeleteVkPipelineLayoutTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkPipelineLayoutTraits::Free(VkPipelineLayout pipeline_layout) {
  vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
}

VkSampler DeleteVkSamplerTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkSamplerTraits::Free(VkSampler sampler) {
  vkDestroySampler(vk_device, sampler, nullptr);
}

VkDescriptorSetLayout DeleteVkDescriptorSetLayoutTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkDescriptorSetLayoutTraits::Free(
    VkDescriptorSetLayout descriptor_set_layout) {
  vkDestroyDescriptorSetLayout(vk_device, descriptor_set_layout, nullptr);
}

VkDescriptorPool DeleteVkDescriptorPoolTraits::InvalidValue() {
  return VK_NULL_HANDLE;
}
void DeleteVkDescriptorPoolTraits::Free(VkDescriptorPool descriptor_pool) {
  vkDestroyDescriptorPool(vk_device, descriptor_pool, nullptr);
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
