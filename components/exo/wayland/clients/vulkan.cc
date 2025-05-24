// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <drm_fourcc.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/client_base.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

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
  VulkanClient() = default;

  VulkanClient(const VulkanClient&) = delete;
  VulkanClient& operator=(const VulkanClient&) = delete;

  void Run(const ClientBase::InitParams& params);

 private:
  friend class ScopedVulkanRenderFrame;
};

// ScopedVulkanRenderFrame class helps setting up all the state needed to begin
// a new frame using vulkan, it creates a command buffer and starts a render
// pass. When destroyed it takes care of submitting to the queue and to wait for
// the work to be done.
class ScopedVulkanRenderFrame {
 public:
  ScopedVulkanRenderFrame(VulkanClient* client,
                          VkFramebuffer framebuffer,
                          SkColor clear_color,
                          int frame,
                          bool use_texture,
                          VkImage blitting_target)
      : client_(client) {
    use_blitter_ = use_texture && blitting_target != VK_NULL_HANDLE;
    vk_command_pool_ = use_blitter_ ? client->vk_command_pool_transfer_->get()
                                    : client->vk_command_pool_->get();
    static const VkCommandBufferBeginInfo vk_command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VkCommandBufferAllocateInfo command_buffer_allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk_command_pool_,
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

    if (!use_blitter_) {
      SkColor4f sk_color = SkColor4f::FromColor(clear_color);
      VkClearValue clear_value = {
          .color =
              {
                  .float32 =
                      {
                          sk_color.fR,
                          sk_color.fG,
                          sk_color.fB,
                          sk_color.fA,
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
                  .extent =
                      {static_cast<uint32_t>(client_->surface_size_.width()),
                       static_cast<uint32_t>(client_->surface_size_.height())},
              },
          .clearValueCount = 1,
          .pClearValues = &clear_value,
      };

      vkCmdBeginRenderPass(command_buffer_, &render_pass_begin_info,
                           VK_SUBPASS_CONTENTS_INLINE);

      if (use_texture) {
        vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          client_->vk_pipeline_->get());
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = 256.0f,
            .height = 256.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(command_buffer_, 0, 1, &viewport);

        VkRect2D scissor{
            .offset = {0, 0},
            .extent = {256, 256},
        };
        vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
        vkCmdBindDescriptorSets(
            command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
            client_->vk_pipeline_layout_->get(), 0, 1,
            &(client_->vk_descriptor_sets_[frame]), 0, nullptr);
        vkCmdDraw(command_buffer_, 6, 1, 0, 0);
      }
    } else {
      VkImageCopy imageCopy{
          .srcSubresource{
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .layerCount = 1,
          },
          .dstSubresource{
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .layerCount = 1,
          },
          .extent{
              .width = 256,
              .height = 256,
              .depth = 4,
          },
      };
      vkCmdCopyImage(command_buffer_, client_->vk_texture_image_->get(),
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, blitting_target,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
    }
  }

  ScopedVulkanRenderFrame(const ScopedVulkanRenderFrame&) = delete;
  ScopedVulkanRenderFrame& operator=(const ScopedVulkanRenderFrame&) = delete;

  ~ScopedVulkanRenderFrame() {
    if (!use_blitter_) {
      vkCmdEndRenderPass(command_buffer_);
    }
    VkResult result = vkEndCommandBuffer(command_buffer_);
    CHECK_EQ(VK_SUCCESS, result);
    VkSubmitInfo submit_info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers = &command_buffer_};

    VkQueue vk_queue =
        use_blitter_ ? client_->vk_queue_transfer_ : client_->vk_queue_;
    result = vkQueueSubmit(vk_queue, 1, &submit_info, VK_NULL_HANDLE);
    CHECK_EQ(VK_SUCCESS, result);

    result = vkQueueWaitIdle(vk_queue);

    vkFreeCommandBuffers(client_->vk_device_->get(), vk_command_pool_, 1,
                         &command_buffer_);
  }

 private:
  const raw_ptr<VulkanClient> client_;
  VkCommandBuffer command_buffer_ = VK_NULL_HANDLE;
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;
  bool use_blitter_ = false;
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

      int frame_parity = frame_count % 2;
      ScopedVulkanRenderFrame vulkan_frame(
          this, buffer->vk_framebuffer->get(),
          kColors[frame_count % std::size(kColors)], frame_parity,
          params.use_vulkan_texture,
          params.use_vulkan_blitter ? buffer->vk_image->get() : VK_NULL_HANDLE);

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
  params.drm_format = DRM_FORMAT_ABGR8888;
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::VulkanClient client;
  client.Run(params);
  return 1;
}
