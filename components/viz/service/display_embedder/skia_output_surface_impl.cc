// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/single_task_sequence.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "third_party/skia/include/gpu/graphite/YUVABackendTextures.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "third_party/skia/include/private/chromium/SkImageChromium.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

namespace viz {

namespace {

// FulfillForPlane is a struct that contains the ImageContext `context` used for
// fulfilling an GrPromiseImageTexture identified by `plane_index`. The
// plane_index is 0 for single planar formats and can be between [0, 3] for
// multiplanar formats.
struct FulfillForPlane {
  explicit FulfillForPlane(ImageContextImpl* context, int plane_index = 0)
      : context_(context), plane_index_(plane_index) {}

  const raw_ptr<ImageContextImpl, DanglingUntriaged> context_ = nullptr;
  const int plane_index_ = 0;
};

sk_sp<GrPromiseImageTexture> FulfillGanesh(void* fulfill) {
  CHECK(fulfill);
  auto* fulfill_for_plane = static_cast<FulfillForPlane*>(fulfill);
  const auto& promise_textures =
      fulfill_for_plane->context_->promise_image_textures();
  int plane_index = fulfill_for_plane->plane_index_;
  CHECK(promise_textures.empty() ||
        plane_index < static_cast<int>(promise_textures.size()));
  return promise_textures.empty()
             ? nullptr
             : sk_ref_sp(promise_textures[plane_index].get());
}

std::tuple<skgpu::graphite::BackendTexture, void*> FulfillGraphite(
    void* fulfill) {
  CHECK(fulfill);
  auto* fulfill_for_plane = static_cast<FulfillForPlane*>(fulfill);
  const auto& graphite_textures =
      fulfill_for_plane->context_->graphite_textures();
  int plane_index = fulfill_for_plane->plane_index_;
  CHECK(graphite_textures.empty() ||
        plane_index < static_cast<int>(graphite_textures.size()));
  auto texture = graphite_textures.empty() ? skgpu::graphite::BackendTexture()
                                           : graphite_textures[plane_index];
  return std::make_tuple(texture, fulfill);
}

void ReleaseGraphite(void* fulfill) {
  // Do nothing. This is called by Graphite after GPU is done with the texture,
  // but we don't rely on it for synchronization or cleanup.
}

void CleanUp(void* fulfill) {
  delete static_cast<FulfillForPlane*>(fulfill);
}

gpu::ContextUrl& GetActiveUrl() {
  static base::NoDestructor<gpu::ContextUrl> active_url(
      GURL("chrome://gpu/SkiaRenderer"));
  return *active_url;
}

OutputSurface::Type GetOutputSurfaceType(SkiaOutputSurfaceDependency* deps) {
  // TODO(penghuang): Support more types.
  return deps->IsUsingVulkan() ? OutputSurface::Type::kVulkan
                               : OutputSurface::Type::kOpenGL;
}

}  // namespace

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    GrDeferredDisplayListRecorder* root_ddl_recorder)
    : ddl_recorder_(root_ddl_recorder), canvas_(ddl_recorder_->getCanvas()) {}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    const GrSurfaceCharacterization& characterization,
    const gpu::Mailbox& mailbox)
    : mailbox_(mailbox) {
  ddl_recorder_storage_.emplace(characterization);
  ddl_recorder_ = &ddl_recorder_storage_.value();
  canvas_ = ddl_recorder_->getCanvas();
}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    skgpu::graphite::Recorder* recorder,
    const SkImageInfo& image_info,
    skgpu::graphite::TextureInfo texture_info,
    const gpu::Mailbox& mailbox)
    : graphite_recorder_(recorder), mailbox_(mailbox) {
  CHECK(graphite_recorder_);
  canvas_ = graphite_recorder_->makeDeferredCanvas(image_info, texture_info);
}

SkiaOutputSurfaceImpl::ScopedPaint::~ScopedPaint() {
  CHECK(!canvas_);
}

sk_sp<GrDeferredDisplayList> SkiaOutputSurfaceImpl::ScopedPaint::DetachDDL() {
  canvas_ = nullptr;
  return ddl_recorder_->detach();
}

std::unique_ptr<skgpu::graphite::Recording>
SkiaOutputSurfaceImpl::ScopedPaint::SnapRecording() {
  canvas_ = nullptr;
  return graphite_recorder_->snap();
}

SkiaOutputSurfaceImpl::FrameBufferDamageTracker::FrameBufferDamageTracker(
    size_t number_of_buffers)
    : number_of_buffers_(number_of_buffers) {}

SkiaOutputSurfaceImpl::FrameBufferDamageTracker::~FrameBufferDamageTracker() =
    default;

void SkiaOutputSurfaceImpl::FrameBufferDamageTracker::FrameBuffersChanged(
    const gfx::Size& frame_buffer_size) {
  frame_buffer_size_ = frame_buffer_size;
  damage_between_frames_.clear();
  cached_current_damage_.reset();
}

void SkiaOutputSurfaceImpl::FrameBufferDamageTracker::SwappedWithDamage(
    const gfx::Rect& damage) {
  damage_between_frames_.push_back(damage);
  // Keep at most `number_of_buffers_` frames.
  if (damage_between_frames_.size() > number_of_buffers_) {
    damage_between_frames_.pop_front();
  }
  cached_current_damage_.reset();
}

void SkiaOutputSurfaceImpl::FrameBufferDamageTracker::SkippedSwapWithDamage(
    const gfx::Rect& damage) {
  if (!damage_between_frames_.empty()) {
    damage_between_frames_.back().Union(damage);
    cached_current_damage_.reset();
  } else {
    // First frame after `FrameBuffersChanged already has full damage.
    // So no need to keep track of it with another entry, which would violate
    // the condition the deque size is at most `number_of_buffers_ - 1`.
  }
}

gfx::Rect
SkiaOutputSurfaceImpl::FrameBufferDamageTracker::GetCurrentFrameBufferDamage()
    const {
  if (!cached_current_damage_)
    cached_current_damage_ = ComputeCurrentFrameBufferDamage();
  return *cached_current_damage_;
}

gfx::Rect SkiaOutputSurfaceImpl::FrameBufferDamageTracker::
    ComputeCurrentFrameBufferDamage() const {
  // First `number_of_buffers_` frames has full frame damage.
  if (damage_between_frames_.size() < number_of_buffers_) {
    return gfx::Rect(frame_buffer_size_);
  }

  // Subsequent frames has `number_of_buffers_ - 1` frames of incremental
  // damange unioned. Note index 0 is specifically skipped over its the damage
  // that's last drawn into that's drawn into the current frame buffer.
  gfx::Rect result;
  for (size_t i = 1; i < damage_between_frames_.size(); ++i) {
    result.Union(damage_between_frames_[i]);
  }
  return result;
}

// static
std::unique_ptr<SkiaOutputSurface> SkiaOutputSurfaceImpl::Create(
    DisplayCompositorMemoryAndTaskController* display_controller,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings) {
  DCHECK(display_controller);
  DCHECK(display_controller->skia_dependency());
  DCHECK(display_controller->gpu_task_scheduler());
  auto output_surface = std::make_unique<SkiaOutputSurfaceImpl>(
      base::PassKey<SkiaOutputSurfaceImpl>(), display_controller,
      renderer_settings, debug_settings);
  if (!output_surface->Initialize())
    output_surface = nullptr;
  return output_surface;
}

