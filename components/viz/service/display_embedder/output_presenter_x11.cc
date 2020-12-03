// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_x11.h"

extern "C" {
#include <X11/xshmfence.h>
}

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_fence.h"

#define VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA (VkStructureType)1000001002
struct wsi_image_create_info {
  VkStructureType sType;
  const void* pNext;
  bool scanout;
};

namespace viz {

namespace {

class Fence {
 public:
  static std::unique_ptr<Fence> Create(x11::Connection* connection,
                                       x11::Pixmap pixmap) {
    base::ScopedFD fence_fd(xshmfence_alloc_shm());
    if (!fence_fd.is_valid()) {
      DLOG(ERROR) << "xshmfence_alloc_shm() failed!";
      return {};
    }

    auto* shm_fence = xshmfence_map_shm(fence_fd.get());
    if (!shm_fence) {
      DLOG(ERROR) << "xshmfence_map_shm() failed!";
      return {};
    }

    auto* dri3 = &connection->dri3();
    auto fence = connection->GenerateId<x11::Sync::Fence>();
    dri3->FenceFromFD(
        {pixmap, static_cast<uint32_t>(fence), 1, std::move(fence_fd)});
    return std::make_unique<Fence>(connection, base::PassKey<Fence>(),
                                   shm_fence, fence);
  }

  Fence(x11::Connection* connection,
        base::PassKey<Fence>,
        xshmfence* shm_fence,
        x11::Sync::Fence fence)
      : connection_(connection), shm_fence_(shm_fence), fence_(fence) {}

  ~Fence() {
    xshmfence_unmap_shm(shm_fence_);
    connection_->sync().DestroyFence({fence_});
  }

  void Reset() { xshmfence_reset(shm_fence_); }
  void Trigger() { xshmfence_trigger(shm_fence_); }
  void Wait() { xshmfence_await(shm_fence_); }

  x11::Sync::Fence fence() const { return fence_; }

 private:
  x11::Connection* const connection_;
  xshmfence* const shm_fence_;
  const x11::Sync::Fence fence_;
};

class PresenterImageX11 : public OutputPresenter::Image {
 public:
  PresenterImageX11() = default;
  ~PresenterImageX11() override {
    if (vk_fence_ != VK_NULL_HANDLE)
      vkDestroyFence(device_queue_->GetVulkanDevice(), vk_fence_, nullptr);
    if (pixmap_ != x11::Pixmap::None)
      x11::Connection::Get()->FreePixmap({pixmap_});
  }

