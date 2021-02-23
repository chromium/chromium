// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_x11.h"

extern "C" {
#include <X11/xshmfence.h>
}

#include <algorithm>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/present.h"

#define VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA (VkStructureType)1000001002
struct wsi_image_create_info {
  VkStructureType sType;
  const void* pNext;
  bool scanout;
};

namespace viz {

namespace {

std::vector<VkSemaphore> ToVkSemaphores(
    std::vector<GrBackendSemaphore>& semaphores) {
  std::vector<VkSemaphore> vk_semaphores(semaphores.size());
  for (size_t i = 0; i < semaphores.size(); ++i)
    vk_semaphores[i] = semaphores[i].vkSemaphore();
  return vk_semaphores;
}

class Fence {
 public:
  static std::unique_ptr<Fence> Create(x11::Pixmap pixmap) {
    x11::RefCountedFD fence_fd(xshmfence_alloc_shm());
    if (fence_fd.get() < 0) {
      DLOG(ERROR) << "xshmfence_alloc_shm() failed!";
      return {};
    }

    auto* shm_fence = xshmfence_map_shm(fence_fd.get());
    if (!shm_fence) {
      DLOG(ERROR) << "xshmfence_map_shm() failed!";
      return {};
    }

    auto* connection = x11::Connection::Get();
    auto* dri3 = &connection->dri3();
    auto fence = connection->GenerateId<x11::Sync::Fence>();
    dri3->FenceFromFD(pixmap, static_cast<uint32_t>(fence), 1, fence_fd);
    return std::make_unique<Fence>(base::PassKey<Fence>(), shm_fence, fence);
  }

  Fence(base::PassKey<Fence>, xshmfence* shm_fence, x11::Sync::Fence fence)
      : shm_fence_(shm_fence), fence_(fence) {}

  ~Fence() {
    auto* connection = x11::Connection::Get();
    xshmfence_unmap_shm(shm_fence_);
    connection->sync().DestroyFence({fence_});
  }

  void Reset() { xshmfence_reset(shm_fence_); }
  void Wait() { xshmfence_await(shm_fence_); }

  x11::Sync::Fence fence() const { return fence_; }

 private:
  xshmfence* const shm_fence_;
  const x11::Sync::Fence fence_;
};

class PresenterImageX11 : public OutputPresenter::Image {
 public:
  PresenterImageX11();
  ~PresenterImageX11() override;

  bool Initialize(x11::Window window,
                  gpu::SharedImageFactory* factory,
                  gpu::SharedImageRepresentationFactory* representation_factory,
                  SkiaOutputSurfaceDependency* deps,
                  const gfx::Size& size,
                  const gfx::ColorSpace& color_space,
                  ResourceFormat format,
                  int depth,
                  uint32_t shared_image_usage,
                  const std::vector<std::vector<uint64_t>>& modifier_vectors,
                  scoped_refptr<base::SingleThreadTaskRunner> x11_task_runner);
  // OutputPresenterX11::Image:
  void BeginPresent() final;
  void EndPresent() final;
  int GetPresentCount() const final;
  void OnContextLost() final;

  class OnX11 : public base::RefCountedThreadSafe<OnX11> {
   public:
    OnX11(gpu::VulkanDeviceQueue* device_queue, VkFence vk_fence);

    void Initialize(x11::Dri3::PixmapFromBuffersRequest request);
    void WaitForVkFence();
    VkFence vk_fence() const { return vk_fence_; }
    x11::Pixmap pixmap() const {
      DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
      return pixmap_;
    }
    Fence* idle_fence() const { return idle_fence_.get(); }

   private:
    friend base::RefCountedThreadSafe<OnX11>;
    ~OnX11();

    gpu::VulkanDeviceQueue* const device_queue_;
    const VkFence vk_fence_;
    std::unique_ptr<Fence> idle_fence_;
    // |pixmap_| is created, destroyed and used on X11 thread only.
    x11::Pixmap pixmap_ GUARDED_BY_CONTEXT(thread_checker_){};
    THREAD_CHECKER(thread_checker_);
  };