SkiaOutputSurfaceImpl::SkiaOutputSurfaceImpl(
    base::PassKey<SkiaOutputSurfaceImpl> /* pass_key */,
    DisplayCompositorMemoryAndTaskController* display_controller,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings)
    : SkiaOutputSurface(
          GetOutputSurfaceType(display_controller->skia_dependency())),
      dependency_(display_controller->skia_dependency()),
      renderer_settings_(renderer_settings),
      debug_settings_(debug_settings),
      display_compositor_controller_(display_controller),
      gpu_task_scheduler_(display_compositor_controller_->gpu_task_scheduler()),
      is_using_raw_draw_(features::IsUsingRawDraw()),
      is_raw_draw_using_msaa_(features::IsRawDrawUsingMSAA()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_using_raw_draw_) {
    auto* manager = dependency_->GetSharedImageManager();
    DCHECK(manager->is_thread_safe());
    representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(manager,
                                                                nullptr);
  }
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("viz", __PRETTY_FUNCTION__);
  current_paint_.reset();
  root_ddl_recorder_.reset();

  if (!render_pass_image_cache_.empty()) {
    std::vector<AggregatedRenderPassId> render_pass_ids;
    render_pass_ids.reserve(render_pass_ids.size());
    for (auto& entry : render_pass_image_cache_)
      render_pass_ids.push_back(entry.first);
    RemoveRenderPassResource(std::move(render_pass_ids));
  }
  DCHECK(render_pass_image_cache_.empty());

  // Save a copy of this pointer before moving it into the task. Tasks that are
  // already enqueud may need to use it before |impl_on_gpu_| is destroyed.
  SkiaOutputSurfaceImplOnGpu* impl_on_gpu = impl_on_gpu_.get();

  // Post a task to destroy |impl_on_gpu_| on the GPU thread.
  auto task = base::BindOnce(
      [](std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu) {},
      std::move(impl_on_gpu_));
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  // Flush GPU tasks and block until all tasks are finished.
  FlushGpuTasksWithImpl(SyncMode::kWaitForTasksFinished, impl_on_gpu);
}

gpu::SurfaceHandle SkiaOutputSurfaceImpl::GetSurfaceHandle() const {
  return dependency_->GetSurfaceHandle();
}

void SkiaOutputSurfaceImpl::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SkiaOutputSurfaceImpl::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(capabilities().supports_dc_layers);

  if (has_set_draw_rectangle_for_frame_)
    return;

  // TODO(kylechar): Add a check that |draw_rectangle| is the full size of the
  // framebuffer the next time this is called after Reshape().

  draw_rectangle_.emplace(draw_rectangle);
  has_set_draw_rectangle_for_frame_ = true;
}

void SkiaOutputSurfaceImpl::SetEnableDCLayers(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(capabilities().supports_dc_layers);

  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetEnableDCLayers,
                             base::Unretained(impl_on_gpu_.get()), enable);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::DiscardBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::RecreateRootDDLRecorder() {
  if (graphite_recorder_) {
    return;
  }
  GrSurfaceCharacterization characterization =
      CreateGrSurfaceCharacterizationCurrentFrame(
          size_, color_type_, alpha_type_, /*mipmap=*/false, sk_color_space_);
  CHECK(characterization.isValid());
  root_ddl_recorder_.emplace(characterization);
  // This will trigger the lazy initialization of the recorder
  std::ignore = root_ddl_recorder_->getCanvas();
  reset_ddl_recorder_on_swap_ = false;
}

void SkiaOutputSurfaceImpl::Reshape(const ReshapeParams& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!params.size.IsEmpty());
  DCHECK(params.alpha_type == kPremul_SkAlphaType ||
         params.alpha_type == kOpaque_SkAlphaType);

  size_ = params.size;
  format_ = GetSharedImageFormat(params.format);
  alpha_type_ = params.alpha_type;

  const auto format_index = static_cast<int>(params.format);
  color_type_ = capabilities_.sk_color_types[format_index];
  DCHECK(color_type_ != kUnknown_SkColorType)
      << "SkColorType is invalid for buffer format_index: " << format_index;

  sk_color_space_ = params.color_space.ToSkColorSpace();

  // SetDrawRectangle() will need to be called at the new size.
  has_set_draw_rectangle_for_frame_ = false;

  if (use_damage_area_from_skia_output_device_) {
    damage_of_current_buffer_ = gfx::Rect(size_);
  } else if (frame_buffer_damage_tracker_) {
    frame_buffer_damage_tracker_->FrameBuffersChanged(size_);
  }

  if (is_using_raw_draw_ && is_raw_draw_using_msaa_) {
    if (base::SysInfo::IsLowEndDevice()) {
      // On "low-end" devices use 4 samples per pixel to save memory.
      sample_count_ = 4;
    } else {
      sample_count_ = params.device_scale_factor >= 2.0f ? 4 : 8;
    }
  } else {
    sample_count_ = 1;
  }

  SkImageInfo image_info = SkImageInfo::Make(
      size_.width(), size_.height(), color_type_, alpha_type_, sk_color_space_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::Reshape,
                             base::Unretained(impl_on_gpu_.get()), image_info,
                             params.color_space, sample_count_,
                             params.device_scale_factor, GetDisplayTransform());
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());
  FlushGpuTasks(SyncMode::kNoWait);

  RecreateRootDDLRecorder();
}

void SkiaOutputSurfaceImpl::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

void SkiaOutputSurfaceImpl::SetGpuVSyncEnabled(bool enabled) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetGpuVSyncEnabled,
                             base::Unretained(impl_on_gpu_.get()), enabled);
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(task), {});
}

void SkiaOutputSurfaceImpl::SetGpuVSyncCallback(GpuVSyncCallback callback) {
  gpu_vsync_callback_ = std::move(callback);
}

void SkiaOutputSurfaceImpl::SetVSyncDisplayID(int64_t display_id) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetVSyncDisplayID,
                             base::Unretained(impl_on_gpu_.get()), display_id);
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(task), {});
}

void SkiaOutputSurfaceImpl::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

gfx::OverlayTransform SkiaOutputSurfaceImpl::GetDisplayTransform() {
  switch (capabilities_.orientation_mode) {
    case OutputSurface::OrientationMode::kLogic:
      return gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
    case OutputSurface::OrientationMode::kHardware:
      return display_transform_;
  }
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  CHECK(!current_paint_);
  CHECK(root_ddl_recorder_ || graphite_recorder_);
  if (graphite_recorder_) {
    // TODO(crbug.com/1434131): Use correct alpha type once Graphite supports
    // non-premultiplied alpha surfaces.
    SkImageInfo image_info =
        SkImageInfo::Make(gfx::SizeToSkISize(size_), color_type_,
                          kPremul_SkAlphaType, sk_color_space_);
    // Surfaceless output devices allocate shared image behind the scenes. On
    // the GPU thread this is treated same as regular IOSurfaces for the
    // purpose of creating Graphite TextureInfo i.e. it will have CopySrc and
    // CopyDst usage. So don't treat it like a root surface which generally
    // won't have or support those usages.
    skgpu::graphite::TextureInfo texture_info =
        gpu::GetGraphiteTextureInfo(dependency_->gr_context_type(), format_);
    CHECK(texture_info.isValid());
    current_paint_.emplace(graphite_recorder_, image_info, texture_info);
  } else {
    reset_ddl_recorder_on_swap_ = true;
    current_paint_.emplace(&root_ddl_recorder_.value());
  }
  return current_paint_->canvas();
}