  bool Initialize(x11::Connection* connection,
                  x11::Window window,
                  gpu::SharedImageFactory* factory,
                  gpu::SharedImageRepresentationFactory* representation_factory,
                  SkiaOutputSurfaceDependency* deps,
                  const gfx::Size& size,
                  const gfx::ColorSpace& color_space,
                  ResourceFormat format,
                  int depth,
                  uint32_t shared_image_usage,
                  const std::vector<std::vector<uint64_t>>& modifier_vectors) {
    device_queue_ = deps->GetVulkanContextProvider()->GetDeviceQueue();

    // OutputPresenterX11 only supports MESA vulkan driver.
    switch (device_queue_->vk_physical_device_driver_properties().driverID) {
      case VK_DRIVER_ID_MESA_RADV:
      case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
        break;
      default:
        return false;
    }

    const auto vk_format = ToVkFormat(format);
    std::unique_ptr<gpu::VulkanImage> vulkan_image;
    if (modifier_vectors.empty()) {
      // If DRM modifier is not supported, lagecy wsi_image_create_info will be
      // used for scanout.
      wsi_image_create_info create_info = {
          .sType = VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA,
          .scanout = true,
      };
      vulkan_image = gpu::VulkanImage::CreateWithExternalMemory(
          device_queue_, size, vk_format, shared_image_usage, /*flags=*/0,
          VK_IMAGE_TILING_OPTIMAL, &create_info);
    } else {
      for (auto& modifiers : modifier_vectors) {
        vulkan_image = gpu::VulkanImage::CreateWithExternalMemoryAndModifiers(
            device_queue_, size, vk_format, modifiers, shared_image_usage,
            /*flags=*/0);
        if (vulkan_image)
          break;
      }
    }

    if (!vulkan_image)
      return false;

    // Destroy the |vulkan_image| when this function returns. The |vulkan_image|
    // will be imported as a SharedImage, and the original |vulkan_image| will
    // not be used after returning of this function.
    // TODO(penghuang): maybe creating the shared image directly.
    base::ScopedClosureRunner vulkan_image_destructor(base::BindOnce(
        &gpu::VulkanImage::Destroy, base::Unretained(vulkan_image.get())));

    const auto& layouts = vulkan_image->layouts();

    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle.modifier = vulkan_image->modifier();
    for (size_t i = 0; i < vulkan_image->plane_count(); ++i) {
      gmb_handle.native_pixmap_handle.planes.emplace_back(
          layouts[i].rowPitch, layouts[i].offset, layouts[i].size,
          vulkan_image->GetMemoryFd());
    }

    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (!factory->CreateSharedImage(mailbox, 0, std::move(gmb_handle),
                                    BufferFormat(format),
                                    deps->GetSurfaceHandle(), size, color_space,
                                    kTopLeft_GrSurfaceOrigin,
                                    kPremul_SkAlphaType, shared_image_usage)) {
      DLOG(ERROR) << "CreateSharedImage failed.";
      return false;
    }

    if (!Image::Initialize(factory, representation_factory, mailbox, deps))
      return false;

    auto* dri3 = &connection->dri3();

    DCHECK(format == RGBA_8888 || format == BGRA_8888);
    pixmap_ = connection->GenerateId<x11::Pixmap>();
    std::vector<base::ScopedFD> buffers(vulkan_image->plane_count());
    for (size_t i = 0; i < vulkan_image->plane_count(); ++i)
      buffers[i] = vulkan_image->GetMemoryFd();
    dri3->PixmapFromBuffers({.pixmap = pixmap_,
                             .window = window,
                             .width = size.width(),
                             .height = size.height(),
                             .stride0 = layouts[0].rowPitch,
                             .offset0 = layouts[0].offset,
                             .stride1 = layouts[1].rowPitch,
                             .offset1 = layouts[1].offset,
                             .stride2 = layouts[2].rowPitch,
                             .offset2 = layouts[2].offset,
                             .stride3 = layouts[3].rowPitch,
                             .offset3 = layouts[3].offset,
                             .depth = depth,
                             .bpp = 32,
                             .modifier = vulkan_image->modifier(),
                             .buffers = std::move(buffers)});

    VkFenceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if (vkCreateFence(device_queue_->GetVulkanDevice(), &create_info, nullptr,
                      &vk_fence_) != VK_SUCCESS)
      return false;

    wait_fence_ = Fence::Create(connection, pixmap_);
    if (!wait_fence_)
      return false;

    idle_fence_ = Fence::Create(connection, pixmap_);
    if (!idle_fence_)
      return false;

    return true;
  }

  void BeginPresent() final {
    if (++present_count_ != 1) {
      DCHECK(scoped_read_access_);
      return;
    }

    DCHECK(!sk_surface());
    DCHECK(!scoped_read_access_);

    std::vector<GrBackendSemaphore> begin_read_semaphores;
    scoped_read_access_ = skia_representation()->BeginScopedReadAccess(
        &begin_read_semaphores, &end_read_semaphores_);
    DCHECK(scoped_read_access_);
    vkResetFences(device_queue_->GetVulkanDevice(), 1, &vk_fence_);
    std::vector<VkSemaphore> vk_semaphores;
    for (auto semaphore : begin_read_semaphores)
      vk_semaphores.push_back(semaphore.vkSemaphore());
    if (!gpu::SubmitWaitVkSemaphores(device_queue_->GetVulkanQueue(),
                                     vk_semaphores, vk_fence_)) {
      NOTREACHED();
    }
    // TODO(penghuang): wait |vk_fence_| and then trigger the |wait_fence_|.
  }

