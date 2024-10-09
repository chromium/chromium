// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/clients/client_base.h"

#include <aura-shell-client-protocol.h>
#include <chrome-color-management-client-protocol.h>
#include <fcntl.h>
#include <fullscreen-shell-unstable-v1-client-protocol.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <linux-explicit-synchronization-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>
#include <stylus-unstable-v2-client-protocol.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vulkan/vulkan_core.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bits.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_GBM)
#include <drm_fourcc.h>
#include <gbm.h>
#include <xf86drm.h>

#if defined(USE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#endif
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#endif

namespace exo {
namespace wayland {
namespace clients {
namespace switches {

// Specifies the client buffer size.
const char kSize[] = "size";

// Specifies the client scale factor (ie. number of physical pixels per DIP).
const char kScale[] = "scale";

// Specifies the client transform (ie. rotation).
const char kTransform[] = "transform";

// Specifies if the background should be transparent.
const char kTransparentBackground[] = "transparent-background";

// Use drm buffer instead of shared memory.
const char kUseDrm[] = "use-drm";

// Use memfd backed buffer instead of shared memory.
const char kUseMemfd[] = "use-memfd";

// Specifies if client should be fullscreen.
const char kFullscreen[] = "fullscreen";

// Specifies if client should run with vulkan debug logs.
const char kVulkanDebug[] = "vk-debug";

// Specifies if vulkan clients should run with a texture (default is to only use
// a clear color).
const char kVulkanTexture[] = "vk-texture";

// Specifies if client should run with a blitter.
const char kVulkanBlitter[] = "vk-blitter";

// Specifies if client should y-invert the dmabuf surfaces.
const char kYInvert[] = "y-invert";

// Specifies if client should use the deprecated wl_shell instead of xdg or
// zxdg_v6.
const char kWlShell[] = "wl-shell";

// In cases where we can't easily change the environment variables, such as tast
// tests, this flag specifies the socket to pass to wl_display_connect.
//
// When attaching to exo, this will usually be: /var/run/chrome/wayland-0
const char kWaylandSocket[] = "wayland_socket";

}  // namespace switches

namespace {

// Buffer format.
const int32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;
const int32_t kShmFormatVulkan = WL_SHM_FORMAT_ABGR8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;
#if defined(USE_GBM)
const GLenum kSizedInternalFormat = GL_BGRA8_EXT;
#endif
const size_t kBytesPerPixel = 4;

#if defined(USE_GBM)
// DRI render node path template.
const char kDriRenderNodeTemplate[] = "/dev/dri/renderD%u";
#endif

ClientBase* CastToClientBase(void* data) {
  return static_cast<ClientBase*>(data);
}

class MemfdMemoryMapping : public base::SharedMemoryMapping {
 public:
  MemfdMemoryMapping(base::span<uint8_t> mapped_span)
      : base::SharedMemoryMapping(
            mapped_span,
            mapped_span.size(),
            base::UnguessableToken::Create(),
            base::SharedMemoryMapper::GetDefaultInstance()) {}
};

void BufferRelease(void* data, wl_buffer* /* buffer */) {
  ClientBase::Buffer* buffer = static_cast<ClientBase::Buffer*>(data);
  buffer->busy = false;
}

wl_buffer_listener g_buffer_listener = {BufferRelease};

#if defined(USE_GBM)
#if defined(USE_VULKAN)
uint32_t VulkanChooseGraphicsQueueFamily(VkPhysicalDevice device) {
  uint32_t properties_number = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &properties_number, nullptr);

  std::vector<VkQueueFamilyProperties> properties(properties_number);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &properties_number,
                                           properties.data());

  // Choose the first graphics queue.
  for (uint32_t i = 0; i < properties_number; ++i) {
    if ((properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      DCHECK_GT(properties[i].queueCount, 0u);
      return i;
    }
  }
  return UINT32_MAX;
}
uint32_t VulkanChooseTransferQueueFamily(VkPhysicalDevice device) {
  uint32_t properties_number = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &properties_number, nullptr);
  std::vector<VkQueueFamilyProperties> properties(properties_number);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &properties_number,
                                           properties.data());

  // Choose the first transfer queue that doesn't have graphics support.
  for (uint32_t i = 0; i < properties_number; ++i) {
    if ((properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
        !(properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      DCHECK_GT(properties[i].queueCount, 0u);
      return i;
    }
  }

  LOG(ERROR) << "failed to get transfer queue";
  return UINT32_MAX;
}

std::unique_ptr<ScopedVkInstance> CreateVkInstance(bool enable_vulkan_debug) {
  VkApplicationInfo application_info{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = nullptr,
      .applicationVersion = 0,
      .pEngineName = nullptr,
      .engineVersion = 0,
      .apiVersion = VK_MAKE_VERSION(1, 1, 0),
  };

  std::vector<const char*> enabled_layers;
  if (enable_vulkan_debug) {
    enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
  }
  VkInstanceCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .flags = 0,
      .pApplicationInfo = &application_info,
      .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
      .ppEnabledLayerNames = enabled_layers.data(),
      .enabledExtensionCount = 0,
      .ppEnabledExtensionNames = nullptr,
  };

  std::unique_ptr<ScopedVkInstance> vk_instance(new ScopedVkInstance());
  VkResult result = vkCreateInstance(
      &create_info, nullptr, ScopedVkInstance::Receiver(*vk_instance).get());
  CHECK_EQ(VK_SUCCESS, result)
      << "Failed to create a Vulkan instance. Do you have an ICD "
         "driver (e.g: "
         "/usr/share/vulkan/icd.d/intel_icd.x86_64.json). Does it "
         "point to a valid .so? Try to set export VK_LOADER_DEBUG=all "
         "for more debuggining info.";
  return vk_instance;
}

std::unique_ptr<ScopedVkDevice> CreateVkDevice(
    VkInstance vk_instance,
    uint32_t* queue_family_index,
    uint32_t* transfer_queue_family_index) {
  uint32_t physical_devices_number = 1;
  VkPhysicalDevice physical_device;
  VkResult result = vkEnumeratePhysicalDevices(
      vk_instance, &physical_devices_number, &physical_device);
  CHECK(result == VK_SUCCESS || result == VK_INCOMPLETE)
      << "Failed to enumerate physical devices.";

  *queue_family_index = VulkanChooseGraphicsQueueFamily(physical_device);
  *transfer_queue_family_index =
      VulkanChooseTransferQueueFamily(physical_device);
  CHECK_NE(UINT32_MAX, *queue_family_index);

  float priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_info{
      {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = *queue_family_index,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      },
      {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = *transfer_queue_family_index,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      }};
  const char* required_extensions[] = {
      "VK_KHR_external_memory_fd",
  };
  const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
  VkDeviceCreateInfo device_create_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 2,
      .pQueueCreateInfos = queue_info.data(),
      .enabledLayerCount = 1,
      .ppEnabledLayerNames = validation_layers,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = required_extensions,
  };