void SkiaOutputSurfaceImpl::MakePromiseSkImage(
    ImageContext* image_context,
    const gfx::ColorSpace& yuv_color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(!image_context->mailbox_holder().mailbox.IsZero());
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImpl::MakePromiseSkImage");

  ImageContextImpl* image_context_impl =
      static_cast<ImageContextImpl*>(image_context);
  images_in_current_paint_.push_back(image_context_impl);

  const auto& mailbox_holder = image_context->mailbox_holder();

  if (is_using_raw_draw_) {
    auto* sync_point_manager = dependency_->GetSyncPointManager();
    auto const& sync_token = mailbox_holder.sync_token;
    if (sync_token.HasData() &&
        !sync_point_manager->IsSyncTokenReleased(sync_token)) {
      gpu_task_sync_tokens_.push_back(sync_token);
      FlushGpuTasks(SyncMode::kWaitForTasksStarted);
      image_context->mutable_mailbox_holder()->sync_token.Clear();
    }
    CHECK(representation_factory_);
    if (image_context_impl->BeginRasterAccess(representation_factory_.get())) {
      return;
    }
  }

  if (image_context->has_image())
    return;

  auto format = image_context->format();
  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    MakePromiseSkImageSinglePlane(image_context_impl, /*mipmapped=*/false);
  } else {
    MakePromiseSkImageMultiPlane(image_context_impl, yuv_color_space);
  }

  if (mailbox_holder.sync_token.HasData()) {
    resource_sync_tokens_.push_back(mailbox_holder.sync_token);
    image_context->mutable_mailbox_holder()->sync_token.Clear();
  }
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromYUV(
    const std::vector<ImageContext*>& contexts,
    sk_sp<SkColorSpace> image_color_space,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config)) ==
         contexts.size());
  auto* y_context = static_cast<ImageContextImpl*>(contexts[0]);
  // Note: YUV to RGB conversion is handled by a color filter in SkiaRenderer.
  SkYUVAInfo yuva_info(gfx::SizeToSkISize(y_context->size()), plane_config,
                       subsampling, kIdentity_SkYUVColorSpace);
  sk_sp<SkImage> image;
  if (graphite_recorder_) {
    std::array<skgpu::graphite::TextureInfo, SkYUVAInfo::kMaxPlanes>
        texture_infos;
    void* fulfills[SkYUVAInfo::kMaxPlanes] = {};
    for (size_t i = 0; i < contexts.size(); ++i) {
      auto* context = static_cast<ImageContextImpl*>(contexts[i]);
      auto format = context->format();
      // NOTE: We don't have promises for individual planes, but still need
      // texture info for fallback. Fallback textures are not considered YUV
      // planes since they are allocated separately and need write usage.
      context->SetImage(nullptr, {gpu::GetGraphiteTextureInfo(
                                     dependency_->gr_context_type(), format)});

      texture_infos[i] =
          gpu::GetGraphiteTextureInfo(dependency_->gr_context_type(), format,
                                      /*plane_index=*/0, /*is_yuv_plane=*/true);
      fulfills[i] = new FulfillForPlane(context);
    }
    skgpu::graphite::YUVABackendTextureInfo yuva_backend_info(
        graphite_recorder_, yuva_info, texture_infos, skgpu::Mipmapped::kNo);
    image = SkImages::PromiseTextureFromYUVA(
        graphite_recorder_, yuva_backend_info, std::move(image_color_space),
        skgpu::graphite::Volatile::kYes, FulfillGraphite, CleanUp,
        ReleaseGraphite, {}, fulfills);
  } else {
    GrBackendFormat formats[SkYUVAInfo::kMaxPlanes] = {};
    void* fulfills[SkYUVAInfo::kMaxPlanes] = {};
    for (size_t i = 0; i < contexts.size(); ++i) {
      auto* context = static_cast<ImageContextImpl*>(contexts[i]);
      formats[i] =
          GetGrBackendFormatForTexture(context->format(), /*plane_index=*/0,
                                       context->mailbox_holder().texture_target,
                                       /*ycbcr_info=*/absl::nullopt);
      // NOTE: We don't have promises for individual planes, but still need
      // format for fallback.
      context->SetImage(nullptr, {formats[i]});
      fulfills[i] = new FulfillForPlane(context);
    }
    GrYUVABackendTextureInfo yuva_backend_info(
        yuva_info, formats, GrMipmapped::kNo, kTopLeft_GrSurfaceOrigin);
    image = SkImages::PromiseTextureFromYUVA(
        gr_context_thread_safe_, yuva_backend_info,
        std::move(image_color_space), FulfillGanesh, CleanUp, fulfills);
  }
  for (size_t i = 0; i < contexts.size(); ++i) {
    auto* context = static_cast<ImageContextImpl*>(contexts[i]);
    CHECK_EQ(context->origin(), kTopLeft_GrSurfaceOrigin);
    if (context->mailbox_holder().sync_token.HasData()) {
      resource_sync_tokens_.push_back(context->mailbox_holder().sync_token);
      context->mutable_mailbox_holder()->sync_token.Clear();
    }
    images_in_current_paint_.push_back(context);
  }
  CHECK(image);
  return image;
}

void SkiaOutputSurfaceImpl::MakePromiseSkImageSinglePlane(
    ImageContextImpl* image_context,
    bool mipmap) {
  CHECK(!image_context->has_image());
  auto format = image_context->format();
  CHECK(format.is_single_plane() || format.PrefersExternalSampler());
  FulfillForPlane* fulfill = new FulfillForPlane(image_context);
  SkColorType color_type =
      ToClosestSkColorType(/*gpu_compositing=*/true, format);
  if (graphite_recorder_) {
    skgpu::graphite::TextureInfo texture_info = gpu::GetGraphiteTextureInfo(
        dependency_->gr_context_type(), format, /*plane_index=*/0,
        /*is_yuv_plane=*/false, mipmap);
    SkColorInfo color_info(color_type, image_context->alpha_type(),
                           image_context->color_space());
    auto image = SkImages::PromiseTextureFrom(
        graphite_recorder_, gfx::SizeToSkISize(image_context->size()),
        texture_info, color_info, skgpu::graphite::Volatile::kYes,
        FulfillGraphite, CleanUp, ReleaseGraphite, fulfill);
    image_context->SetImage(std::move(image), {texture_info});
  } else {
    CHECK(gr_context_thread_safe_);
    GrBackendFormat backend_format = GetGrBackendFormatForTexture(
        format, /*plane_index=*/0,
        image_context->mailbox_holder().texture_target,
        image_context->ycbcr_info());
    auto image = SkImages::PromiseTextureFrom(
        gr_context_thread_safe_, backend_format,
        gfx::SizeToSkISize(image_context->size()),
        mipmap ? GrMipMapped::kYes : GrMipMapped::kNo, image_context->origin(),
        color_type, image_context->alpha_type(), image_context->color_space(),
        FulfillGanesh, CleanUp, fulfill);
    image_context->SetImage(std::move(image), {backend_format});
  }
}