  void EndPresent() final {
    DCHECK(present_count_);
    if (--present_count_)
      return;
    std::vector<VkSemaphore> vk_semaphores;
    for (auto semaphore : end_read_semaphores_)
      vk_semaphores.push_back(semaphore.vkSemaphore());
    end_read_semaphores_.clear();
    gpu::SubmitSignalVkSemaphores(device_queue_->GetVulkanQueue(),
                                  vk_semaphores);
    scoped_read_access_.reset();
  }

  int GetPresentCount() const final { return present_count_; }

  void OnContextLost() final {}

  x11::Pixmap pixmap() const { return pixmap_; }
  Fence* wait_fence() const { return wait_fence_.get(); }
  Fence* idle_fence() const { return idle_fence_.get(); }
  bool busy() const { return busy_; }
  void set_busy(bool busy) { busy_ = busy; }

 private:
  std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access_;
  int present_count_ = 0;
  std::vector<GrBackendSemaphore> end_read_semaphores_;
  x11::Pixmap pixmap_{};
  gpu::VulkanDeviceQueue* device_queue_ = nullptr;
  VkFence vk_fence_ = VK_NULL_HANDLE;
  std::unique_ptr<Fence> wait_fence_;
  std::unique_ptr<Fence> idle_fence_;
  bool busy_ = false;
};

}  // namespace

// static
const uint32_t OutputPresenterX11::kDefaultSharedImageUsage =
    gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY |
    gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;

// static
std::unique_ptr<OutputPresenterX11> OutputPresenterX11::Create(
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory) {
  auto presenter = std::make_unique<OutputPresenterX11>(deps, factory,
                                                        representation_factory);
  if (!presenter->Initialize())
    presenter.reset();
  return presenter;
}

OutputPresenterX11::OutputPresenterX11(
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory,
    uint32_t shared_image_usage)
    : dependency_(deps),
      shared_image_factory_(factory),
      shared_image_representation_factory_(representation_factory),
      shared_image_usage_(shared_image_usage),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

OutputPresenterX11::~OutputPresenterX11() {
  if (!event_id_)
    return;

  auto* present = &connection_->present();
  present->SelectInput({static_cast<x11::Present::Event>(event_id_), window_,
                        x11::Present::EventMask::NoEvent});

  auto* event_source = ui::X11EventSource::GetInstance();
  event_source->RemoveXEventDispatcher(this);
}

bool OutputPresenterX11::Initialize() {
  if (!dependency_->IsUsingVulkan())
    return false;

  auto* device_queue = dependency_->GetSharedContextState()
                           ->vk_context_provider()
                           ->GetDeviceQueue();
  // OutputPresenterX11 only supports MESA vulkan driver.
  switch (device_queue->vk_physical_device_driver_properties().driverID) {
    case VK_DRIVER_ID_MESA_RADV:
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
      break;
    default:
      return false;
  }

  connection_ = x11::Connection::Get();
  auto* present = &connection_->present();
  if (!present->present())
    return false;

  auto* dri3 = &connection_->dri3();
  if (!dri3->present())
    return false;

  auto* sync = &connection_->sync();
  if (!sync->present())
    return false;

  window_ = static_cast<x11::Window>(dependency_->GetSurfaceHandle());

  auto geometry = connection_->GetGeometry({window_}).Sync();
  depth_ = geometry->depth;

  const bool support_modifier =
      gfx::HasExtension(device_queue->enabled_extensions(),
                        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
  if (auto modifiers = dri3->GetSupportedModifiers(
                               {static_cast<uint32_t>(window_), depth_, 32})
                           .Sync()) {
    if (!modifiers->window_modifiers.empty())
      modifier_vectors_.push_back(std::move(modifiers->window_modifiers));
    if (!modifiers->screen_modifiers.empty())
      modifier_vectors_.push_back(std::move(modifiers->screen_modifiers));
  }

  // If Xserver require pixmap with modifier, but Vulkan doesn't support
  // modifier, we cannot use X11 present.
  if (!modifier_vectors_.empty() && !support_modifier)
    return false;

  // Without modifier, Xserver can only handle BGRA format.
  supports_rgba_ = !modifier_vectors_.empty();

  event_id_ = connection_->GenerateId<uint32_t>();
  constexpr auto kEventMasks = x11::Present::EventMask::ConfigureNotify |
                               x11::Present::EventMask::CompleteNotify |
                               x11::Present::EventMask::IdleNotify;
  present->SelectInput(
      {static_cast<x11::Present::Event>(event_id_), window_, kEventMasks});

  auto* event_source = ui::X11EventSource::GetInstance();
  event_source->AddXEventDispatcher(this);

  return true;
}

void OutputPresenterX11::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  capabilities->supports_post_sub_buffer = true;
  capabilities->supports_commit_overlay_planes = false;
  // Set supports_surfaceless to enable overlays.
  capabilities->supports_surfaceless = true;
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  // TODO(https://crbug.com/1108406): only add supported formats base on
  // platform, driver, etc.
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      supports_rgba_ ? kRGBA_8888_SkColorType : kBGRA_8888_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kBGRA_8888_SkColorType;
}

bool OutputPresenterX11::Reshape(const gfx::Size& size,
                                 float device_scale_factor,
                                 const gfx::ColorSpace& color_space,
                                 gfx::BufferFormat format,
                                 gfx::OverlayTransform transform) {
  DCHECK(format == gfx::BufferFormat::RGBA_8888 ||
         format == gfx::BufferFormat::BGRA_8888);
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      image_format_ = supports_rgba_ ? RGBA_8888 : BGRA_8888;
      break;
    case gfx::BufferFormat::BGRA_8888:
      image_format_ = BGRA_8888;
      break;
    default:
      NOTREACHED();
  }
  return true;
}