  scoped_refptr<OnX11> on_x11() const { return on_x11_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> x11_task_runner_;
  std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access_;
  int present_count_ = 0;
  gpu::VulkanDeviceQueue* device_queue_ = nullptr;
  std::vector<GrBackendSemaphore> end_read_semaphores_;
  scoped_refptr<OnX11> on_x11_;
};

PresenterImageX11::OnX11::OnX11(gpu::VulkanDeviceQueue* device_queue,
                                VkFence vk_fence)
    : device_queue_(device_queue), vk_fence_(vk_fence) {
  DETACH_FROM_THREAD(thread_checker_);
}

PresenterImageX11::OnX11::~OnX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (vk_fence_ != VK_NULL_HANDLE)
    vkDestroyFence(device_queue_->GetVulkanDevice(), vk_fence_, nullptr);
  if (pixmap_ != x11::Pixmap::None)
    x11::Connection::Get()->FreePixmap({pixmap_});
}

void PresenterImageX11::OnX11::Initialize(
    x11::Dri3::PixmapFromBuffersRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* connection = x11::Connection::Get();
  auto* dri3 = &connection->dri3();

  pixmap_ = connection->GenerateId<x11::Pixmap>();
  request.pixmap = pixmap_;
  dri3->PixmapFromBuffers(request);
  idle_fence_ = Fence::Create(pixmap_);
  CHECK(idle_fence_);
}

void PresenterImageX11::OnX11::WaitForVkFence() {
  auto result = vkWaitForFences(device_queue_->GetVulkanDevice(), 1, &vk_fence_,
                                VK_TRUE, UINT64_MAX);
  DCHECK_EQ(result, VK_SUCCESS);
}

PresenterImageX11::PresenterImageX11() = default;

PresenterImageX11::~PresenterImageX11() {
  if (on_x11_)
    x11_task_runner_->ReleaseSoon(FROM_HERE, std::move(on_x11_));
}

bool PresenterImageX11::Initialize(
    x11::Window window,
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory,
    SkiaOutputSurfaceDependency* deps,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    ResourceFormat format,
    int depth,
    uint32_t shared_image_usage,
    const std::vector<std::vector<uint64_t>>& modifier_vectors,
    scoped_refptr<base::SingleThreadTaskRunner> x11_task_runner) {
  DCHECK(format == RGBA_8888 || format == BGRA_8888);

  device_queue_ = deps->GetVulkanContextProvider()->GetDeviceQueue();
  x11_task_runner_ = std::move(x11_task_runner);

  // OutputPresenterX11 only supports MESA vulkan driver.
  switch (auto driver_id =
              device_queue_->vk_physical_device_driver_properties().driverID) {
    case VK_DRIVER_ID_MESA_RADV:
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
      break;
    default:
      DLOG(ERROR) << "Not supported driver:" << driver_id;
      return false;
  }

  constexpr VkImageUsageFlags kVkImageUsageFlags =
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
        device_queue_, size, vk_format, kVkImageUsageFlags,
        /*flags=*/0, VK_IMAGE_TILING_OPTIMAL, &create_info);
  } else {
    for (auto& modifiers : modifier_vectors) {
      vulkan_image = gpu::VulkanImage::CreateWithExternalMemoryAndModifiers(
          device_queue_, size, vk_format, modifiers, kVkImageUsageFlags,
          /*flags=*/0);
      if (vulkan_image)
        break;
    }
  }

  if (!vulkan_image) {
    DLOG(ERROR) << "Create VulkanImage failed.";
    return false;
  }

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
  if (!factory->CreateSharedImage(
          mailbox, 0, std::move(gmb_handle), BufferFormat(format),
          deps->GetSurfaceHandle(), size, color_space, kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType, shared_image_usage)) {
    DLOG(ERROR) << "CreateSharedImage failed.";
    return false;
  }