  std::unique_ptr<ScopedVkDevice> vk_device(new ScopedVkDevice());
  result = vkCreateDevice(physical_device, &device_create_info, nullptr,
                          ScopedVkDevice::Receiver(*vk_device).get());
  CHECK_EQ(VK_SUCCESS, result);
  return vk_device;
}

std::unique_ptr<ScopedVkRenderPass> CreateVkRenderPass(VkDevice vk_device) {
  VkAttachmentDescription attach_description[]{
      {
          .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
          .samples = static_cast<VkSampleCountFlagBits>(1),
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
      },
  };
  VkAttachmentReference attachment_reference[]{
      {
          .attachment = 0,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
  };
  VkSubpassDescription subpass_description[]{
      {
          .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
          .colorAttachmentCount = 1,
          .pColorAttachments = attachment_reference,
      },
  };
  VkRenderPassCreateInfo render_pass_create_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = attach_description,
      .subpassCount = 1,
      .pSubpasses = subpass_description,
  };
  std::unique_ptr<ScopedVkRenderPass> vk_render_pass(
      new ScopedVkRenderPass(VK_NULL_HANDLE, {vk_device}));
  VkResult result =
      vkCreateRenderPass(vk_device, &render_pass_create_info, nullptr,
                         ScopedVkRenderPass::Receiver(*vk_render_pass).get());
  CHECK_EQ(VK_SUCCESS, result);
  return vk_render_pass;
}

std::unique_ptr<ScopedVkCommandPool> CreateVkCommandPool(
    VkDevice vk_device,
    uint32_t queue_family_index) {
  VkCommandPoolCreateInfo command_pool_create_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
               VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queue_family_index,
  };
  std::unique_ptr<ScopedVkCommandPool> vk_command_pool(
      new ScopedVkCommandPool(VK_NULL_HANDLE, {vk_device}));
  VkResult result = vkCreateCommandPool(
      vk_device, &command_pool_create_info, nullptr,
      ScopedVkCommandPool::Receiver(*vk_command_pool).get());
  CHECK_EQ(VK_SUCCESS, result);
  return vk_command_pool;
}

static std::vector<char> ReadFile(const std::string& filename) {
  base::File file(base::FilePath(filename),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::vector<char>();
  }

  size_t fileSize = (size_t)file.GetLength();
  std::vector<char> buffer(fileSize);

  fileSize = file.Read(0, buffer.data(), fileSize);
  file.Close();

  CHECK_EQ(fileSize, buffer.size())
      << "Shader file " << filename << " can't be read properly.";

  return buffer;
}

VkShaderModule CreateShaderModule(VkDevice device,
                                  const std::string& filename) {
  VkShaderModule shader_module;
  auto shader_code = ReadFile(filename);
  VkShaderModuleCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = shader_code.size(),
      .pCode = reinterpret_cast<const uint32_t*>(shader_code.data())};

  VkResult result =
      vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
  CHECK_EQ(result, VK_SUCCESS)
      << "failed to create shader module for: " << filename;
  return shader_module;
}

std::unique_ptr<ScopedVkDescriptorSetLayout> CreateDescriptorSetLayout(
    VkDevice device) {
  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 0;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &samplerLayoutBinding;

  std::unique_ptr<ScopedVkDescriptorSetLayout> descriptor_set_layout(
      new ScopedVkDescriptorSetLayout());
  VkResult result = vkCreateDescriptorSetLayout(
      device, &layoutInfo, nullptr,
      ScopedVkDescriptorSetLayout::Receiver(*descriptor_set_layout).get());
  CHECK_EQ(result, VK_SUCCESS) << "failed to create descriptor set layout.";

  return descriptor_set_layout;
}

std::unique_ptr<ScopedVkDescriptorPool> CreateDescriptorPool(
    VkDevice device,
    uint32_t max_frames_in_flight) {
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = max_frames_in_flight;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = max_frames_in_flight;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = max_frames_in_flight;

  std::unique_ptr<ScopedVkDescriptorPool> descriptor_pool(
      new ScopedVkDescriptorPool);
  VkResult result = vkCreateDescriptorPool(
      device, &poolInfo, nullptr,
      ScopedVkDescriptorPool::Receiver(*descriptor_pool).get());
  CHECK_EQ(result, VK_SUCCESS) << "failed to create descriptor pool.";

  return descriptor_pool;
}

std::unique_ptr<ScopedVkPipelineLayout> CreatePipelineLayout(
    VkDevice device,
    VkDescriptorSetLayout descriptor_set_layout) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  std::unique_ptr<ScopedVkPipelineLayout> pipeline_layout(
      new ScopedVkPipelineLayout());
  VkResult result = vkCreatePipelineLayout(
      device, &pipelineLayoutInfo, nullptr,
      ScopedVkPipelineLayout::Receiver(*pipeline_layout).get());
  CHECK_EQ(result, VK_SUCCESS) << "failed to create pipeline layout.";

  return pipeline_layout;
}

std::unique_ptr<ScopedVkPipeline> CreateGraphicsPipeline(
    VkDevice device,
    VkRenderPass render_pass,
    VkPipelineLayout pipeline_layout,
    const gfx::Size& viewport_size) {
  VkShaderModule vert_shader_module =
      CreateShaderModule(device, "shaders/vert.spv");

  VkShaderModule frag_shader_module =
      CreateShaderModule(device, "shaders/frag.spv");

  VkPipelineShaderStageCreateInfo vert_shader_stage_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_shader_module,
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo frag_shader_stage_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_shader_module,
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info,
                                                     frag_shader_stage_info};

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data(),
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(viewport_size.width()),
      .height = static_cast<float>(viewport_size.height()),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  VkRect2D scissor{.offset = {0, 0},
                   .extent = {static_cast<uint32_t>(viewport_size.width()),
                              static_cast<uint32_t>(viewport_size.height())}};
  VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };
  VkPipelineMultisampleStateCreateInfo multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo colorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };

  VkGraphicsPipelineCreateInfo pipeline_info{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = pipeline_layout,
      .renderPass = render_pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
  };

  std::unique_ptr<ScopedVkPipeline> pipeline(new ScopedVkPipeline());

  VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
      ScopedVkPipeline::Receiver(*pipeline).get());
  CHECK_EQ(result, VK_SUCCESS) << "failed to create graphics pipeline";

  vkDestroyShaderModule(device, frag_shader_module, nullptr);
  vkDestroyShaderModule(device, vert_shader_module, nullptr);

  return pipeline;
}

////////////////////////////////////////////////////////////////////////////////
// Texture

uint32_t FindMemoryType(VkInstance vk_instance,
                        int32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
  uint32_t physical_devices_number = 1;
  VkPhysicalDevice physical_device;
  VkResult result = vkEnumeratePhysicalDevices(
      vk_instance, &physical_devices_number, &physical_device);
  CHECK_EQ(result, VK_SUCCESS)
      << "failed to find suitable physical device type";

  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  LOG(ERROR) << "failed to find suitable memory type";
  return 0;
}

void CreateBuffer(VkInstance vk_instance,
                  VkDevice device,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& bufferMemory) {
  VkBufferCreateInfo bufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
  CHECK_EQ(result, VK_SUCCESS) << "failed to create buffer.";

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = FindMemoryType(
          vk_instance, memRequirements.memoryTypeBits, properties),
  };

  result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
  CHECK_EQ(result, VK_SUCCESS) << "failed to allocate buffer memory";

  vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer BeginSingleTimeCommands(VkDevice device,
                                        VkCommandPool command_pool) {
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(device, &allocInfo, &command_buffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &beginInfo);

  return command_buffer;
}

void EndSingleTimeCommands(VkDevice device,
                           VkCommandPool command_pool,
                           VkQueue queue,
                           VkCommandBuffer command_buffer) {
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };
  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