void SkiaOutputSurfaceImpl::MakePromiseSkImageMultiPlane(
    ImageContextImpl* image_context,
    const gfx::ColorSpace& yuv_color_space) {
  CHECK(!image_context->has_image());
  auto format = image_context->format();
  CHECK(format.is_multi_plane());
  SkYUVAInfo::PlaneConfig plane_config = gpu::ToSkYUVAPlaneConfig(format);
  SkYUVAInfo::Subsampling subsampling = gpu::ToSkYUVASubsampling(format);
  // TODO(crbug.com/828599): This should really default to rec709.
  SkYUVColorSpace sk_yuv_color_space = kRec601_SkYUVColorSpace;
  yuv_color_space.ToSkYUVColorSpace(format.MultiplanarBitDepth(),
                                    &sk_yuv_color_space);
  SkYUVAInfo yuva_info(gfx::SizeToSkISize(image_context->size()), plane_config,
                       subsampling, sk_yuv_color_space);
  if (graphite_recorder_) {
    std::vector<skgpu::graphite::TextureInfo> texture_infos;
    void* fulfills[SkYUVAInfo::kMaxPlanes] = {};
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      CHECK_EQ(image_context->origin(), kTopLeft_GrSurfaceOrigin);
      fulfills[plane_index] = new FulfillForPlane(image_context, plane_index);
      texture_infos.emplace_back(gpu::GetGraphiteTextureInfo(
          dependency_->gr_context_type(), format, plane_index));
    }

    skgpu::graphite::YUVABackendTextureInfo yuva_backend_info(
        graphite_recorder_, yuva_info, texture_infos, skgpu::Mipmapped::kNo);
    auto image = SkImages::PromiseTextureFromYUVA(
        graphite_recorder_, yuva_backend_info, image_context->color_space(),
        skgpu::graphite::Volatile::kYes, FulfillGraphite, CleanUp,
        ReleaseGraphite, {}, fulfills);
    DCHECK(image);
    image_context->SetImage(std::move(image), std::move(texture_infos));
  } else {
    CHECK(gr_context_thread_safe_);
    std::vector<GrBackendFormat> formats;
    void* fulfills[SkYUVAInfo::kMaxPlanes] = {};
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         ++plane_index) {
      CHECK_EQ(image_context->origin(), kTopLeft_GrSurfaceOrigin);
      formats.push_back(GetGrBackendFormatForTexture(
          format, plane_index, image_context->mailbox_holder().texture_target,
          image_context->ycbcr_info()));
      fulfills[plane_index] = new FulfillForPlane(image_context, plane_index);
    }

    GrYUVABackendTextureInfo yuva_backend_info(
        yuva_info, formats.data(), GrMipmapped::kNo, kTopLeft_GrSurfaceOrigin);
    auto image = SkImages::PromiseTextureFromYUVA(
        gr_context_thread_safe_, yuva_backend_info,
        image_context->color_space(), FulfillGanesh, CleanUp, fulfills);
    DCHECK(image);
    image_context->SetImage(std::move(image), std::move(formats));
  }
}

gpu::SyncToken SkiaOutputSurfaceImpl::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  if (image_contexts.empty())
    return gpu::SyncToken();

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts,
      base::Unretained(impl_on_gpu_.get()), std::move(image_contexts));
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
  return Flush();
}

std::unique_ptr<ExternalUseClient::ImageContext>
SkiaOutputSurfaceImpl::CreateImageContext(
    const gpu::MailboxHolder& holder,
    const gfx::Size& size,
    SharedImageFormat format,
    bool maybe_concurrent_reads,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool raw_draw_if_possible) {
  return std::make_unique<ImageContextImpl>(
      holder, size, format, maybe_concurrent_reads, ycbcr_info,
      std::move(color_space),
      /*is_for_render_pass=*/false, raw_draw_if_possible);
}

void SkiaOutputSurfaceImpl::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!current_paint_);
  // If the renderer allocates images then `current_buffer_modified_` isn't
  // updated, and we can't make this check.
  DCHECK(capabilities_.renderer_allocates_images ||
         ((!frame.sub_buffer_rect || !frame.sub_buffer_rect->IsEmpty()) ==
          current_buffer_modified_));

  has_set_draw_rectangle_for_frame_ = false;

  // If current_buffer_modified_ is false, it means SkiaRenderer doesn't draw
  // anything for current frame. So this SwapBuffer() must be a empty swap, so
  // the previous buffer will be used for this frame.
  if (frame_buffer_damage_tracker_ && current_buffer_modified_) {
    gfx::Rect damage_rect =
        frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(size_);
    frame_buffer_damage_tracker_->SwappedWithDamage(damage_rect);
  }
  current_buffer_modified_ = false;

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffers,
                     base::Unretained(impl_on_gpu_.get()), std::move(frame));
  EnqueueGpuTask(std::move(callback), std::move(resource_sync_tokens_),
                 /*make_current=*/true,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());

  // Recreate |root_ddl_recorder_| after SwapBuffers has been scheduled on GPU
  // thread to save some time in BeginPaintCurrentFrame
  // Recreating recorder is expensive. Avoid recreation if there was no paint.
  if (reset_ddl_recorder_on_swap_) {
    RecreateRootDDLRecorder();
  }
}

void SkiaOutputSurfaceImpl::SwapBuffersSkipped(
    const gfx::Rect root_pass_damage_rect) {
  if (current_buffer_modified_ && frame_buffer_damage_tracker_) {
    // If |current_buffer_modified_| is true but we skipped swap there is still
    // damage to the current framebuffer to account for. Unlike SwapBuffers()
    // don't reset current buffers rect, since that damage still need to be
    // taken into account when the buffer is swapped later.
    frame_buffer_damage_tracker_->SkippedSwapWithDamage(root_pass_damage_rect);
  }
  current_buffer_modified_ = false;

  // PostTask to the GPU thread to deal with freeing resources and running
  // callbacks.
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffersSkipped,
                             base::Unretained(impl_on_gpu_.get()));
  // SwapBuffersSkipped currently does mostly the same as SwapBuffers and needs
  // MakeCurrent.
  EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                 /*make_current=*/true, /*need_framebuffer=*/false);

  // Recreating recorder is expensive. Avoid recreation if there was no paint.
  if (reset_ddl_recorder_on_swap_) {
    RecreateRootDDLRecorder();
  }
}

void SkiaOutputSurfaceImpl::ScheduleOutputSurfaceAsOverlay(
    OverlayProcessorInterface::OutputSurfaceOverlayPlane output_surface_plane) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay,
      base::Unretained(impl_on_gpu_.get()), std::move(output_surface_plane));
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& surface_size,
    SharedImageFormat format,
    bool mipmap,
    bool scanout_dcomp_surface,
    sk_sp<SkColorSpace> color_space,
    bool is_overlay,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  CHECK(!current_paint_);
  CHECK(resource_sync_tokens_.empty());

  SkColorType color_type =
      ToClosestSkColorType(/*gpu_compositing=*/true, format);
  if (graphite_recorder_) {
    SkImageInfo image_info =
        SkImageInfo::Make(gfx::SizeToSkISize(surface_size), color_type,
                          kPremul_SkAlphaType, color_space);
    skgpu::graphite::TextureInfo texture_info =
        gpu::GetGraphiteTextureInfo(dependency_->gr_context_type(), format);
    if (!texture_info.isValid()) {
      DLOG(ERROR) << "BeginPaintRenderPass: invalid Graphite TextureInfo";
      return nullptr;
    }
    current_paint_.emplace(graphite_recorder_, image_info, texture_info,
                           mailbox);
  } else {
    GrSurfaceCharacterization characterization =
        CreateGrSurfaceCharacterizationRenderPass(
            surface_size, color_type, kPremul_SkAlphaType, mipmap,
            std::move(color_space), is_overlay, scanout_dcomp_surface);
    if (!characterization.isValid()) {
      DLOG(ERROR) << "BeginPaintRenderPass: invalid GrSurfaceCharacterization";
      return nullptr;
    }
    current_paint_.emplace(characterization, mailbox);
  }

  // We are going to overwrite the render pass when it is not for overlay, so we
  // need to reset the image_context and a new promise image will be created
  // when MakePromiseSkImageFromRenderPass() is called.
  if (!is_overlay) {
    auto it = render_pass_image_cache_.find(id);
    if (it != render_pass_image_cache_.end()) {
      it->second->clear_image();
    }
  }

  return current_paint_->canvas();
}