  if (!Image::Initialize(factory, representation_factory, mailbox, deps)) {
    DLOG(ERROR) << "Image::Initialize() failed.";
    return false;
  }

  VkFenceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VkFence vk_fence = VK_NULL_HANDLE;
  auto result = vkCreateFence(device_queue_->GetVulkanDevice(), &create_info,
                              nullptr, &vk_fence);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateFence() failed: " << result;
    return false;
  }

  // The ownership of |vk_fence| is passed to OnX11 which will destroy it.
  on_x11_ = base::MakeRefCounted<OnX11>(device_queue_, vk_fence);

  std::vector<x11::RefCountedFD> fds(vulkan_image->plane_count());
  for (size_t i = 0; i < vulkan_image->plane_count(); ++i)
    fds[i] = x11::RefCountedFD(vulkan_image->GetMemoryFd());
  x11::Dri3::PixmapFromBuffersRequest request = {
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
      .buffers = std::move(fds)};
  // Post a task to X11 thread to cretae X11 pixmap, and it will be only used
  // on X11 thread.
  x11_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnX11::Initialize, on_x11_, std::move(request)));

  return true;
}

void PresenterImageX11::BeginPresent() {
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
  VkFence vk_fence = on_x11_->vk_fence();
  vkResetFences(device_queue_->GetVulkanDevice(), 1, &vk_fence);
  auto vk_semaphores = ToVkSemaphores(begin_read_semaphores);
  if (!gpu::SubmitWaitVkSemaphores(device_queue_->GetVulkanQueue(),
                                   vk_semaphores, vk_fence)) {
    NOTREACHED();
  }
}

void PresenterImageX11::EndPresent() {
  DCHECK(present_count_);
  if (--present_count_)
    return;
  auto vk_semaphores = ToVkSemaphores(end_read_semaphores_);
  end_read_semaphores_.clear();
  // Wait on the idle_fence on GPU main, after it is released, so we can reuse
  // the image safely.
  on_x11_->idle_fence()->Wait();
  gpu::SubmitSignalVkSemaphores(device_queue_->GetVulkanQueue(), vk_semaphores);
  scoped_read_access_.reset();
}

int PresenterImageX11::GetPresentCount() const {
  return present_count_;
}

void PresenterImageX11::OnContextLost() {}

constexpr size_t kNumberOfBuffers = 3;
constexpr size_t kMaxPendingFrames = 2;

}  // namespace

class OutputPresenterX11::OnX11 : public x11::EventObserver {
 public:
  explicit OnX11(x11::Window window);
  ~OnX11() override;

  void Initialize();
  void PostSubBuffer(scoped_refptr<PresenterImageX11::OnX11> image,
                     const gfx::Rect& rect,
                     SwapCompletionCallback completion_callback,
                     BufferPresentedCallback presentation_callback);

 private:
  // x11::EventObserver implementations:
  void OnEvent(const x11::Event& event) final;

  bool OnCompleteNotifyEvent(const x11::Present::CompleteNotifyEvent* event);
  bool OnIdleNotifyEvent(const x11::Present::IdleNotifyEvent* event);

  const x11::Window window_ GUARDED_BY_CONTEXT(thread_checker_);
  // For executing task on GPU main thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_
      GUARDED_BY_CONTEXT(thread_checker_);
  std::unique_ptr<ui::X11EventSource> event_source_
      GUARDED_BY_CONTEXT(thread_checker_);
  uint32_t event_id_ GUARDED_BY_CONTEXT(thread_checker_) = 0;
  uint64_t last_target_msc_ GUARDED_BY_CONTEXT(thread_checker_) = 0;
  uint64_t last_present_msc_ GUARDED_BY_CONTEXT(thread_checker_) = 0;