std::vector<std::unique_ptr<OutputPresenter::Image>>
OutputPresenterX11::AllocateImages(gfx::ColorSpace color_space,
                                   gfx::Size image_size,
                                   size_t num_images) {
  std::vector<std::unique_ptr<Image>> images;
  for (size_t i = 0; i < num_images; ++i) {
    auto image = std::make_unique<PresenterImageX11>();
    if (!image->Initialize(connection_, window_, shared_image_factory_,
                           shared_image_representation_factory_, dependency_,
                           image_size, color_space, image_format_, depth_,
                           shared_image_usage_, modifier_vectors_)) {
      DLOG(ERROR) << "Failed to initialize image.";
      return {};
    }
    images.push_back(std::move(image));
  }

  allocated_image_count_ = images.size();
  return images;
}

std::unique_ptr<OutputPresenter::Image>
OutputPresenterX11::AllocateBackgroundImage(gfx::ColorSpace color_space,
                                            gfx::Size image_size) {
  auto image = std::make_unique<PresenterImageX11>();
  if (!image->Initialize(connection_, window_, shared_image_factory_,
                         shared_image_representation_factory_, dependency_,
                         image_size, color_space, image_format_, depth_,
                         shared_image_usage_, modifier_vectors_)) {
    DLOG(ERROR) << "Failed to initialize image.";
    return nullptr;
  }
  return image;
}

void OutputPresenterX11::SwapBuffers(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  NOTIMPLEMENTED();
}