SkCanvas* SkiaOutputSurfaceImpl::RecordOverdrawForCurrentPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(debug_settings_->show_overdraw_feedback);
  DCHECK(current_paint_);
  DCHECK(!overdraw_surface_ddl_recorder_);

  nway_canvas_.emplace(size_.width(), size_.height());
  nway_canvas_->addCanvas(current_paint_->canvas());

  // Overdraw feedback uses |SkOverdrawCanvas|, which relies on a buffer with an
  // 8-bit unorm alpha channel to work. RGBA8 is always supported, so we use it.
  SkColorType color_type_with_alpha = SkColorType::kRGBA_8888_SkColorType;

  GrSurfaceCharacterization characterization =
      CreateGrSurfaceCharacterizationRenderPass(
          size_, color_type_with_alpha, alpha_type_, /*mipmap=*/false,
          sk_color_space_, /*is_overlay=*/false,
          /*scanout_dcomp_surface=*/false);
  if (characterization.isValid()) {
    overdraw_surface_ddl_recorder_.emplace(characterization);
    overdraw_canvas_.emplace((overdraw_surface_ddl_recorder_->getCanvas()));
    nway_canvas_->addCanvas(&overdraw_canvas_.value());
  }

  return &nway_canvas_.value();
}

void SkiaOutputSurfaceImpl::EndPaint(
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
    const gfx::Rect& update_rect,
    bool is_overlay) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(current_paint_);

  sk_sp<GrDeferredDisplayList> ddl;
  sk_sp<GrDeferredDisplayList> overdraw_ddl;
  std::unique_ptr<skgpu::graphite::Recording> graphite_recording;

  if (graphite_recorder_) {
    graphite_recording = current_paint_->SnapRecording();
  } else {
    ddl = current_paint_->DetachDDL();

    if (overdraw_surface_ddl_recorder_) {
      overdraw_ddl = overdraw_surface_ddl_recorder_->detach();
      DCHECK(overdraw_ddl);
      overdraw_canvas_.reset();
      overdraw_surface_ddl_recorder_.reset();
      nway_canvas_.reset();
    }
  }

  // If the current paint mailbox is empty, we are painting a frame, otherwise
  // we are painting a render pass. impl_on_gpu_ is released on the GPU thread
  // by a posted task from SkiaOutputSurfaceImpl::dtor, so it is safe to use
  // base::Unretained.
  if (current_paint_->mailbox().IsZero()) {
    // Draw on the root render pass.
    current_buffer_modified_ = true;
    auto task =
        base::BindOnce(&SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame,
                       base::Unretained(impl_on_gpu_.get()), std::move(ddl),
                       std::move(overdraw_ddl), std::move(graphite_recording),
                       std::move(images_in_current_paint_),
                       resource_sync_tokens_, std::move(on_finished),
                       std::move(return_release_fence_cb), draw_rectangle_);
    EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                   /*make_current=*/true, /*need_framebuffer=*/true);
    draw_rectangle_.reset();
  } else {
    auto task = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass,
        base::Unretained(impl_on_gpu_.get()), current_paint_->mailbox(),
        std::move(ddl), std::move(overdraw_ddl), std::move(graphite_recording),
        std::move(images_in_current_paint_), resource_sync_tokens_,
        std::move(on_finished), std::move(return_release_fence_cb), update_rect,
        is_overlay);
    EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                   /*make_current=*/true, /*need_framebuffer=*/false);
  }
  images_in_current_paint_.clear();
  current_paint_.reset();
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& size,
    SharedImageFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);

  auto& image_context = render_pass_image_cache_[id];
  if (!image_context) {
    gpu::MailboxHolder mailbox_holder(mailbox, gpu::SyncToken(), GL_TEXTURE_2D);
    image_context = std::make_unique<ImageContextImpl>(
        mailbox_holder, size, format, /*maybe_concurrent_reads=*/false,
        /*ycbcr_info=*/absl::nullopt, std::move(color_space),
        /*is_for_render_pass=*/true);
  }
  if (!image_context->has_image()) {
    MakePromiseSkImageSinglePlane(image_context.get(), mipmap);
    if (!image_context->has_image()) {
      return nullptr;
    }
  }
  images_in_current_paint_.push_back(image_context.get());
  return image_context->image();
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<AggregatedRenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  std::vector<std::unique_ptr<ExternalUseClient::ImageContext>> image_contexts;
  image_contexts.reserve(ids.size());
  for (const auto id : ids) {
    auto it = render_pass_image_cache_.find(id);
    // If the render pass was only used for a copy request, there won't be a
    // matching entry in |render_pass_image_cache_|.
    if (it != render_pass_image_cache_.end()) {
      it->second->clear_image();
      image_contexts.push_back(std::move(it->second));
      render_pass_image_cache_.erase(it);
    }
  }

  if (!image_contexts.empty()) {
    // impl_on_gpu_ is released on the GPU thread by a posted task from
    // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
    auto callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts,
        base::Unretained(impl_on_gpu_.get()), std::move(image_contexts));
    // ReleaseImageContexts will delete gpu resources and needs MakeCurrent.
    EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                   /*need_framebuffer=*/false);
  }
}

void SkiaOutputSurfaceImpl::CopyOutput(
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (request->has_blit_request()) {
    for (const auto& mailbox_holder : request->blit_request().mailboxes()) {
      if (mailbox_holder.sync_token.HasData()) {
        resource_sync_tokens_.push_back(mailbox_holder.sync_token);
      }
    }
  }

  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CopyOutput,
                                 base::Unretained(impl_on_gpu_.get()), geometry,
                                 color_space, std::move(request), mailbox);
  EnqueueGpuTask(std::move(callback), std::move(resource_sync_tokens_),
                 /*make_current=*/true, /*need_framebuffer=*/mailbox.IsZero());
}

void SkiaOutputSurfaceImpl::ScheduleOverlays(
    OverlayList overlays,
    std::vector<gpu::SyncToken> sync_tokens) {
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::ScheduleOverlays,
                     base::Unretained(impl_on_gpu_.get()), std::move(overlays));
  EnqueueGpuTask(std::move(task), std::move(sync_tokens),
                 /*make_current=*/false, /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::SetFrameRate(float frame_rate) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetFrameRate,
                             base::Unretained(impl_on_gpu_.get()), frame_rate);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::SetCapabilitiesForTesting(
    gfx::SurfaceOrigin output_surface_origin) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(impl_on_gpu_);
  capabilities_.output_surface_origin = output_surface_origin;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting,
                     base::Unretained(impl_on_gpu_.get()), capabilities_);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
}

bool SkiaOutputSurfaceImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("viz", __PRETTY_FUNCTION__);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();

  // This runner could be called from vsync or GPU thread after |this| is
  // destroyed. We post directly to display compositor thread to check
  // |weak_ptr_| as |dependency_| may have been destroyed.
#if BUILDFLAG(IS_ANDROID)
  // Callback is never used on Android. Doesn't work with WebView because
  // calling it bypasses SkiaOutputSurfaceDependency.
  GpuVSyncCallback vsync_callback_runner = base::DoNothing();
#else
  GpuVSyncCallback vsync_callback_runner = base::BindRepeating(
      [](scoped_refptr<base::SingleThreadTaskRunner> runner,
         base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr,
         base::TimeTicks timebase, base::TimeDelta interval) {
        runner->PostTask(FROM_HERE,
                         base::BindOnce(&SkiaOutputSurfaceImpl::OnGpuVSync,
                                        weak_ptr, timebase, interval));
      },
      base::SingleThreadTaskRunner::GetCurrentDefault(), weak_ptr_);
#endif

  bool result = false;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                     base::Unretained(this), vsync_callback_runner, &result);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  // |capabilities_| will be initialized in InitializeOnGpuThread(), so have to
  // wait.
  FlushGpuTasks(SyncMode::kWaitForTasksFinished);

  if (capabilities_.preserve_buffer_content &&
      capabilities_.supports_post_sub_buffer) {
    capabilities_.only_invalidates_damage_rect = false;
    // If there is only one pending frame, then we can use damage area hint from
    // SkiaOutputDevice, otherwise we have to track damage area with
    // FrameBufferDamageTracker.
    if (capabilities_.pending_swap_params.max_pending_swaps == 1 &&
        capabilities_.damage_area_from_skia_output_device) {
      use_damage_area_from_skia_output_device_ = true;
      damage_of_current_buffer_ = gfx::Rect();
    } else if (!capabilities_.renderer_allocates_images) {
      // We don't need a damage tracker if SkiaRenderer allocates the images,
      // because it will keep track of the damage as well.
      frame_buffer_damage_tracker_.emplace(capabilities_.number_of_buffers);
    }
  }
  return result;
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(
    GpuVSyncCallback vsync_callback_runner,
    bool* result) {
  auto did_swap_buffer_complete_callback = base::BindRepeating(
      &SkiaOutputSurfaceImpl::DidSwapBuffersComplete, weak_ptr_);
  auto buffer_presented_callback =
      base::BindRepeating(&SkiaOutputSurfaceImpl::BufferPresented, weak_ptr_);
  auto context_lost_callback =
      base::BindOnce(&SkiaOutputSurfaceImpl::ContextLost, weak_ptr_);
  auto schedule_gpu_task = base::BindRepeating(
      &SkiaOutputSurfaceImpl::ScheduleOrRetainGpuTask, weak_ptr_);
  auto add_child_window_to_browser_callback = base::BindRepeating(
      &SkiaOutputSurfaceImpl::AddChildWindowToBrowser, weak_ptr_);

  impl_on_gpu_ = SkiaOutputSurfaceImplOnGpu::Create(
      dependency_, renderer_settings_, gpu_task_scheduler_->GetSequenceId(),
      display_compositor_controller_->controller_on_gpu(),
      std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(schedule_gpu_task), std::move(vsync_callback_runner),
      std::move(add_child_window_to_browser_callback));
  if (!impl_on_gpu_) {
    *result = false;
    return;
  }
  capabilities_ = impl_on_gpu_->capabilities();
  is_displayed_as_overlay_ = impl_on_gpu_->IsDisplayedAsOverlay();

  if (auto* gr_context = dependency_->GetSharedContextState()->gr_context()) {
    gr_context_thread_safe_ = gr_context->threadSafeProxy();
  }
  graphite_recorder_ =
      dependency_->GetSharedContextState()->viz_compositor_graphite_recorder();

  *result = true;
}

GrSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateGrSurfaceCharacterizationRenderPass(
    const gfx::Size& surface_size,
    SkColorType color_type,
    SkAlphaType alpha_type,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    bool is_overlay,
    bool scanout_dcomp_surface) const {
  if (!gr_context_thread_safe_) {
    DLOG(ERROR) << "gr_context_thread_safe_ is null.";
    return GrSurfaceCharacterization();
  }

  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
  const int sample_count = std::min(
      sample_count_,
      gr_context_thread_safe_->maxSurfaceSampleCountForColorType(color_type));
  auto backend_format = gr_context_thread_safe_->defaultBackendFormat(
      color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid());
#if BUILDFLAG(IS_APPLE)
  if (is_overlay) {
    DCHECK_EQ(dependency_->gr_context_type(), gpu::GrContextType::kGL);
    // For overlay, IOSurface will be used, and we may need using
    // GL_TEXTURE_RECTANGLE_ARB as texture target.
    backend_format =
        GrBackendFormat::MakeGL(backend_format.asGLFormatEnum(),
                                gpu::GetPlatformSpecificTextureTarget());
  }
#endif
  auto image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        alpha_type, std::move(color_space));

  // Skia draws to DComp surfaces by binding them to GLFB0, since the surfaces
  // are write-only and cannot be wrapped as a GL texture.
  if (scanout_dcomp_surface) {
    DCHECK_EQ(backend_format.backend(), GrBackendApi::kOpenGL);
  }

  auto characterization = gr_context_thread_safe_->createCharacterization(
      cache_max_resource_bytes, image_info, backend_format, sample_count,
      kTopLeft_GrSurfaceOrigin, surface_props, mipmap,
      /*willUseGLFBO0=*/scanout_dcomp_surface,
      /*isTextureable=*/!scanout_dcomp_surface, GrProtected::kNo);
  DCHECK(characterization.isValid());
  return characterization;
}

GrSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateGrSurfaceCharacterizationCurrentFrame(
    const gfx::Size& surface_size,
    SkColorType color_type,
    SkAlphaType alpha_type,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) const {
  if (!gr_context_thread_safe_) {
    DLOG(ERROR) << "gr_context_thread_safe_ is null.";
    return GrSurfaceCharacterization();
  }

  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
  int sample_count = std::min(
      sample_count_,
      gr_context_thread_safe_->maxSurfaceSampleCountForColorType(color_type));
  auto backend_format = gr_context_thread_safe_->defaultBackendFormat(
      color_type, GrRenderable::kYes);
#if BUILDFLAG(IS_MAC)
  DCHECK_EQ(dependency_->gr_context_type(), gpu::GrContextType::kGL);
  // For root rander pass, IOSurface will be used, and we may need using
  // GL_TEXTURE_RECTANGLE_ARB as texture target.
  backend_format = GrBackendFormat::MakeGL(
      backend_format.asGLFormatEnum(), gpu::GetPlatformSpecificTextureTarget());
#endif
  DCHECK(backend_format.isValid())
      << "GrBackendFormat is invalid for color_type: " << color_type;
  auto surface_origin =
      capabilities_.output_surface_origin == gfx::SurfaceOrigin::kBottomLeft
          ? kBottomLeft_GrSurfaceOrigin
          : kTopLeft_GrSurfaceOrigin;
  auto image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        alpha_type, std::move(color_space));
  DCHECK((capabilities_.uses_default_gl_framebuffer &&
          dependency_->gr_context_type() == gpu::GrContextType::kGL) ||
         !capabilities_.uses_default_gl_framebuffer);
  // Skia doesn't support set desired MSAA count for default gl framebuffer.
  if (capabilities_.uses_default_gl_framebuffer) {
    sample_count = 1;
  }
  bool is_textureable = !capabilities_.uses_default_gl_framebuffer &&
                        !capabilities_.root_is_vulkan_secondary_command_buffer;
  auto characterization = gr_context_thread_safe_->createCharacterization(
      cache_max_resource_bytes, image_info, backend_format, sample_count,
      surface_origin, surface_props, mipmap,
      capabilities_.uses_default_gl_framebuffer, is_textureable,
      GrProtected::kNo, /*vkRTSupportsInputAttachment=*/false,
      capabilities_.root_is_vulkan_secondary_command_buffer);