  // Callbacks wait for X11 CompleteNotifyEvent
  base::circular_deque<SwapCompletionCallback> swap_completion_callbacks_
      GUARDED_BY_CONTEXT(thread_checker_);
  base::circular_deque<BufferPresentedCallback> presentation_callbacks_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

OutputPresenterX11::OnX11::OnX11(x11::Window window)
    : window_(window), task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DETACH_FROM_THREAD(thread_checker_);
}

OutputPresenterX11::OnX11::~OnX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* connection = x11::Connection::Get();
  auto* present = &connection->present();
  present->SelectInput({static_cast<x11::Present::Event>(event_id_), window_,
                        x11::Present::EventMask::NoEvent});
  connection->RemoveEventObserver(this);
}

void OutputPresenterX11::OnX11::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* connection = x11::Connection::Get();
  event_source_ = std::make_unique<ui::X11EventSource>(connection);
  connection->AddEventObserver(this);

  auto* present = &connection->present();
  event_id_ = connection->GenerateId<uint32_t>();
  present->SelectInput({static_cast<x11::Present::Event>(event_id_), window_,
                        x11::Present::EventMask::CompleteNotify});
}

void OutputPresenterX11::OnX11::PostSubBuffer(
    scoped_refptr<PresenterImageX11::OnX11> image,
    const gfx::Rect& rect,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Wait for VKFence passed before sending pixmap to Xserver to present.
  image->WaitForVkFence();

  last_target_msc_ = std::max(last_present_msc_, last_target_msc_) + 1;
  x11::Connection::Get()->present().PresentPixmap({
      .window = window_,
      .pixmap = image->pixmap(),
      .idle_fence = image->idle_fence()->fence(),
      .target_msc = last_target_msc_,
  });

  swap_completion_callbacks_.push_back(std::move(completion_callback));
  presentation_callbacks_.push_back(std::move(presentation_callback));
}

void OutputPresenterX11::OnX11::OnEvent(const x11::Event& event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (event.window() != window_)
    return;
  if (auto* e = event.As<x11::Present::CompleteNotifyEvent>()) {
    OnCompleteNotifyEvent(e);
  }
}

bool OutputPresenterX11::OnX11::OnCompleteNotifyEvent(
    const x11::Present::CompleteNotifyEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (event->msc > last_present_msc_)
    last_present_msc_ = event->msc;

  auto timestamp =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(event->ust);
  // Assume the refresh rate is 60 Hz for now.
  // TODO(penghuang): query refresh rate from Xserver.
  constexpr auto kInterval = base::TimeDelta::FromMicroseconds(
      base::Time::kMicrosecondsPerSecond / 60);
  uint32_t flags = gfx::PresentationFeedback::kVSync;
  if (event->mode == x11::Present::CompleteMode::Flip)
    flags |= gfx::PresentationFeedback::kZeroCopy;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(swap_completion_callbacks_.front()),
                     gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK)));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(presentation_callbacks_.front()),
                     gfx::PresentationFeedback(timestamp, kInterval, flags)));
  swap_completion_callbacks_.pop_front();
  presentation_callbacks_.pop_front();

  return true;
}

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
      shared_image_usage_(shared_image_usage) {}

OutputPresenterX11::~OutputPresenterX11() {
  if (on_x11_)
    x11_thread_->task_runner()->DeleteSoon(FROM_HERE, std::move(on_x11_));
  // The dtor of |x11_thread_| will be blocked until all post tasks are
  // finished.
  x11_thread_.reset();
}

