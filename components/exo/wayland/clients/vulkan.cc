// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/stl_util.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* callback_pending = static_cast<bool*>(data);
  *callback_pending = false;
}

}  // namespace

class VulkanClient : ClientBase {
 public:
  VulkanClient() {}

  void Run(const ClientBase::InitParams& params);

 private:
  friend class ScopedVulkanRenderFrame;

  DISALLOW_COPY_AND_ASSIGN(VulkanClient);
};

// ScopedVulkanRenderFrame class helps setting up all the state needed to begin
// a new frame using vulkan, it creates a command buffer and starts a render
// pass. When destroyed it takes care of submitting to the queue and to wait for
// the work to be done.
class ScopedVulkanRenderFrame {
 public:
  ScopedVulkanRenderFrame(VulkanClient* client,
                          VkFramebuffer framebuffer,
                          SkColor clear_color)
      : client_(client) {
    static const VkCommandBufferBeginInfo vk_command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VkCommandBufferAllocateInfo command_buffer_allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = client->vk_command_pool_->get(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkResult result = vkAllocateCommandBuffers(client_->vk_device_->get(),
                                               &command_buffer_allocate_info,
                                               &command_buffer_);
    CHECK_EQ(VK_SUCCESS, result) << "Failed to create a Vulkan command buffer.";

    result =
        vkBeginCommandBuffer(command_buffer_, &vk_command_buffer_begin_info);
    CHECK_EQ(VK_SUCCESS, result);

    SkColor4f sk_color = SkColor4f::FromColor(clear_color);
    VkClearValue clear_value = {
        .color =
            {
                .float32 =
                    {
                        sk_color.fR, sk_color.fG, sk_color.fB, sk_color.fA,
                    },
            },
    };
    VkRenderPassBeginInfo render_pass_begin_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = client_->vk_render_pass_->get(),
        .framebuffer = framebuffer,
        .renderArea =
            (VkRect2D){
                .offset = {0, 0},
                .extent = {client_->surface_size_.width(),
                           client_->surface_size_.height()},
            },
        .clearValueCount = 1,
        .pClearValues = &clear_value,
    };
    vkCmdBeginRenderPass(command_buffer_, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
  }
  ~ScopedVulkanRenderFrame() {
    vkCmdEndRenderPass(command_buffer_);

    VkResult result = vkEndCommandBuffer(command_buffer_);
    CHECK_EQ(VK_SUCCESS, result);
    VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers = &command_buffer_};
    result = vkQueueSubmit(client_->vk_queue_, 1, &submit_info, VK_NULL_HANDLE);
    CHECK_EQ(VK_SUCCESS, result);

    result = vkQueueWaitIdle(client_->vk_queue_);

    vkFreeCommandBuffers(client_->vk_device_->get(),
                         client_->vk_command_pool_->get(), 1, &command_buffer_);
  }

 private:
  VulkanClient* const client_;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;

  DISALLOW_COPY_AND_ASSIGN(ScopedVulkanRenderFrame);
};

void VulkanClient::Run(const ClientBase::InitParams& params) {
  if (!ClientBase::Init(params))
    return;

  bool callback_pending = false;
  std::unique_ptr<wl_callback> frame_callback;
  wl_callback_listener frame_listener = {FrameCallback};

  size_t frame_count = 0;
  do {
    if (callback_pending)
      continue;

    Buffer* buffer = DequeueBuffer();
    if (!buffer)
      continue;

    {
      static const SkColor kColors[] = {SK_ColorRED, SK_ColorBLACK,
                                        SK_ColorGREEN};

      ScopedVulkanRenderFrame vulkan_frame(
          this, buffer->vk_framebuffer->get(),
          kColors[++frame_count % base::size(kColors)]);

      // This is where the drawing code would go.
      // This client is not drawing anything. Just clearing the fb.
    }
    ++frame_count;

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &callback_pending);
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

  params.use_vulkan = true;
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::VulkanClient client;
  client.Run(params);
  return 1;
}