void TransitionImageLayout(VkDevice device,
                           VkCommandPool command_pool,
                           VkQueue queue,
                           VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout) {
  VkCommandBuffer command_buffer =
      BeginSingleTimeCommands(device, command_pool);

  VkImageSubresourceRange subresource_range{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = subresource_range,
  };
  vkCmdPipelineBarrier(command_buffer, 0 /* TODO */, 0 /* TODO */, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  EndSingleTimeCommands(device, command_pool, queue, command_buffer);
}

void CopyBufferToImage(VkDevice device,
                       VkCommandPool command_pool,
                       VkQueue queue,
                       VkBuffer buffer,
                       VkImage image,
                       uint32_t width,
                       uint32_t height) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands(device, command_pool);

  VkBufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource{
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1},
  };

  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  EndSingleTimeCommands(device, command_pool, queue, commandBuffer);
}

void CreateTexture(VkInstance vk_instance,
                   VkDevice device,
                   VkCommandPool command_pool,
                   VkQueue queue,
                   gfx::Size size,
                   VkImage* textureImage,
                   VkDeviceMemory* textureImageMemory) {
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  VkDeviceSize imageSize = size.width() * size.height() * 4;

  CreateBuffer(vk_instance, device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
  memset(data, 128, static_cast<size_t>(imageSize));
  vkUnmapMemory(device, stagingBufferMemory);

  VkImageCreateInfo imageInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
      .extent{
          .width = static_cast<uint32_t>(size.width()),
          .height = static_cast<uint32_t>(size.height()),
          .depth = 1,
      },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkResult result = vkCreateImage(device, &imageInfo, nullptr, textureImage);
  CHECK_EQ(result, VK_SUCCESS) << "failed to create image";

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, *textureImage, &memRequirements);

  VkMemoryAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          FindMemoryType(vk_instance, memRequirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };
  result = vkAllocateMemory(device, &allocInfo, nullptr, textureImageMemory);
  CHECK_EQ(result, VK_SUCCESS) << "failed to allocate image memory";

  result = vkBindImageMemory(device, *textureImage, *textureImageMemory, 0);
  CHECK_EQ(result, VK_SUCCESS) << "failed to bind image memory";

  TransitionImageLayout(device, command_pool, queue, *textureImage,
                        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CopyBufferToImage(device, command_pool, queue, stagingBuffer, *textureImage,
                    size.width(), size.height());

  TransitionImageLayout(device, command_pool, queue, *textureImage,
                        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);
}

std::unique_ptr<ScopedVkImageView> CreateImageView(VkDevice device,
                                                   VkImage image,
                                                   VkFormat format) {
  VkImageViewCreateInfo viewInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange{
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };

  std::unique_ptr<ScopedVkImageView> imageView(new ScopedVkImageView());
  VkResult result =
      vkCreateImageView(device, &viewInfo, nullptr,
                        ScopedVkImageView::Receiver(*imageView).get());
  CHECK_EQ(result, VK_SUCCESS) << "failed to create texture image view";

  return imageView;
}

std::unique_ptr<ScopedVkSampler> CreateTextureSampler(VkDevice device) {
  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0f,
  };
  std::unique_ptr<ScopedVkSampler> textureSampler(new ScopedVkSampler());
  VkResult result =
      vkCreateSampler(device, &samplerInfo, nullptr,
                      ScopedVkSampler::Receiver(*textureSampler).get());
  CHECK_EQ(result, VK_SUCCESS) << "failed to create texture sampler";

  return textureSampler;
}

std::vector<VkDescriptorSet> CreateDescriptorSets(
    VkDevice device,
    VkImageView textureImageView,
    VkSampler textureSampler,
    VkDescriptorSetLayout descriptorSetLayout,
    VkDescriptorPool descriptorPool,
    size_t max_frames_in_flight) {
  std::vector<VkDescriptorSetLayout> layouts(max_frames_in_flight,
                                             descriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(max_frames_in_flight);
  allocInfo.pSetLayouts = layouts.data();

  std::vector<VkDescriptorSet> descriptor_sets(max_frames_in_flight);
  VkResult result =
      vkAllocateDescriptorSets(device, &allocInfo, descriptor_sets.data());
  CHECK_EQ(result, VK_SUCCESS) << "failed to allocate descriptor sets";

  for (size_t i = 0; i < max_frames_in_flight; i++) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView;
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptorWrites;

    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = descriptor_sets[i];
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);
  }
  return descriptor_sets;
}

#endif  // defined(USE_VULKAN)
#endif  // defined(USE_GBM)

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClientBase::InitParams, public:

ClientBase::InitParams::InitParams() {
#if defined(USE_GBM)
  drm_format = DRM_FORMAT_ARGB8888;
  bo_usage = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING | GBM_BO_USE_TEXTURING;
#endif
}

ClientBase::InitParams::~InitParams() {}

ClientBase::InitParams::InitParams(const InitParams& params) = default;

bool ClientBase::InitParams::FromCommandLine(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSize)) {
    std::string size_str = command_line.GetSwitchValueASCII(switches::kSize);
    if (sscanf(size_str.c_str(), "%zdx%zd", &width, &height) != 2) {
      LOG(ERROR) << "Invalid value for " << switches::kSize;
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kScale) &&
      !base::StringToInt(command_line.GetSwitchValueASCII(switches::kScale),
                         &scale)) {
    LOG(ERROR) << "Invalid value for " << switches::kScale;
    return false;
  }

  if (command_line.HasSwitch(switches::kTransform)) {
    std::string transform_str =
        command_line.GetSwitchValueASCII(switches::kTransform);
    if (transform_str == "0") {
      transform = WL_OUTPUT_TRANSFORM_NORMAL;
    } else if (transform_str == "90") {
      transform = WL_OUTPUT_TRANSFORM_90;
    } else if (transform_str == "180") {
      transform = WL_OUTPUT_TRANSFORM_180;
    } else if (transform_str == "270") {
      transform = WL_OUTPUT_TRANSFORM_270;
    } else {
      LOG(ERROR) << "Invalid value for " << switches::kTransform;
      return false;
    }
    has_transform = true;
  }

  use_drm = command_line.HasSwitch(switches::kUseDrm);
  if (use_drm)
    use_drm_value = command_line.GetSwitchValueASCII(switches::kUseDrm);

  use_memfd = command_line.HasSwitch(switches::kUseMemfd);

  fullscreen = command_line.HasSwitch(switches::kFullscreen);
  transparent_background =
      command_line.HasSwitch(switches::kTransparentBackground);

  y_invert = command_line.HasSwitch(switches::kYInvert);

  use_wl_shell = command_line.HasSwitch(switches::kWlShell);

  if (command_line.HasSwitch(switches::kWaylandSocket))
    wayland_socket = command_line.GetSwitchValueASCII(switches::kWaylandSocket);

  enable_vulkan_debug = command_line.HasSwitch(switches::kVulkanDebug);

  use_vulkan_texture = command_line.HasSwitch(switches::kVulkanTexture);
  use_vulkan_blitter = command_line.HasSwitch(switches::kVulkanBlitter);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ClientBase::Buffer, public:

ClientBase::Buffer::Buffer() {}

ClientBase::Buffer::~Buffer() {}

////////////////////////////////////////////////////////////////////////////////
// ClientBase, public:

bool ClientBase::Init(const InitParams& params) {
  size_.SetSize(params.width, params.height);
  scale_ = params.scale;
  transform_ = params.transform;
  switch (params.transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
      surface_size_.SetSize(params.width, params.height);
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
      surface_size_.SetSize(params.height, params.width);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  surface_size_ = gfx::ToCeiledSize(
      gfx::ScaleSize(gfx::SizeF(surface_size_), 1.0f / params.scale));
  fullscreen_ = params.fullscreen;
  transparent_background_ = params.transparent_background;
  y_invert_ = params.y_invert;
  has_transform_ = params.has_transform;

  display_.reset(wl_display_connect(
      params.wayland_socket ? params.wayland_socket->c_str() : nullptr));
  if (!display_) {
    LOG(ERROR) << "wl_display_connect failed";
    return false;
  }

  base::flat_map<std::string, uint32_t> requested_versions;
  requested_versions[xdg_wm_base_interface.name] =
      XDG_SURFACE_CONFIGURE_SINCE_VERSION;
  requested_versions[zwp_linux_dmabuf_v1_interface.name] =
      params.linux_dmabuf_version;
  globals_.Init(display_.get(), std::move(requested_versions));

  if (!globals_.compositor) {
    LOG(ERROR) << "Can't find compositor interface";
    return false;
  }
  if (!globals_.shm) {
    LOG(ERROR) << "Can't find shm interface";
    return false;
  }
  if (!globals_.presentation) {
    LOG(ERROR) << "Can't find presentation interface";
    return false;
  }
  if (params.use_drm && !globals_.linux_dmabuf) {
    LOG(ERROR) << "Can't find linux_dmabuf interface";
    return false;
  }
  if (!globals_.seat) {
    LOG(ERROR) << "Can't find seat interface";
    return false;
  }

#if defined(USE_GBM)
  if (params.use_drm) {
    static struct zwp_linux_dmabuf_v1_listener kLinuxDmabufListener = {
        .format =
            [](void* data, struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
               uint32_t format) {
              CastToClientBase(data)->HandleDmabufFormat(
                  data, zwp_linux_dmabuf_v1, format);
            },
        .modifier =
            [](void* data, struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
               uint32_t format, uint32_t modifier_hi, uint32_t modifier_lo) {
              CastToClientBase(data)->HandleDmabufModifier(
                  data, zwp_linux_dmabuf_v1, format, modifier_hi, modifier_lo);
            }};
    zwp_linux_dmabuf_v1_add_listener(globals_.linux_dmabuf.get(),
                                     &kLinuxDmabufListener, this);
    wl_display_roundtrip(display_.get());

    // Number of files to look for when discovering DRM devices.
    const uint32_t kDrmMaxMinor = 15;
    const uint32_t kRenderNodeStart = 128;
    const uint32_t kRenderNodeEnd = kRenderNodeStart + kDrmMaxMinor + 1;

    for (uint32_t i = kRenderNodeStart; i < kRenderNodeEnd; i++) {
      std::string dri_render_node(
          base::StringPrintf(kDriRenderNodeTemplate, i));
      base::ScopedFD drm_fd(open(dri_render_node.c_str(), O_RDWR));
      if (drm_fd.get() < 0)
        continue;

      drmVersionPtr drm_version = drmGetVersion(drm_fd.get());
      if (!drm_version) {
        LOG(ERROR) << "Can't get version for device: '" << dri_render_node
                   << "'";
        return false;
      }
      // We can't actually use the virtual GEM, so discard it like we do in CrOS
      if (base::EqualsCaseInsensitiveASCII("vgem", drm_version->name))
        continue;
      if (strstr(drm_version->name, params.use_drm_value.c_str())) {
        drm_fd_ = std::move(drm_fd);
        break;
      }
    }

    if (drm_fd_.get() < 0) {
      LOG_IF(ERROR, params.use_drm)
          << "Can't find drm device: '" << params.use_drm_value << "'";
      LOG_IF(ERROR, !params.use_drm) << "Can't find drm device";
      return false;
    }

    device_.reset(gbm_create_device(drm_fd_.get()));
    if (!device_) {
      LOG(ERROR) << "Can't create gbm device";
      return false;
    }
    ui::OzonePlatform::InitParams ozone_params;
    ozone_params.single_process = true;
    ui::OzonePlatform::InitializeForGPU(ozone_params);
    egl_display_ = static_cast<gl::GLDisplayEGL*>(gl::init::InitializeGLOneOff(
        /*gpu_preference=*/gl::GpuPreference::kDefault));
    DCHECK(egl_display_);
    gl_surface_ = gl::init::CreateOffscreenGLSurface(egl_display_, gfx::Size());
    gl_context_ =
        gl::init::CreateGLContext(nullptr,  // share_group
                                  gl_surface_.get(), gl::GLContextAttribs());

    make_current_ = std::make_unique<ui::ScopedMakeCurrent>(gl_context_.get(),
                                                            gl_surface_.get());

    // Prefer Android native fence, it is used by ExplicitSynchronizationClient.
    // If not, use an EGL sync.  When EGL_ANGLE_global_fence_sync is available,
    // it should be preferred as Chrome assumes EGL syncs synchronize with
    // submissions from all previous contexts.
    if (egl_display_->ext->b_EGL_ANDROID_native_fence_sync) {
      egl_sync_type_ = EGL_SYNC_NATIVE_FENCE_ANDROID;
    } else if (egl_display_->ext->b_EGL_ANGLE_global_fence_sync) {
      egl_sync_type_ = EGL_SYNC_GLOBAL_FENCE_ANGLE;
    } else if (egl_display_->ext->b_EGL_ARM_implicit_external_sync) {
      egl_sync_type_ = EGL_SYNC_FENCE_KHR;
    }

    sk_sp<const GrGLInterface> native_interface = GrGLMakeAssembledInterface(
        nullptr,
        [](void* ctx, const char name[]) { return eglGetProcAddress(name); });
    DCHECK(native_interface);
    gr_context_ = GrDirectContexts::MakeGL(std::move(native_interface));
    DCHECK(gr_context_);

#if defined(USE_VULKAN)
    if (params.use_vulkan) {
      vk_implementation_ = gpu::CreateVulkanImplementation();
      CHECK(vk_implementation_) << "Can't create VulkanImplementation";
      bool ret = vk_implementation_->InitializeVulkanInstance(false);
      CHECK(ret) << "Failed to initialize VulkanImplementation";
      vk_instance_ = CreateVkInstance(params.enable_vulkan_debug);
      uint32_t queue_family_index = UINT32_MAX;
      uint32_t transfer_queue_family_index = UINT32_MAX;
      vk_device_ = CreateVkDevice(vk_instance_->get(), &queue_family_index,
                                  &transfer_queue_family_index);
      CHECK(gpu::GetVulkanFunctionPointers()->BindDeviceFunctionPointers(
          vk_device_->get(), VK_VERSION_1_0, gfx::ExtensionSet()));
      vk_render_pass_ = CreateVkRenderPass(vk_device_->get());

      vkGetDeviceQueue(vk_device_->get(), queue_family_index, 0, &vk_queue_);
      vkGetDeviceQueue(vk_device_->get(), transfer_queue_family_index, 0,
                       &vk_queue_transfer_);

      vk_command_pool_ =
          CreateVkCommandPool(vk_device_->get(), queue_family_index);
      vk_command_pool_transfer_ =
          CreateVkCommandPool(vk_device_->get(), transfer_queue_family_index);

      if (params.use_vulkan_texture) {
        // pipeline initialization
        vk_descriptor_set_layout_ =
            CreateDescriptorSetLayout(vk_device_->get());
        vk_pipeline_layout_ = CreatePipelineLayout(
            vk_device_->get(), vk_descriptor_set_layout_->get());
        vk_descriptor_pool_ =
            CreateDescriptorPool(vk_device_->get(), params.num_buffers);
        vk_pipeline_ =
            CreateGraphicsPipeline(vk_device_->get(), vk_render_pass_->get(),
                                   vk_pipeline_layout_->get(), size_);

        // texture initialization
        vk_texture_image_.reset(
            new ScopedVkImage(VK_NULL_HANDLE, {vk_device_->get()}));
        vk_texture_image_memory_.reset(
            new ScopedVkDeviceMemory(VK_NULL_HANDLE, {vk_device_->get()}));
        CreateTexture(
            vk_instance_->get(), vk_device_->get(), vk_command_pool_->get(),
            vk_queue_, size_, ScopedVkImage::Receiver(*vk_texture_image_).get(),
            ScopedVkDeviceMemory::Receiver(*vk_texture_image_memory_).get());
        vk_texture_image_view_ =
            CreateImageView(vk_device_->get(), vk_texture_image_->get(),
                            VK_FORMAT_A8B8G8R8_UNORM_PACK32);
        vk_texture_sampler_ = CreateTextureSampler(vk_device_->get());
        vk_descriptor_sets_ = CreateDescriptorSets(
            vk_device_->get(), vk_texture_image_view_->get(),
            vk_texture_sampler_->get(), vk_descriptor_set_layout_->get(),
            vk_descriptor_pool_->get(), params.num_buffers);
      }
    }
#endif  // defined(USE_VULKAN)
  }
#endif  // defined(USE_GBM)
  surface_.reset(static_cast<wl_surface*>(
      wl_compositor_create_surface(globals_.compositor.get())));
  if (!surface_) {
    LOG(ERROR) << "Can't create surface";
    return false;
  }

  if (!transparent_background_) {
    std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
        wl_compositor_create_region(globals_.compositor.get())));
    if (!opaque_region) {
      LOG(ERROR) << "Can't create region";
      return false;
    }

    wl_region_add(opaque_region.get(), 0, 0, size_.width(), size_.height());
    wl_surface_set_opaque_region(surface_.get(), opaque_region.get());
  }

  use_memfd_ = params.use_memfd;

  if (params.allocate_buffers_with_output_mode) {
    static wl_output_listener kOutputListener = {
        [](void* data, struct wl_output* wl_output, int32_t x, int32_t y,
           int32_t physical_width, int32_t physical_height, int32_t subpixel,
           const char* make, const char* model, int32_t transform) {
          CastToClientBase(data)->HandleGeometry(
              data, wl_output, x, y, physical_width, physical_height, subpixel,
              make, model, transform);
        },
        [](void* data, struct wl_output* wl_output, uint32_t flags,
           int32_t width, int32_t height, int32_t refresh) {
          CastToClientBase(data)->HandleMode(data, wl_output, flags, width,
                                             height, refresh);
        },
        [](void* data, struct wl_output* wl_output) {
          CastToClientBase(data)->HandleDone(data, wl_output);
        },
        [](void* data, struct wl_output* wl_output, int32_t factor) {
          CastToClientBase(data)->HandleScale(data, wl_output, factor);
        }};

    wl_output_add_listener(globals_.outputs.back().get(), &kOutputListener,
                           this);
  } else {
    for (size_t i = 0; i < params.num_buffers; ++i) {
      auto buffer =
          CreateBuffer(size_, params.drm_format, params.bo_usage,
                       /*add_buffer_listener=*/!params.use_release_fences,
                       params.use_vulkan);
      if (!buffer) {
        LOG(ERROR) << "Failed to create buffer";
        return false;
      }
      buffers_.push_back(std::move(buffer));
    }

    for (size_t i = 0; i < buffers_.size(); ++i) {
      // If the buffer handle doesn't exist, we would either be killed by the
      // server or die here.
      if (!buffers_[i]->buffer) {
        LOG(ERROR) << "buffer handle uninitialized.";
        return false;
      }
    }
  }

  if (params.use_fullscreen_shell) {
    zwp_fullscreen_shell_v1_present_surface(globals_.fullscreen_shell.get(),
                                            surface_.get(), 0, nullptr);

  } else if (params.use_wl_shell) {
    if (!globals_.shell) {
      LOG(ERROR) << "Can't find shell interface";
      return false;
    }

    std::unique_ptr<wl_shell_surface> shell_surface(
        static_cast<wl_shell_surface*>(
            wl_shell_get_shell_surface(globals_.shell.get(), surface_.get())));
    if (!shell_surface) {
      LOG(ERROR) << "Can't get shell surface";
      return false;
    }

    wl_shell_surface_set_title(shell_surface.get(), params.title.c_str());

    SetupAuraShellIfAvailable();

    if (fullscreen_) {
      wl_shell_surface_set_fullscreen(
          shell_surface.get(), WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0,
          nullptr);
    } else {
      wl_shell_surface_set_toplevel(shell_surface.get());
    }
  } else {
    xdg_surface_.reset(xdg_wm_base_get_xdg_surface(globals_.xdg_wm_base.get(),
                                                   surface_.get()));
    if (!xdg_surface_) {
      LOG(ERROR) << "Can't get xdg surface";
      return false;
    }
    static const xdg_surface_listener xdg_surface_listener = {
        [](void* data, struct xdg_surface* xdg_surface, uint32_t layout_mode) {
          xdg_surface_ack_configure(xdg_surface, layout_mode);
        },
    };
    xdg_surface_add_listener(xdg_surface_.get(), &xdg_surface_listener, this);
    xdg_toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));
    if (!xdg_toplevel_) {
      LOG(ERROR) << "Can't get xdg toplevel";
      return false;
    }
    static const xdg_toplevel_listener xdg_toplevel_listener = {
        [](void* data, struct xdg_toplevel* xdg_toplevel, int32_t width,
           int32_t height, struct wl_array* states) {},
        [](void* data, struct xdg_toplevel* xdg_toplevel) {}};
    xdg_toplevel_add_listener(xdg_toplevel_.get(), &xdg_toplevel_listener,
                              this);

    if (fullscreen_) {
      LOG(ERROR) << "full screen not supported yet.";
      return false;
    }

    SetupAuraShellIfAvailable();

    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
  }

  if (params.use_touch) {
    static wl_touch_listener kTouchListener = {
        [](void* data, struct wl_touch* wl_touch, uint32_t serial,
           uint32_t time, struct wl_surface* surface, int32_t id, wl_fixed_t x,
           wl_fixed_t y) {
          CastToClientBase(data)->HandleDown(data, wl_touch, serial, time,
                                             surface, id, x, y);
        },
        [](void* data, struct wl_touch* wl_touch, uint32_t serial,
           uint32_t time, int32_t id) {
          CastToClientBase(data)->HandleUp(data, wl_touch, serial, time, id);
        },
        [](void* data, struct wl_touch* wl_touch, uint32_t time, int32_t id,
           wl_fixed_t x, wl_fixed_t y) {
          CastToClientBase(data)->HandleMotion(data, wl_touch, time, id, x, y);
        },
        [](void* data, struct wl_touch* wl_touch) {
          CastToClientBase(data)->HandleFrame(data, wl_touch);
        },
        [](void* data, struct wl_touch* wl_touch) {
          CastToClientBase(data)->HandleCancel(data, wl_touch);
        },
        [](void* data, struct wl_touch* wl_touch, int32_t id, wl_fixed_t major,
           wl_fixed_t minor) {
          CastToClientBase(data)->HandleShape(data, wl_touch, id, major, minor);
        },
        [](void* data, struct wl_touch* wl_touch, int32_t id,
           wl_fixed_t orientation) {
          CastToClientBase(data)->HandleOrientation(data, wl_touch, id,
                                                    orientation);
        }};

    wl_touch* touch = wl_seat_get_touch(globals_.seat.get());
    wl_touch_add_listener(touch, &kTouchListener, this);
  }
  if (params.use_stylus)
    SetupPointerStylus();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ClientBase, protected:

ClientBase::ClientBase() = default;

ClientBase::~ClientBase() {
  make_current_ = nullptr;
  gl_context_ = nullptr;
  gl_surface_ = nullptr;
#if defined(USE_GBM)
  if (egl_display_) {
    gl::init::ShutdownGL(egl_display_, false);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// wl_touch_listener

void ClientBase::HandleDown(void* data,
                            struct wl_touch* wl_touch,
                            uint32_t serial,
                            uint32_t time,
                            struct wl_surface* surface,
                            int32_t id,
                            wl_fixed_t x,
                            wl_fixed_t y) {}

void ClientBase::HandleUp(void* data,
                          struct wl_touch* wl_touch,
                          uint32_t serial,
                          uint32_t time,
                          int32_t id) {}

void ClientBase::HandleMotion(void* data,
                              struct wl_touch* wl_touch,
                              uint32_t time,
                              int32_t id,
                              wl_fixed_t x,
                              wl_fixed_t y) {}

void ClientBase::HandleFrame(void* data, struct wl_touch* wl_touch) {}

void ClientBase::HandleCancel(void* data, struct wl_touch* wl_touch) {}

void ClientBase::HandleShape(void* data,
                             struct wl_touch* wl_touch,
                             int32_t id,
                             wl_fixed_t major,
                             wl_fixed_t minor) {}

void ClientBase::HandleOrientation(void* data,
                                   struct wl_touch* wl_touch,
                                   int32_t id,
                                   wl_fixed_t orientation) {}

////////////////////////////////////////////////////////////////////////////////
// wl_output_listener

void ClientBase::HandleGeometry(void* data,
                                struct wl_output* wl_output,
                                int32_t x,
                                int32_t y,
                                int32_t physical_width,
                                int32_t physical_height,
                                int32_t subpixel,
                                const char* make,
                                const char* model,
                                int32_t transform) {
  if (has_transform_)
    return;
  // |transform| describes the display transform. In order to take advantage of
  // hardware overlays, content needs to be rotated in the opposite direction to
  // show right-side up on the display.
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_90:
      transform_ = WL_OUTPUT_TRANSFORM_270;
      break;
    case WL_OUTPUT_TRANSFORM_270:
      transform_ = WL_OUTPUT_TRANSFORM_90;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      transform_ = WL_OUTPUT_TRANSFORM_FLIPPED_270;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      transform_ = WL_OUTPUT_TRANSFORM_FLIPPED_90;
      break;
    default:
      transform_ = transform;
  }
}

void ClientBase::HandleMode(void* data,
                            struct wl_output* wl_output,
                            uint32_t flags,
                            int32_t width,
                            int32_t height,
                            int32_t refresh) {}

void ClientBase::HandleDone(void* data, struct wl_output* wl_output) {}

void ClientBase::HandleScale(void* data,
                             struct wl_output* wl_output,
                             int32_t factor) {}

////////////////////////////////////////////////////////////////////////////////
// zwp_linux_dmabuf_v1_listener

void ClientBase::HandleDmabufFormat(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
    uint32_t format) {}

void ClientBase::HandleDmabufModifier(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf_v1,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo) {}

////////////////////////////////////////////////////////////////////////////////
// zaura_output_listener

void ClientBase::HandleInsets(const gfx::Insets& insets) {}

void ClientBase::HandleLogicalTransform(int32_t transform) {}

////////////////////////////////////////////////////////////////////////////////
// helper functions

std::unique_ptr<ClientBase::Buffer> ClientBase::CreateBuffer(
    const gfx::Size& size,
    int32_t drm_format,
    int32_t bo_usage,
    bool add_buffer_listener,
    bool use_vulkan) {
  std::unique_ptr<Buffer> buffer;
#if defined(USE_GBM)
  if (device_) {
    buffer = CreateDrmBuffer(size, drm_format, nullptr, 0, bo_usage, y_invert_);
    CHECK(buffer) << "Can't create drm buffer";
  }
#endif

  if (!buffer) {
    buffer = std::make_unique<Buffer>();
    size_t stride = size.width() * kBytesPerPixel;
    size_t length = size.height() * stride;
    uint8_t* mapped_data;

    if (use_memfd_) {
      // udmabuf_create requires a page aligned buffer.
      length = base::bits::AlignUp(length,
                                   base::checked_cast<size_t>(getpagesize()));
      int memfd = memfd_create("memfd", MFD_ALLOW_SEALING);
      if (memfd < 0) {
        PLOG(ERROR) << "memfd_create failed";
        return nullptr;
      }

      // Truncate the chunk of memory to be page aligned so that the server
      // has the option of using the shared memory region as a dma-buf through
      // udmabuf_create.
      int res = HANDLE_EINTR(ftruncate(memfd, length));
      if (res < 0) {
        PLOG(ERROR) << "ftruncate failed";
        return nullptr;
      }

      // Seal the fd with F_SEAL_SHRINK so that the server has the option of
      // using the shared memory region as a dma-buf through udmabuf_create.
      if (fcntl(memfd, F_ADD_SEALS, F_SEAL_SHRINK) < 0) {
        PLOG(ERROR) << "Failed to seal memfd";
        return nullptr;
      }
      mapped_data = static_cast<uint8_t*>(
          mmap(nullptr, length, PROT_WRITE | PROT_READ, MAP_SHARED, memfd, 0));

      if (mapped_data == MAP_FAILED) {
        PLOG(ERROR) << "Failed to mmap";
        return nullptr;
      }

      base::span<uint8_t> mapped_span = base::make_span(mapped_data, length);
      buffer->shared_memory_mapping = MemfdMemoryMapping(mapped_span);
      buffer->shm_pool.reset(
          wl_shm_create_pool(globals_.shm.get(), memfd, length));

      close(memfd);

    } else {
      base::UnsafeSharedMemoryRegion shared_memory_region =
          base::UnsafeSharedMemoryRegion::Create(length);

      if (!shared_memory_region.IsValid()) {
        LOG(ERROR) << "Shared Memory Region is not valid";
        return nullptr;
      }

      base::WritableSharedMemoryMapping map = shared_memory_region.Map();

      if (!map.IsValid()) {
        LOG(ERROR) << "WritableSharedMemoryMapping is not valid";
        return nullptr;
      }

      mapped_data = map.GetMemoryAs<uint8_t>();
      buffer->shared_memory_mapping = std::move(map);

      base::subtle::PlatformSharedMemoryRegion platform_shared_memory =
          base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
              std::move(shared_memory_region));

      buffer->shm_pool.reset(wl_shm_create_pool(
          globals_.shm.get(), platform_shared_memory.GetPlatformHandle().fd,
          buffer->shared_memory_mapping.size()));
    }

    int32_t format = use_vulkan ? kShmFormatVulkan : kShmFormat;

    buffer->buffer.reset(static_cast<wl_buffer*>(
        wl_shm_pool_create_buffer(buffer->shm_pool.get(), 0, size.width(),
                                  size.height(), stride, format)));
    if (!buffer->buffer) {
      LOG(ERROR) << "Can't create buffer";
      return nullptr;
    }

    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    buffer->sk_surface = SkSurfaces::WrapPixels(
        SkImageInfo::Make(size.width(), size.height(), kColorType,
                          kOpaque_SkAlphaType),
        mapped_data, stride, &props);
    DCHECK(buffer->sk_surface);
  }

  if (add_buffer_listener) {
    wl_buffer_add_listener(buffer->buffer.get(), &g_buffer_listener,
                           buffer.get());
  }
  return buffer;
}