bool OutputPresenterX11::Initialize() {
  if (!dependency_->IsUsingVulkan())
    return false;

  auto* device_queue = dependency_->GetSharedContextState()
                           ->vk_context_provider()
                           ->GetDeviceQueue();
  // OutputPresenterX11 only supports MESA vulkan driver.
  auto driver_id =
      device_queue->vk_physical_device_driver_properties().driverID;
  switch (driver_id) {
    case VK_DRIVER_ID_MESA_RADV:
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
      break;
    default:
      DLOG(ERROR) << "Not supported driver: " << driver_id;
      return false;
  }

  // Intel GPU needs modifier to work with the X11 present extension.
  bool need_modifier = driver_id == VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA;

  auto* connection = x11::Connection::Get();
  auto* present = &connection->present();
  if (!present->present())
    return false;

  auto* dri3 = &connection->dri3();
  if (!dri3->present())
    return false;

  auto* sync = &connection->sync();
  if (!sync->present())
    return false;

  auto window = static_cast<x11::Window>(dependency_->GetSurfaceHandle());

  auto geometry = connection->GetGeometry(window).Sync();
  depth_ = geometry->depth;

  if (auto modifiers = dri3->GetSupportedModifiers(
                               {static_cast<uint32_t>(window), depth_, 32})
                           .Sync()) {
    if (!modifiers->window_modifiers.empty())
      modifier_vectors_.push_back(std::move(modifiers->window_modifiers));
    if (!modifiers->screen_modifiers.empty())
      modifier_vectors_.push_back(std::move(modifiers->screen_modifiers));
  }
  need_modifier |= !modifier_vectors_.empty();

  const bool support_modifier =
      gfx::HasExtension(device_queue->enabled_extensions(),
                        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);

  // If Xserver requires pixmap with modifier, but Vulkan doesn't support
  // modifier, we cannot use X11 present.
  if (need_modifier && !support_modifier) {
    DLOG(ERROR) << "Modifier is needed but not supported";
    return false;
  }

  x11_thread_ = std::make_unique<base::Thread>("OutputPresenterX11");
  bool result = x11_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::UI, 0));
  CHECK(result);

  on_x11_ = std::make_unique<OnX11>(window);
  x11_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OutputPresenterX11::OnX11::Initialize,
                                base::Unretained(on_x11_.get())));
  return true;
}

void OutputPresenterX11::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  capabilities->number_of_buffers = kNumberOfBuffers;
  capabilities->max_frames_pending = kMaxPendingFrames;
  capabilities->supports_post_sub_buffer = true;
  capabilities->supports_commit_overlay_planes = false;
  // Set supports_surfaceless to enable overlays.
  capabilities->supports_surfaceless = true;
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  // X11 only supports the BGRA format.
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kBGRA_8888_SkColorType;
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
      image_format_ = BGRA_8888;
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
  DCHECK_EQ(num_images, kNumberOfBuffers);
  auto window = static_cast<x11::Window>(dependency_->GetSurfaceHandle());
  std::vector<std::unique_ptr<Image>> images(num_images);
  for (size_t i = 0; i < num_images; ++i) {
    auto image = std::make_unique<PresenterImageX11>();
    if (!image->Initialize(window, shared_image_factory_,
                           shared_image_representation_factory_, dependency_,
                           image_size, color_space, image_format_, depth_,
                           shared_image_usage_, modifier_vectors_,
                           x11_thread_->task_runner())) {
      DLOG(ERROR) << "Failed to initialize image.";
      return {};
    }
    images[i] = std::move(image);
  }
  return images;
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
  DCHECK(scheduled_image_);
  auto image = static_cast<PresenterImageX11*>(scheduled_image_)->on_x11();
  // Reset the |idle_fence()| which will be released when X11 is done with the
  // pixmap.
  image->idle_fence()->Reset();
  x11_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&OutputPresenterX11::OnX11::PostSubBuffer,
                     base::Unretained(on_x11_.get()), std::move(image), rect,
                     std::move(completion_callback),
                     std::move(presentation_callback)));
  scheduled_image_ = nullptr;
}

void OutputPresenterX11::SchedulePrimaryPlane(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
    Image* image,
    bool is_submitted) {
  DCHECK(!scheduled_image_);
  scheduled_image_ = image;
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

}  // namespace viz