#if BUILDFLAG(ENABLE_VULKAN)
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
#endif
  LOG_IF(DFATAL, !characterization.isValid())
      << "\n  surface_size=" << surface_size.ToString()
      << "\n  format=" << static_cast<int>(color_type)
      << "\n  color_type=" << static_cast<int>(color_type)
      << "\n  backend_format.isValid()=" << backend_format.isValid()
      << "\n  backend_format.backend()="
      << static_cast<int>(backend_format.backend())
      << "\n  backend_format.asGLFormat()="
      << static_cast<int>(backend_format.asGLFormat())
#if BUILDFLAG(ENABLE_VULKAN)
      << "\n  backend_format.asVkFormat()="
      << static_cast<int>(backend_format.asVkFormat(&vk_format))
      << "\n  backend_format.asVkFormat() vk_format="
      << static_cast<int>(vk_format)
#endif
      << "\n  sample_count=" << sample_count
      << "\n  surface_origin=" << static_cast<int>(surface_origin)
      << "\n  willGlFBO0=" << capabilities_.uses_default_gl_framebuffer;
  return characterization;
}

void SkiaOutputSurfaceImpl::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params,
    const gfx::Size& pixel_size,
    gfx::GpuFenceHandle release_fence) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  last_swapped_mailbox_ = params.primary_plane_mailbox;

  if (frame_buffer_damage_tracker_ &&
      params.swap_response.result ==
          gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    frame_buffer_damage_tracker_->FrameBuffersChanged(size_);
  }

  if (use_damage_area_from_skia_output_device_) {
    damage_of_current_buffer_ = params.frame_buffer_damage_area;
    DCHECK(damage_of_current_buffer_);
  }

  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  client_->DidReceiveSwapBuffersAck(params, std::move(release_fence));
  if (!params.released_overlays.empty())
    client_->DidReceiveReleasedOverlays(params.released_overlays);
  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void SkiaOutputSurfaceImpl::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  client_->DidReceivePresentationFeedback(feedback);
  if (update_vsync_parameters_callback_ &&
      (feedback.flags & gfx::PresentationFeedback::kVSync ||
       refresh_interval_ != feedback.interval)) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    update_vsync_parameters_callback_.Run(
        feedback.timestamp, feedback.interval.is_zero()
                                ? BeginFrameArgs::DefaultInterval()
                                : feedback.interval);
    // Update |refresh_interval_|, so we only update when interval is changed.
    refresh_interval_ = feedback.interval;
  }
}

void SkiaOutputSurfaceImpl::AddChildWindowToBrowser(
    gpu::SurfaceHandle child_window) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  client_->AddChildWindowToBrowser(child_window);
}

void SkiaOutputSurfaceImpl::OnGpuVSync(base::TimeTicks timebase,
                                       base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (gpu_vsync_callback_)
    gpu_vsync_callback_.Run(timebase, interval);
}

void SkiaOutputSurfaceImpl::ScheduleGpuTaskForTesting(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  EnqueueGpuTask(std::move(callback), std::move(sync_tokens),
                 /*make_current=*/false, /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
}

void SkiaOutputSurfaceImpl::EnqueueGpuTask(
    GpuTask task,
    std::vector<gpu::SyncToken> sync_tokens,
    bool make_current,
    bool need_framebuffer) {
  gpu_tasks_.push_back(std::move(task));
  std::move(sync_tokens.begin(), sync_tokens.end(),
            std::back_inserter(gpu_task_sync_tokens_));

  // Set |make_current_|, so MakeCurrent() will be called before executing all
  // enqueued GPU tasks.
  make_current_ |= make_current;
  need_framebuffer_ |= need_framebuffer;
}

void SkiaOutputSurfaceImpl::FlushGpuTasks(SyncMode sync_mode) {
  FlushGpuTasksWithImpl(sync_mode, impl_on_gpu_.get());
}

void SkiaOutputSurfaceImpl::FlushGpuTasksWithImpl(
    SyncMode sync_mode,
    SkiaOutputSurfaceImplOnGpu* impl_on_gpu) {
  TRACE_EVENT1("viz", "SkiaOutputSurfaceImpl::FlushGpuTasks", "sync_mode",
               sync_mode);

  // impl_on_gpu will only be null during initialization. If we need to make
  // context current, or measure timings then impl_on_gpu must exist.
  DCHECK(impl_on_gpu || (!make_current_ && !should_measure_next_post_task_));

  // If |wait_for_finish| is true, a GPU task will be always scheduled to make
  // sure all pending tasks are finished on the GPU thread.
  if (gpu_tasks_.empty() && sync_mode == SyncMode::kNoWait)
    return;

  auto event = sync_mode != SyncMode::kNoWait
                   ? std::make_unique<base::WaitableEvent>()
                   : nullptr;

  base::TimeTicks post_task_timestamp;
  if (should_measure_next_post_task_) {
    post_task_timestamp = base::TimeTicks::Now();
  }

  auto callback = base::BindOnce(
      [](std::vector<GpuTask> tasks, SyncMode sync_mode,
         base::WaitableEvent* event, SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
         bool make_current, bool need_framebuffer,
         base::TimeTicks post_task_timestamp) {
        if (sync_mode == SyncMode::kWaitForTasksStarted)
          event->Signal();
        gpu::ContextUrl::SetActiveUrl(GetActiveUrl());
        if (!post_task_timestamp.is_null()) {
          impl_on_gpu->SetDrawTimings(post_task_timestamp);
        }
        if (make_current) {
          // MakeCurrent() will mark context lost in SkiaOutputSurfaceImplOnGpu,
          // if it fails.
          impl_on_gpu->MakeCurrent(need_framebuffer);
        }
        // Each task can check SkiaOutputSurfaceImplOnGpu::contest_is_lost_
        // to detect errors.
        for (auto& task : tasks) {
          std::move(task).Run();
        }

        if (sync_mode == SyncMode::kWaitForTasksFinished)
          event->Signal();
      },
      std::move(gpu_tasks_), sync_mode, event.get(), impl_on_gpu, make_current_,
      need_framebuffer_, post_task_timestamp);

  gpu::GpuTaskSchedulerHelper::ReportingCallback reporting_callback;
  if (should_measure_next_post_task_) {
    // Note that the usage of base::Unretained() with the impl_on_gpu_ is
    // considered safe as it is also owned by |callback| and share the same
    // lifetime.
    reporting_callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::SetDependenciesResolvedTimings,
        base::Unretained(impl_on_gpu_.get()));
  }

  gpu_task_scheduler_->ScheduleGpuTask(std::move(callback),
                                       std::move(gpu_task_sync_tokens_),
                                       std::move(reporting_callback));

  make_current_ = false;
  need_framebuffer_ = false;
  should_measure_next_post_task_ = false;
  gpu_task_sync_tokens_.clear();
  gpu_tasks_.clear();

  if (event)
    event->Wait();
}