VkResult BindImageBo(VkDevice dev,
                     VkImage image,
                     struct gbm_bo* bo,
                     VkDeviceMemory* mems) {
  VkResult result = VK_ERROR_UNKNOWN;
  PFN_vkGetMemoryFdPropertiesKHR bs_vkGetMemoryFdPropertiesKHR =
      reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
          vkGetDeviceProcAddr(dev, "vkGetMemoryFdPropertiesKHR"));
  if (bs_vkGetMemoryFdPropertiesKHR == NULL) {
    LOG(ERROR) << "vkGetDeviceProcAddr(\"vkGetMemoryFdPropertiesKHR\") failed";
    return result;
  }

  int prime_fd = gbm_bo_get_fd(bo);
  if (prime_fd < 0) {
    LOG(ERROR) << "failed to get prime fd for gbm_bo";
    return result;
  }

  VkMemoryFdPropertiesKHR fd_props = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
  };
  bs_vkGetMemoryFdPropertiesKHR(
      dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, prime_fd, &fd_props);

  VkMemoryRequirements mem_reqs = {};
  vkGetImageMemoryRequirements(dev, image, &mem_reqs);

  const uint32_t memory_type_bits =
      fd_props.memoryTypeBits & mem_reqs.memoryTypeBits;
  if (!memory_type_bits) {
    LOG(ERROR) << "no valid memory type";
    close(prime_fd);
    return result;
  }

  const VkMemoryDedicatedAllocateInfo memory_dedicated_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
      .image = image,
  };
  const VkImportMemoryFdInfoKHR memory_fd_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
      .pNext = &memory_dedicated_info,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      .fd = prime_fd,
  };
  VkMemoryAllocateInfo memInfo{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_fd_info,
      .allocationSize = mem_reqs.size,

      // Simply choose the first available memory type.  We
      // need neither performance nor mmap, so all memory
      // types are equally good.
      .memoryTypeIndex = static_cast<uint32_t>(ffs(memory_type_bits)) - 1,
  };

  result = vkAllocateMemory(dev, &memInfo,
                            /*pAllocator*/ NULL, mems);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "memory not allocated properly";
    return result;
  }

  result = vkBindImageMemory(dev, image, mems[0], 0);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "failed to bind memory";
    return result;
  }

  return result;
}