void OutputPresenterX11::PostSubBuffer(
    const gfx::Rect& rect,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  auto* image = static_cast<PresenterImageX11*>(present_images_.back());
  DCHECK(!image->busy());
  image->set_busy(true);

  last_target_msc_ = std::max(last_present_msc_, last_target_msc_) + 1;
  // Trigger the |wait_fence|, so the Xserver will not be blocked.
  // TODO(penghuang): trigger the fence when the image is ready to present, and
  // wait for the |idle_fence| before reusing the image.
  x11::Connection::Get()->present().Pixmap({
      .window = window_,
      .pixmap = image->pixmap(),
      .target_msc = last_target_msc_,
  });

  swap_completion_callbacks_.push_back(std::move(completion_callback));
  presentation_callbacks_.push_back(std::move(presentation_callback));
}

void OutputPresenterX11::SchedulePrimaryPlane(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
    Image* image,
    bool is_submitted) {
  present_images_.push_back(image);
}

void OutputPresenterX11::ScheduleBackground(Image* image) {
  NOTIMPLEMENTED();
}

bool OutputPresenterX11::DispatchXEvent(x11::Event* event) {
  if (event->window() != window_)
    return false;
  if (auto* e = event->As<x11::Present::ConfigureNotifyEvent>()) {
    return OnConfigureNotifyEvent(e);
  } else if (auto* e = event->As<x11::Present::CompleteNotifyEvent>()) {
    return OnCompleteNotifyEvent(e);
  } else if (auto* e = event->As<x11::Present::IdleNotifyEvent>()) {
    return OnIdleNotifyEvent(e);
  } else if (auto* e = event->As<x11::Present::RedirectNotifyEvent>()) {
    return OnRedirectNotifyEvent(e);
  }
  return false;
}

void OutputPresenterX11::CommitOverlayPlanes(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  NOTIMPLEMENTED();
}

void OutputPresenterX11::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays,
    std::vector<ScopedOverlayAccess*> accesses) {
  NOTIMPLEMENTED();
}

bool OutputPresenterX11::OnConfigureNotifyEvent(
    const x11::Present::ConfigureNotifyEvent* event) {
  return true;
}

bool OutputPresenterX11::OnCompleteNotifyEvent(
    const x11::Present::CompleteNotifyEvent* event) {
  DCHECK_LE(last_present_msc_, event->msc);
  last_present_msc_ = event->msc;
  // Calling |swap_completion_callbacks_| will kick off a new frame, in most
  // cases, there should be one or more images are idle. However in some corner
  // cases, all images could be busy.
  // TODO(penghuang): defer calling |swap_completion_callbacks_|, if all images
  // are busy.
  std::move(swap_completion_callbacks_.front())
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  swap_completion_callbacks_.pop_front();

  auto timestamp =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(event->ust);
  // Assume the refresh rate is 60 Hz for now.
  // TODO(penghuang): query refresh rate from Xserver.
  constexpr auto kInterval = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond / 60);
  uint32_t flags = gfx::PresentationFeedback::kVSync;
  if (event->mode == x11::Present::CompleteMode::Flip)
    flags |= gfx::PresentationFeedback::kZeroCopy;
  std::move(presentation_callbacks_.front())
      .Run(gfx::PresentationFeedback(timestamp, kInterval, flags));
  presentation_callbacks_.pop_front();
  return true;
}

bool OutputPresenterX11::OnIdleNotifyEvent(
    const x11::Present::IdleNotifyEvent* event) {
  for (auto* image : present_images_) {
    auto* image_x11 = static_cast<PresenterImageX11*>(image);
    if (image_x11->pixmap() == event->pixmap) {
      DCHECK(image_x11->busy());
      image_x11->set_busy(false);
      break;
    }
  }

  // Remove idle images at the beginning of the |present_images_|.
  while (!present_images_.empty()) {
    auto* image_x11 = static_cast<PresenterImageX11*>(present_images_.front());
    if (image_x11->busy())
      break;
    present_images_.pop_front();
  }

  return true;
}

bool OutputPresenterX11::OnRedirectNotifyEvent(
    const x11::Present::RedirectNotifyEvent* event) {
  return true;
}

}  // namespace viz