GrBackendFormat SkiaOutputSurfaceImpl::GetGrBackendFormatForTexture(
    SharedImageFormat si_format,
    int plane_index,
    uint32_t gl_texture_target,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (dependency_->gr_context_type() == gpu::GrContextType::kVulkan) {
    if (!ycbcr_info) {
      return GrBackendFormat::MakeVk(gpu::ToVkFormat(si_format, plane_index));
    }

    // Assume optimal tiling.
    GrVkYcbcrConversionInfo gr_ycbcr_info =
        CreateGrVkYcbcrConversionInfo(dependency_->GetVulkanContextProvider()
                                          ->GetDeviceQueue()
                                          ->GetVulkanPhysicalDevice(),
                                      VK_IMAGE_TILING_OPTIMAL, ycbcr_info);
#if BUILDFLAG(IS_LINUX)
    // Textures that were allocated _on linux_ with ycbcr info came from
    // VaapiVideoDecoder, which exports using DRM format modifiers.
    return GrBackendFormat::MakeVk(gr_ycbcr_info,
                                   /*willUseDRMFormatModifiers=*/true);
#else
    return GrBackendFormat::MakeVk(gr_ycbcr_info);
#endif  // BUILDFLAG(IS_LINUX)
  } else {
#else
  {
#endif  // BUILDFLAG(ENABLE_VULKAN)
    CHECK_EQ(dependency_->gr_context_type(), gpu::GrContextType::kGL);
    // Convert internal format from GLES2 to platform GL.
    bool use_angle_rgbx_format = impl_on_gpu_->GetFeatureInfo()
                                     ->feature_flags()
                                     .angle_rgbx_internal_format;
    auto gl_format_desc = si_format.PrefersExternalSampler()
                              ? gpu::ToGLFormatDescExternalSampler(si_format)
                              : gpu::ToGLFormatDesc(si_format, plane_index,
                                                    use_angle_rgbx_format);
    auto gl_storage_internal_format = gl_format_desc.storage_internal_format;
    unsigned int texture_storage_format = gpu::GetGrGLBackendTextureFormat(
        impl_on_gpu_->GetFeatureInfo(), gl_storage_internal_format,
        gr_context_thread_safe_);

    return GrBackendFormat::MakeGL(texture_storage_format, gl_texture_target);
  }
}

bool SkiaOutputSurfaceImpl::IsDisplayedAsOverlayPlane() const {
  return is_displayed_as_overlay_;
}

gpu::Mailbox SkiaOutputSurfaceImpl::GetOverlayMailbox() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return last_swapped_mailbox_;
}

void SkiaOutputSurfaceImpl::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}

base::ScopedClosureRunner SkiaOutputSurfaceImpl::GetCacheBackBufferCb() {
  // Note, that we call it directly on viz thread to get the callback.
  return impl_on_gpu_->GetCacheBackBufferCb();
}

void SkiaOutputSurfaceImpl::AddContextLostObserver(
    ContextLostObserver* observer) {
  observers_.AddObserver(observer);
}

void SkiaOutputSurfaceImpl::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  observers_.RemoveObserver(observer);
}

gpu::SyncToken SkiaOutputSurfaceImpl::Flush() {
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      impl_on_gpu_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::ReleaseFenceSync,
                     base::Unretained(impl_on_gpu_.get()), sync_fence_release_);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
  return sync_token;
}

bool SkiaOutputSurfaceImpl::EnsureMinNumberOfBuffers(int n) {
  DCHECK(capabilities_.supports_dynamic_frame_buffer_allocation);
  DCHECK_GT(n, 0);
  DCHECK_LE(n, capabilities_.number_of_buffers);

  if (cached_number_of_buffers_ >= n)
    return false;

  cached_number_of_buffers_ = n;
  if (frame_buffer_damage_tracker_) {
    frame_buffer_damage_tracker_->FrameBuffersChanged(size_);
  }

  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureMinNumberOfBuffers,
                     base::Unretained(impl_on_gpu_.get()), n);
  EnqueueGpuTask(std::move(task), std::vector<gpu::SyncToken>(),
                 /*make_current=*/true,
                 /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
  return true;
}

void SkiaOutputSurfaceImpl::ContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DLOG(ERROR) << "SkiaOutputSurfaceImpl::ContextLost()";
  gr_context_thread_safe_.reset();
  for (auto& observer : observers_)
    observer.OnContextLost();
}

void SkiaOutputSurfaceImpl::ScheduleOrRetainGpuTask(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> tokens) {
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(callback),
                                               std::move(tokens));
}

gfx::Rect SkiaOutputSurfaceImpl::GetCurrentFramebufferDamage() const {
  if (use_damage_area_from_skia_output_device_) {
    DCHECK(damage_of_current_buffer_);
    return *damage_of_current_buffer_;
  }

  if (!frame_buffer_damage_tracker_) {
    return gfx::Rect();
  }

  return frame_buffer_damage_tracker_->GetCurrentFrameBufferDamage();
}

void SkiaOutputSurfaceImpl::SetNeedsMeasureNextDrawLatency() {
  should_measure_next_post_task_ = true;
}

void SkiaOutputSurfaceImpl::PreserveChildSurfaceControls() {
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PreserveChildSurfaceControls,
                     base::Unretained(impl_on_gpu_.get()));
  EnqueueGpuTask(std::move(task), std::vector<gpu::SyncToken>(),
                 /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  auto task = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::InitDelegatedInkPointRendererReceiver,
      base::Unretained(impl_on_gpu_.get()), std::move(pending_receiver));
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

gpu::Mailbox SkiaOutputSurfaceImpl::CreateSharedImage(
    SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::StringPiece debug_label,
    gpu::SurfaceHandle surface_handle) {
  gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();

  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CreateSharedImage,
                             base::Unretained(impl_on_gpu_.get()), mailbox,
                             format, size, color_space, usage,
                             std::string(debug_label), surface_handle);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);

  return mailbox;
}

gpu::Mailbox SkiaOutputSurfaceImpl::CreateSolidColorSharedImage(
    const SkColor4f& color,
    const gfx::ColorSpace& color_space) {
  gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();

  auto task = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::CreateSolidColorSharedImage,
      base::Unretained(impl_on_gpu_.get()), mailbox, color, color_space);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);

  return mailbox;
}

void SkiaOutputSurfaceImpl::DestroySharedImage(const gpu::Mailbox& mailbox) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::DestroySharedImage,
                             base::Unretained(impl_on_gpu_.get()), mailbox);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
}

bool SkiaOutputSurfaceImpl::SupportsBGRA() const {
  if (graphite_recorder_) {
    // TODO(crbug.com/1451789): Implement properly for Graphite.
    return true;
  }

  return gr_context_thread_safe_
      ->defaultBackendFormat(SkColorType::kBGRA_8888_SkColorType,
                             GrRenderable::kYes)
      .isValid();
}

}  // namespace viz