VkResult CreateVkImage(VkDevice dev,
                       struct gbm_bo* bo,
                       VkFormat format,
                       VkImage* image) {
  // TODO(crbug.com/1002071): Pass the stride of the imported buffer to
  // vkCreateImage once the extension is supported on Intel.
  VkExternalMemoryImageCreateInfo memImInfo{
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
  };
  VkImageCreateInfo memInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = &memImInfo,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = (VkExtent3D){gbm_bo_get_width(bo), gbm_bo_get_height(bo), 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = static_cast<VkSampleCountFlagBits>(1),
      .tiling = VK_IMAGE_TILING_LINEAR,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  return vkCreateImage(dev, &memInfo,
                       /*pAllocator*/ NULL, image);
}

std::unique_ptr<ClientBase::Buffer> ClientBase::CreateDrmBuffer(
    const gfx::Size& size,
    int32_t drm_format,
    const uint64_t* drm_modifiers,
    const unsigned int drm_modifiers_count,
    int32_t bo_usage,
    bool y_invert) {
  std::unique_ptr<Buffer> buffer;
#if defined(USE_GBM)
  if (device_) {
    if (!gbm_device_is_format_supported(device_.get(), drm_format, bo_usage)) {
      LOG(ERROR) << "Format/usage not supported";
      return nullptr;
    }

    buffer = std::make_unique<Buffer>();
    if (drm_modifiers_count == 0) {
      buffer->bo.reset(gbm_bo_create(device_.get(), size.width(), size.height(),
                                     drm_format, bo_usage));
    } else {
      buffer->bo.reset(gbm_bo_create_with_modifiers(
          device_.get(), size.width(), size.height(), drm_format, drm_modifiers,
          drm_modifiers_count));
    }
    if (!buffer->bo) {
      LOG(ERROR) << "Can't create gbm buffer";
      return nullptr;
    }
    base::ScopedFD fd(gbm_bo_get_plane_fd(buffer->bo.get(), 0));

    buffer->params.reset(
        zwp_linux_dmabuf_v1_create_params(globals_.linux_dmabuf.get()));
    uint64_t modifier = gbm_bo_get_modifier(buffer->bo.get());
    for (size_t i = 0;
         i < static_cast<size_t>(gbm_bo_get_plane_count(buffer->bo.get()));
         ++i) {
      base::ScopedFD plane_i_fd(gbm_bo_get_plane_fd(buffer->bo.get(), i));
      uint32_t stride = gbm_bo_get_stride_for_plane(buffer->bo.get(), i);
      uint32_t offset = gbm_bo_get_offset(buffer->bo.get(), i);
      zwp_linux_buffer_params_v1_add(buffer->params.get(), plane_i_fd.get(), i,
                                     offset, stride, modifier >> 32, modifier);
    }
    uint32_t flags = 0;
    if (y_invert)
      flags |= ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT;

    buffer->buffer.reset(zwp_linux_buffer_params_v1_create_immed(
        buffer->params.get(), size.width(), size.height(), drm_format, flags));

    if (gbm_bo_get_plane_count(buffer->bo.get()) != 1)
      return buffer;

    EGLint khr_image_attrs[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT,
        fd.get(),
        EGL_WIDTH,
        size.width(),
        EGL_HEIGHT,
        size.height(),
        EGL_LINUX_DRM_FOURCC_EXT,
        drm_format,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        static_cast<EGLint>(gbm_bo_get_stride_for_plane(buffer->bo.get(), 0)),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        0,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
        static_cast<EGLint>(modifier),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
        static_cast<EGLint>(modifier >> 32),
        EGL_NONE};
    EGLImageKHR image = eglCreateImageKHR(
        eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
        nullptr /* no client buffer */, khr_image_attrs);

    buffer->egl_image = std::make_unique<ScopedEglImage>(image);
    GLuint texture = 0;
    glGenTextures(1, &texture);
    buffer->texture = std::make_unique<ScopedTexture>(texture);
    glBindTexture(GL_TEXTURE_2D, buffer->texture->get());
    glEGLImageTargetTexture2DOES(
        GL_TEXTURE_2D, static_cast<GLeglImageOES>(buffer->egl_image->get()));
    glBindTexture(GL_TEXTURE_2D, 0);

    GrGLTextureInfo texture_info;
    texture_info.fID = buffer->texture->get();
    texture_info.fTarget = GL_TEXTURE_2D;
    texture_info.fFormat = kSizedInternalFormat;
    auto backend_texture = GrBackendTextures::MakeGL(
        size.width(), size.height(), skgpu::Mipmapped::kNo, texture_info);
    buffer->sk_surface = SkSurfaces::WrapBackendTexture(
        gr_context_.get(), backend_texture, kTopLeft_GrSurfaceOrigin,
        /* sampleCnt */ 0, kColorType, /* colorSpace */ nullptr,
        /* props */ nullptr);
    DCHECK(buffer->sk_surface);

#if defined(USE_VULKAN)
    if (!vk_implementation_) {
      return buffer;
    }

    base::ScopedFD vk_image_fd(gbm_bo_get_plane_fd(buffer->bo.get(), 0));
    CHECK(vk_image_fd.is_valid());

    buffer->vk_memory.reset(
        new ScopedVkDeviceMemory(VK_NULL_HANDLE, {vk_device_->get()}));
    buffer->vk_image.reset(
        new ScopedVkImage(VK_NULL_HANDLE, {vk_device_->get()}));
    VkResult result = CreateVkImage(
        vk_device_->get(), buffer->bo.get(), VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        ScopedVkImage::Receiver(*buffer->vk_image).get());
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "Failed to create a Vulkan image.";
      return buffer;
    }

    result = BindImageBo(
        vk_device_->get(), buffer->vk_image->get(), buffer->bo.get(),
        ScopedVkDeviceMemory::Receiver(*buffer->vk_memory).get());
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "Failed to bind a dmabuf to the Vulkan image.";
      return buffer;
    }

    VkImageViewCreateInfo vk_image_view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = buffer->vk_image->get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    buffer->vk_image_view.reset(
        new ScopedVkImageView(VK_NULL_HANDLE, {vk_device_->get()}));
    result = vkCreateImageView(
        vk_device_->get(), &vk_image_view_create_info, nullptr,
        ScopedVkImageView::Receiver(*buffer->vk_image_view).get());
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "Failed to create a Vulkan image view.";
      return buffer;
    }
    VkFramebufferCreateInfo vk_framebuffer_create_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = vk_render_pass_->get(),
        .attachmentCount = 1,
        .pAttachments = &buffer->vk_image_view->get(),
        .width = static_cast<uint32_t>(size.width()),
        .height = static_cast<uint32_t>(size.height()),
        .layers = 1,
    };
    buffer->vk_framebuffer.reset(
        new ScopedVkFramebuffer(VK_NULL_HANDLE, {vk_device_->get()}));

    result = vkCreateFramebuffer(
        vk_device_->get(), &vk_framebuffer_create_info, nullptr,
        ScopedVkFramebuffer::Receiver(*buffer->vk_framebuffer).get());
    if (result != VK_SUCCESS) {
      LOG(ERROR) << "Failed to create a Vulkan framebuffer.";
      return buffer;
    }
#endif  // defined(USE_VULKAN)
  }
#endif  // defined(USE_GBM)

  return buffer;
}

ClientBase::Buffer* ClientBase::DequeueBuffer() {
  auto buffer_it =
      base::ranges::find_if_not(buffers_, &ClientBase::Buffer::busy);
  if (buffer_it == buffers_.end())
    return nullptr;

  Buffer* buffer = buffer_it->get();
  buffer->busy = true;
  return buffer;
}

void ClientBase::SetupAuraShellIfAvailable() {
  if (!globals_.aura_shell) {
    LOG(ERROR) << "Can't find aura shell interface";
    return;
  }

  static zaura_shell_listener kAuraShellListener = {
      [](void* data, struct zaura_shell* zaura_shell, uint32_t layout_mode) {},
      [](void* data, struct zaura_shell* zaura_shell, uint32_t id) {
        CastToClientBase(data)->bug_fix_ids_.insert(id);
      },
      [](void* data, struct zaura_shell* zaura_shell,
         struct wl_array* desk_names) {},
      [](void* data, struct zaura_shell* zaura_shell,
         int32_t active_desk_index) {},
      [](void* data, struct zaura_shell* zaura_shell,
         struct wl_surface* gained_active, struct wl_surface* lost_active) {},
      [](void* data, struct zaura_shell* zaura_shell) {},
      [](void* data, struct zaura_shell* zaura_shell) {},
      [](void* data, struct zaura_shell* zaura_shell,
         const char* compositor_version) {},
      [](void* data, struct zaura_shell* zaura_shell) {},
      [](void* data, struct zaura_shell* zaura_shell,
         uint32_t upper_left_radius, uint32_t upper_right_radius,
         uint32_t lower_right_radius, uint32_t lower_left_radius) {}};
  zaura_shell_add_listener(globals_.aura_shell.get(), &kAuraShellListener,
                           this);

  std::unique_ptr<zaura_surface> aura_surface(static_cast<zaura_surface*>(
      zaura_shell_get_aura_surface(globals_.aura_shell.get(), surface_.get())));
  if (!aura_surface) {
    LOG(ERROR) << "Can't get aura surface";
    return;
  }

  zaura_surface_set_frame(aura_surface.get(), ZAURA_SURFACE_FRAME_TYPE_NORMAL);

  static zaura_output_listener kAuraOutputListener = {
      [](void* data, struct zaura_output* zaura_output, uint32_t flags,
         uint32_t scale) {},
      [](void* data, struct zaura_output* zaura_output, uint32_t connection) {},
      [](void* data, struct zaura_output* zaura_output, uint32_t scale) {},
      [](void* data, struct zaura_output* zaura_output, int32_t top,
         int32_t left, int32_t bottom, int32_t right) {
        CastToClientBase(data)->HandleInsets(
            gfx::Insets::TLBR(top, left, bottom, right));
      },
      [](void* data, struct zaura_output* zaura_output, int32_t transform) {
        CastToClientBase(data)->HandleLogicalTransform(transform);
      },
      [](void* data, struct zaura_output* zaura_output, uint32_t display_id_hi,
         uint32_t display_id_lo) {},
  };

  while (globals_.aura_outputs.size() < globals_.outputs.size()) {
    size_t offset = globals_.aura_outputs.size();
    std::unique_ptr<zaura_output> aura_output(zaura_shell_get_aura_output(
        globals_.aura_shell.get(), globals_.outputs[offset].get()));
    zaura_output_add_listener(aura_output.get(), &kAuraOutputListener, this);
    globals_.aura_outputs.emplace_back(std::move(aura_output));
  }
}

void ClientBase::SetupPointerStylus() {
  if (!globals_.stylus) {
    LOG(ERROR) << "Can't find stylus interface";
    return;
  }

  wl_pointer_ = base::WrapUnique(
      static_cast<wl_pointer*>(wl_seat_get_pointer(globals_.seat.get())));

  zcr_pointer_stylus_ = base::WrapUnique(
      static_cast<zcr_pointer_stylus_v2*>(zcr_stylus_v2_get_pointer_stylus(
          globals_.stylus.get(), wl_pointer_.get())));
  if (!zcr_pointer_stylus_) {
    LOG(ERROR) << "Can't get pointer stylus";
    return;
  }

  static zcr_pointer_stylus_v2_listener kPointerStylusV2Listener = {
      [](void* data, struct zcr_pointer_stylus_v2* x, uint32_t y) {},
      [](void* data, struct zcr_pointer_stylus_v2* x, uint32_t y,
         wl_fixed_t z) {},
      [](void* data, struct zcr_pointer_stylus_v2* x, uint32_t y, wl_fixed_t z,
         wl_fixed_t a) {},
  };
  zcr_pointer_stylus_v2_add_listener(zcr_pointer_stylus_.get(),
                                     &kPointerStylusV2Listener, this);
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo
