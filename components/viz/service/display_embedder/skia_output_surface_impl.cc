// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/render_pass_alpha_type.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/graphite_cache_controller.h"
#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
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
#include "skia/ext/skia_trace_memory_dump_impl.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
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
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/vk/VulkanTypes.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

namespace viz {

namespace {

// Records memory dumps and responds to memory pressure signals for Graphite Viz
// via a global object.
class GraphiteVizMemoryAssistant
    : public base::trace_event::MemoryDumpProvider {
 public:
  static GraphiteVizMemoryAssistant& GetInstance() {
    static base::NoDestructor<GraphiteVizMemoryAssistant> instance;
    return *instance;
  }

  void AddClient(skgpu::graphite::Recorder* recorder,
                 gpu::raster::GraphiteCacheController* cache_controller,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    if (num_clients_ == 0) {
      CHECK(!recorder_);
      CHECK(!cache_controller_);
      recorder_ = recorder;
      cache_controller_ = cache_controller;
      base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
          this, "GraphiteVizMemoryAssistant", std::move(task_runner));

      memory_pressure_listener_.emplace(
          FROM_HERE,
          base::BindRepeating(&GraphiteVizMemoryAssistant::HandleMemoryPressure,
                              base::Unretained(this)));
    }
    num_clients_++;
  }

  void RemoveClient() {
    num_clients_--;
    if (num_clients_ == 0) {
      memory_pressure_listener_.reset();
      recorder_ = nullptr;
      cache_controller_ = nullptr;
      base::trace_event::MemoryDumpManager::GetInstance()
          ->UnregisterDumpProvider(this);
    }
  }

 private:
  friend class base::NoDestructor<GraphiteVizMemoryAssistant>;

  GraphiteVizMemoryAssistant() = default;
  ~GraphiteVizMemoryAssistant() override = default;

  // MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override {
    CHECK(recorder_);
    bool background = args.level_of_detail ==
                      base::trace_event::MemoryDumpLevelOfDetail::kBackground;

    if (background) {
      std::string recorder_dump_name = base::StringPrintf(
          "skia/gpu_resources/viz_compositor_graphite_recorder_0x%" PRIXPTR,
          reinterpret_cast<uintptr_t>(recorder_.get()));
      base::trace_event::MemoryAllocatorDump* recorder_dump =
          pmd->CreateAllocatorDump(recorder_dump_name);
      recorder_dump->AddScalar(
          base::trace_event::MemoryAllocatorDump::kNameSize,
          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          recorder_->currentBudgetedBytes());

      // The ImageProvider's bytes are not included in the recorder's budgeted
      // bytes as they are owned by Chrome, so dump them separately.
      const auto* image_provider =
          static_cast<const gpu::GraphiteImageProvider*>(
              recorder_->clientImageProvider());
      std::string image_provider_dump_name = base::StringPrintf(
          "skia/gpu_resources/"
          "viz_compositor_graphite_image_provider_0x%" PRIXPTR,
          reinterpret_cast<uintptr_t>(image_provider));
      base::trace_event::MemoryAllocatorDump* image_provider_dump =
          pmd->CreateAllocatorDump(image_provider_dump_name);
      image_provider_dump->AddScalar(
          base::trace_event::MemoryAllocatorDump::kNameSize,
          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          image_provider->CurrentSizeInBytes());

    } else {
      skia::SkiaTraceMemoryDumpImpl trace_memory_dump(args.level_of_detail,
                                                      pmd);
      recorder_->dumpMemoryStatistics(&trace_memory_dump);
    }

    return true;
  }

  void HandleMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
    switch (memory_pressure_level) {
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
        return;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
        // With moderate pressure, clear any unlocked resources.
        cache_controller_->CleanUpScratchResources();
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
        cache_controller_->CleanUpAllResources();
        break;
    }
  }

  // NOTE: The implementation guarantees that the callback will always be called
  // on the thread that created the listener.
  std::optional<base::MemoryPressureListener> memory_pressure_listener_;

  raw_ptr<skgpu::graphite::Recorder> recorder_ = nullptr;
  raw_ptr<gpu::raster::GraphiteCacheController> cache_controller_ = nullptr;
  uint32_t num_clients_ = 0;
};

// FulfillForPlane is a struct that contains the ImageContext `context` used for
// fulfilling an GrPromiseImageTexture identified by `plane_index`. The
// plane_index is 0 for single planar formats and can be between [0, 3] for
// multiplanar formats.
struct FulfillForPlane {
  FulfillForPlane() = default;
  FulfillForPlane(const FulfillForPlane&) = default;
  FulfillForPlane& operator=(const FulfillForPlane&) = default;

  explicit FulfillForPlane(ImageContextImpl* context, int plane_index = 0)
      : context_(context), plane_index_(plane_index) {}

  raw_ptr<ImageContextImpl, AcrossTasksDanglingUntriaged> context_ = nullptr;
  int plane_index_ = 0;
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
  CHECK(fulfill);
  delete static_cast<FulfillForPlane*>(fulfill);
}

void CleanUpArray(void* fulfill_array) {
  CHECK(fulfill_array);
  base::HeapArray<FulfillForPlane>::DeleteLeakedData(fulfill_array);
}

gpu::ContextUrl& GetActiveUrl() {
  static base::NoDestructor<gpu::ContextUrl> active_url(
      GURL("chrome://gpu/SkiaRenderer"));
  return *active_url;
}

scoped_refptr<gpu::raster::GraphiteCacheController>
GetOrCreateGraphiteCacheController(skgpu::graphite::Recorder* recorder) {
  // All SkiaOutputSurfaceImpl instances on a thread share one cache controller,
  // and the controller will be released when all SkiaOutputSurfaceImpl
  // instances are released, so we use a sequence local WeakPtr here.
  static base::SequenceLocalStorageSlot<
      base::WeakPtr<gpu::raster::GraphiteCacheController>>
      sls_weak_controller;
  auto& weak_controller = sls_weak_controller.GetOrCreateValue();
  if (weak_controller) {
    return base::WrapRefCounted(weak_controller.get());
  }
  auto controller =
      base::MakeRefCounted<gpu::raster::GraphiteCacheController>(recorder);
  weak_controller = controller->AsWeakPtr();
  return controller;
}

}  // namespace

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    GrDeferredDisplayListRecorder* root_ddl_recorder,
    bool skip_draw_for_tests)
    : ddl_recorder_(root_ddl_recorder), canvas_(ddl_recorder_->getCanvas()) {
  Initialize(skip_draw_for_tests);
}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    const GrSurfaceCharacterization& characterization,
    const gpu::Mailbox& mailbox,
    bool skip_draw_for_tests)
    : mailbox_(mailbox) {
  ddl_recorder_storage_.emplace(characterization);
  ddl_recorder_ = &ddl_recorder_storage_.value();
  canvas_ = ddl_recorder_->getCanvas();
  Initialize(skip_draw_for_tests);
}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    skgpu::graphite::Recorder* recorder,
    const SkImageInfo& image_info,
    skgpu::graphite::TextureInfo texture_info,
    const gpu::Mailbox& mailbox,
    bool skip_draw_for_tests)
    : graphite_recorder_(recorder), mailbox_(mailbox) {
  CHECK(graphite_recorder_);
  canvas_ = graphite_recorder_->makeDeferredCanvas(image_info, texture_info);
  Initialize(skip_draw_for_tests);
}

SkiaOutputSurfaceImpl::ScopedPaint::~ScopedPaint() {
  CHECK(!canvas_);
}

void SkiaOutputSurfaceImpl::ScopedPaint::Initialize(bool skip_draw_for_tests) {
  if (canvas_ && skip_draw_for_tests) {
    auto image_info = canvas_->imageInfo();
    no_draw_canvas_ = std::make_unique<SkNoDrawCanvas>(image_info.width(),
                                                       image_info.height());
    canvas_ = no_draw_canvas_.get();
  }
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
    : dependency_(display_controller->skia_dependency()),
      renderer_settings_(renderer_settings),
      debug_settings_(debug_settings),
      display_compositor_controller_(display_controller),
      gpu_task_scheduler_(display_compositor_controller_->gpu_task_scheduler()),
      is_using_raw_draw_(features::IsUsingRawDraw()),
      is_raw_draw_using_msaa_(features::IsRawDrawUsingMSAA()),
      skip_draw_for_tests_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGLDrawingForTests)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_using_raw_draw_) {
    auto* manager = dependency_->GetSharedImageManager();
    DCHECK(manager->is_thread_safe());
    representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(manager,
                                                                nullptr);
  }
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("viz", __PRETTY_FUNCTION__);
  current_paint_.reset();
  root_ddl_recorder_.reset();

  if (graphite_recorder_) {
    GraphiteVizMemoryAssistant::GetInstance().RemoveClient();
  }

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SkiaOutputSurfaceImpl::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
          size_, color_type_, alpha_type_, skgpu::Mipmapped::kNo,
          sk_color_space_);
  CHECK(characterization.isValid());
  root_ddl_recorder_.emplace(characterization);
  // This will trigger the lazy initialization of the recorder
  std::ignore = root_ddl_recorder_->getCanvas();
  reset_ddl_recorder_on_swap_ = false;
}

void SkiaOutputSurfaceImpl::Reshape(const ReshapeParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!params.size.IsEmpty());

  size_ = params.size;
  format_ = params.format;
  alpha_type_ = static_cast<SkAlphaType>(params.alpha_type);

  auto it = capabilities_.sk_color_type_map.find(params.format);
  if (it != capabilities_.sk_color_type_map.end()) {
    color_type_ = it->second;
  }
  CHECK_NE(color_type_, kUnknown_SkColorType)
      << "SkColorType is invalid for format: " << params.format.ToString();

  sk_color_space_ = params.color_space.ToSkColorSpace();

  if (capabilities_.damage_area_from_skia_output_device) {
    damage_of_current_buffer_ = gfx::Rect(size_);
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

  const SkiaOutputDevice::ReshapeParams device_reshape_params = {
      .image_info =
          SkImageInfo::Make(size_.width(), size_.height(), color_type_,
                            alpha_type_, sk_color_space_),
      .color_space = params.color_space,
      .sample_count = sample_count_,
      .device_scale_factor = params.device_scale_factor,
      .transform = GetDisplayTransform(),
  };
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.

  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::Reshape,
                             base::Unretained(impl_on_gpu_.get()),
                             device_reshape_params);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());
  FlushGpuTasks(SyncMode::kNoWait);

  RecreateRootDDLRecorder();
}

void SkiaOutputSurfaceImpl::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  CHECK(!current_paint_);
  CHECK(root_ddl_recorder_ || graphite_recorder_);
  if (graphite_recorder_) {
    SkImageInfo image_info = SkImageInfo::Make(
        gfx::SizeToSkISize(size_), color_type_, alpha_type_, sk_color_space_);
    // Surfaceless output devices allocate shared image behind the scenes. On
    // the GPU thread this is treated same as regular IOSurfaces for the
    // purpose of creating Graphite TextureInfo i.e. it will have CopySrc and
    // CopyDst usage. So don't treat it like a root surface which generally
    // won't have or support those usages.
    skgpu::graphite::TextureInfo texture_info = gpu::GraphiteBackendTextureInfo(
        gr_context_type_, format_, /*readonly=*/false, /*plane_index=*/0,
        /*is_yuv_plane=*/false,
        /*mipmapped=*/false, /*scanout_dcomp_surface=*/false,
        /*supports_multiplanar_rendering=*/false,
        /*supports_multiplanar_copy=*/false);
    CHECK(texture_info.isValid());
    current_paint_.emplace(graphite_recorder_, image_info, texture_info,
                           gpu::Mailbox(), skip_draw_for_tests_);
  } else {
    reset_ddl_recorder_on_swap_ = true;
    current_paint_.emplace(&root_ddl_recorder_.value(), skip_draw_for_tests_);
  }
  return current_paint_->canvas();
}

void SkiaOutputSurfaceImpl::MakePromiseSkImage(
    ImageContext* image_context,
    const gfx::ColorSpace& color_space,
    bool force_rgbx) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_paint_);
  DCHECK(!image_context->mailbox_holder().mailbox.IsZero());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
               "SkiaOutputSurfaceImpl::MakePromiseSkImage");

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
    MakePromiseSkImageSinglePlane(image_context_impl, /*mipmapped=*/false,
                                  color_space, force_rgbx);
  } else {
    DCHECK(!force_rgbx);
    MakePromiseSkImageMultiPlane(image_context_impl, color_space);
  }

  if (mailbox_holder.sync_token.HasData()) {
    resource_sync_tokens_.push_back(mailbox_holder.sync_token);
    image_context->mutable_mailbox_holder()->sync_token.Clear();
  }
}

void SkiaOutputSurfaceImpl::MakePromiseSkImageSinglePlane(
    ImageContextImpl* image_context,
    bool mipmap,
    const gfx::ColorSpace& color_space,
    bool force_rgbx) {
  CHECK(!image_context->has_image());
  auto format = image_context->format();
  CHECK(format.is_single_plane() || format.PrefersExternalSampler());
  FulfillForPlane* fulfill = new FulfillForPlane(image_context);
  SkColorType color_type =
      format.PrefersExternalSampler()
          ? gpu::ToClosestSkColorTypeExternalSampler(format)
          : ToClosestSkColorType(/*gpu_compositing=*/true, format);

  if (force_rgbx) {
    if (color_type == SkColorType::kBGRA_8888_SkColorType ||
        color_type == SkColorType::kRGBA_8888_SkColorType) {
      // We do not have a BGRX type in skia. RGBX will suffice for this case as
      // BGRA8 on 'kRGB_888x_SkColorType' is designed not to swizzle.
      color_type = SkColorType::kRGB_888x_SkColorType;
    }
  }

  if (graphite_recorder_) {
    skgpu::graphite::TextureInfo texture_info = gpu::GraphitePromiseTextureInfo(
        gr_context_type_, format, image_context->ycbcr_info(),
        /*plane_index=*/0, mipmap);
    SkColorInfo color_info(color_type, image_context->alpha_type(),
                           image_context->color_space());
    skgpu::Origin origin = image_context->origin() == kTopLeft_GrSurfaceOrigin
                               ? skgpu::Origin::kTopLeft
                               : skgpu::Origin::kBottomLeft;
    auto image = SkImages::PromiseTextureFrom(
        graphite_recorder_, gfx::SizeToSkISize(image_context->size()),
        texture_info, color_info, origin, graphite_use_volatile_promise_images_,
        FulfillGraphite, CleanUp, ReleaseGraphite, fulfill);
    LOG_IF(ERROR, !image) << "Failed to create the promise sk image";
    image_context->SetImage(std::move(image), {texture_info});
  } else {
    CHECK(gr_context_thread_safe_);
    GrBackendFormat backend_format = GetGrBackendFormatForTexture(
        format, /*plane_index=*/0,
        image_context->mailbox_holder().texture_target,
        image_context->ycbcr_info(), color_space);
    auto image = SkImages::PromiseTextureFrom(
        gr_context_thread_safe_, backend_format,
        gfx::SizeToSkISize(image_context->size()),
        mipmap ? skgpu::Mipmapped::kYes : skgpu::Mipmapped::kNo,
        image_context->origin(), color_type, image_context->alpha_type(),
        image_context->color_space(), FulfillGanesh, CleanUp, fulfill);
    LOG_IF(ERROR, !image) << "Failed to create the promise sk image";
    image_context->SetImage(std::move(image), {backend_format});
  }
}

void SkiaOutputSurfaceImpl::MakePromiseSkImageMultiPlane(
    ImageContextImpl* image_context,
    const gfx::ColorSpace& color_space) {
  CHECK(!image_context->has_image());
  auto format = image_context->format();
  CHECK(format.is_multi_plane());
  SkYUVAInfo::PlaneConfig plane_config = gpu::ToSkYUVAPlaneConfig(format);
  SkYUVAInfo::Subsampling subsampling = gpu::ToSkYUVASubsampling(format);
  // TODO(crbug.com/368870063): Implement RGB matrix support in
  // ToSkYUVColorSpace.
  SkYUVColorSpace sk_yuv_color_space = kIdentity_SkYUVColorSpace;
  if (color_space.GetMatrixID() != gfx::ColorSpace::MatrixID::RGB) {
    // TODO(crbug.com/41380578): This should really default to rec709.
    sk_yuv_color_space = kRec601_SkYUVColorSpace;
    color_space.ToSkYUVColorSpace(format.MultiplanarBitDepth(),
                                  &sk_yuv_color_space);
  }
  SkYUVAInfo yuva_info(gfx::SizeToSkISize(image_context->size()), plane_config,
                       subsampling, sk_yuv_color_space);
  if (graphite_recorder_) {
    // This function is for per-plane sampling and is never used in conjunction
    // with YCbCr sampling. In particular, on Android SharedImages used with
    // YCbCr sampling always have RGBA format and on ChromeOS such SharedImages
    // will have PrefersExternalSampler() set to true. Both of these cases are
    // handled by MakePromiseSkImageSinglePlane().
    // TODO(blundell): Hoist this CHECK up to apply universally for Ganesh as
    // well as Graphite.
    CHECK(!image_context->ycbcr_info());

    auto fulfill_array =
        base::HeapArray<FulfillForPlane>::WithSize(SkYUVAInfo::kMaxPlanes);
    void* fulfill_ptrs[SkYUVAInfo::kMaxPlanes] = {};
    std::vector<skgpu::graphite::TextureInfo> texture_infos;
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      CHECK_EQ(image_context->origin(), kTopLeft_GrSurfaceOrigin);
      fulfill_array[plane_index] = FulfillForPlane(image_context, plane_index);
      fulfill_ptrs[plane_index] = &fulfill_array[plane_index];
      texture_infos.emplace_back(gpu::GraphitePromiseTextureInfo(
          gr_context_type_, format, /*ycbcr_info=*/std::nullopt, plane_index));
    }

    skgpu::graphite::YUVABackendTextureInfo yuva_backend_info(
        graphite_recorder_, yuva_info, texture_infos, skgpu::Mipmapped::kNo);
    void* fulfill_array_ptr = std::move(fulfill_array).leak().data();
    auto image = SkImages::PromiseTextureFromYUVA(
        graphite_recorder_, yuva_backend_info, image_context->color_space(),
        graphite_use_volatile_promise_images_, FulfillGraphite, CleanUpArray,
        ReleaseGraphite, fulfill_array_ptr, fulfill_ptrs);
    LOG_IF(ERROR, !image) << "Failed to create the yuv promise sk image";
    image_context->SetImage(std::move(image), std::move(texture_infos));
  } else {
    CHECK(gr_context_thread_safe_);
    std::vector<GrBackendFormat> formats;
    void* fulfills[SkYUVAInfo::kMaxPlanes] = {};
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         ++plane_index) {
      CHECK_EQ(image_context->origin(), kTopLeft_GrSurfaceOrigin);
      // NOTE: To compute the format, it is necessary to pass the ColorSpace
      // that came originally from the TransferableResource.
      formats.push_back(GetGrBackendFormatForTexture(
          format, plane_index, image_context->mailbox_holder().texture_target,
          image_context->ycbcr_info(), color_space));
      fulfills[plane_index] = new FulfillForPlane(image_context, plane_index);
    }

    GrYUVABackendTextureInfo yuva_backend_info(yuva_info, formats.data(),
                                               skgpu::Mipmapped::kNo,
                                               kTopLeft_GrSurfaceOrigin);
    auto image = SkImages::PromiseTextureFromYUVA(
        gr_context_thread_safe_, yuva_backend_info,
        image_context->color_space(), FulfillGanesh, CleanUp, fulfills);
    LOG_IF(ERROR, !image) << "Failed to create the yuv promise sk image";
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
    const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool raw_draw_if_possible) {
  return std::make_unique<ImageContextImpl>(
      holder, size, format, maybe_concurrent_reads, ycbcr_info,
      std::move(color_space),
      /*is_for_render_pass=*/false, raw_draw_if_possible);
}

DBG_FLAG_FBOOL("skia_gpu.swap_buffers.force_disable_makecurrent",
               force_disable_makecurrent)

void SkiaOutputSurfaceImpl::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!current_paint_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffers,
                     base::Unretained(impl_on_gpu_.get()), std::move(frame));

  // Normally MakeCurrent isn't needed for SwapBuffers, but it used to be called
  // unconditionally, both for historical reasons and edge cases too.
  // Now, we call MakeCurrent here only appropriated, and delay it in some other
  // circumstances.
  bool make_current =
      capabilities_.present_requires_make_current &&
      !force_disable_makecurrent();  // Defaults to false.

  EnqueueGpuTask(std::move(callback), std::move(resource_sync_tokens_),
                 make_current,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());

  // Recreate |root_ddl_recorder_| after SwapBuffers has been scheduled on GPU
  // thread to save some time in BeginPaintCurrentFrame
  // Recreating recorder is expensive. Avoid recreation if there was no paint.
  if (reset_ddl_recorder_on_swap_) {
    RecreateRootDDLRecorder();
  }

  if (graphite_cache_controller_) {
    graphite_cache_controller_->ScheduleCleanup();
  }
}

void SkiaOutputSurfaceImpl::SwapBuffersSkipped(
    const gfx::Rect root_pass_damage_rect) {
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

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& surface_size,
    SharedImageFormat format,
    RenderPassAlphaType alpha_type,
    skgpu::Mipmapped mipmap,
    bool scanout_dcomp_surface,
    sk_sp<SkColorSpace> color_space,
    bool is_overlay,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  CHECK(!current_paint_);
  CHECK(resource_sync_tokens_.empty());

  SkColorType color_type =
      ToClosestSkColorType(/*gpu_compositing=*/true, format);
  if (graphite_recorder_) {
    SkImageInfo image_info =
        SkImageInfo::Make(gfx::SizeToSkISize(surface_size), color_type,
                          static_cast<SkAlphaType>(alpha_type), color_space);
    skgpu::graphite::TextureInfo texture_info = gpu::GraphiteBackendTextureInfo(
        gr_context_type_, format, /*readonly=*/false, /*plane_index=*/0,
        /*is_yuv_plane=*/false, mipmap == skgpu::Mipmapped::kYes,
        scanout_dcomp_surface, /*supports_multiplanar_rendering=*/false,
        /*supports_multiplanar_copy=*/false);
    if (!texture_info.isValid()) {
      DLOG(ERROR) << "BeginPaintRenderPass: invalid Graphite TextureInfo";
      return nullptr;
    }
    current_paint_.emplace(graphite_recorder_, image_info, texture_info,
                           mailbox, skip_draw_for_tests_);
  } else {
    GrSurfaceCharacterization characterization =
        CreateGrSurfaceCharacterizationRenderPass(
            surface_size, color_type, static_cast<SkAlphaType>(alpha_type),
            mipmap, std::move(color_space), is_overlay, scanout_dcomp_surface);
    if (!characterization.isValid()) {
      DLOG(ERROR) << "BeginPaintRenderPass: invalid GrSurfaceCharacterization";
      return nullptr;
    }
    current_paint_.emplace(characterization, mailbox, skip_draw_for_tests_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
          size_, color_type_with_alpha, alpha_type_, skgpu::Mipmapped::kNo,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    auto task = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame,
        base::Unretained(impl_on_gpu_.get()), std::move(ddl),
        std::move(overdraw_ddl), std::move(graphite_recording),
        std::move(images_in_current_paint_), resource_sync_tokens_,
        std::move(on_finished), std::move(return_release_fence_cb));
    EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                   /*make_current=*/true, /*need_framebuffer=*/true);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_paint_);

  auto& image_context = render_pass_image_cache_[id];
  if (!image_context) {
    gpu::MailboxHolder mailbox_holder(mailbox, gpu::SyncToken(), GL_TEXTURE_2D);
    image_context = std::make_unique<ImageContextImpl>(
        mailbox_holder, size, format, /*maybe_concurrent_reads=*/false,
        /*ycbcr_info=*/std::nullopt, std::move(color_space),
        /*is_for_render_pass=*/true);
  }
  if (!image_context->has_image()) {
    // NOTE: The ColorSpace parameter is relevant only for external sampling,
    // whereas RenderPasses always work with true single-planar formats.
    MakePromiseSkImageSinglePlane(image_context.get(), mipmap,
                                  gfx::ColorSpace(), false);
    if (!image_context->has_image()) {
      return nullptr;
    }
  }
  images_in_current_paint_.push_back(image_context.get());
  return image_context->image();
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<AggregatedRenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request->has_blit_request()) {
    const auto& sync_token = request->blit_request().sync_token();
    if (sync_token.HasData()) {
      resource_sync_tokens_.push_back(sync_token);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(impl_on_gpu_);
  capabilities_.output_surface_origin = output_surface_origin;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting,
                     base::Unretained(impl_on_gpu_.get()), capabilities_);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
}

bool SkiaOutputSurfaceImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("viz", __PRETTY_FUNCTION__);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();

  bool result = false;
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                                 base::Unretained(this), &result);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  // |capabilities_| will be initialized in InitializeOnGpuThread(), so have to
  // wait.
  FlushGpuTasks(SyncMode::kWaitForTasksFinished);

  if (capabilities_.damage_area_from_skia_output_device) {
    damage_of_current_buffer_.emplace();
  }

  // |graphite_recorder_| is used on viz thread, so we get or create cache
  // controller for graphite_recorder_ and use it on viz thread.
  if (graphite_recorder_) {
    graphite_cache_controller_ =
        GetOrCreateGraphiteCacheController(graphite_recorder_);
    GraphiteVizMemoryAssistant::GetInstance().AddClient(
        graphite_recorder_, graphite_cache_controller_.get(),
        dependency_->GetClientTaskRunner());
  }
  return result;
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(bool* result) {
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
  auto release_overlays_callback =
      base::BindRepeating(&SkiaOutputSurfaceImpl::ReleaseOverlays, weak_ptr_);

  impl_on_gpu_ = SkiaOutputSurfaceImplOnGpu::Create(
      dependency_, renderer_settings_, gpu_task_scheduler_->GetSequenceId(),
      display_compositor_controller_->controller_on_gpu(),
      std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(schedule_gpu_task),
      std::move(add_child_window_to_browser_callback),
      std::move(release_overlays_callback));
  if (!impl_on_gpu_) {
    *result = false;
    return;
  }
  capabilities_ = impl_on_gpu_->capabilities();

  auto shared_context_state = dependency_->GetSharedContextState();
  gr_context_type_ = shared_context_state->gr_context_type();
  if (auto* gr_context = shared_context_state->gr_context()) {
    gr_context_thread_safe_ = gr_context->threadSafeProxy();
  }
  graphite_recorder_ = shared_context_state->viz_compositor_graphite_recorder();
  // On Dawn/Metal & Dawn/D3D, it is possible to use non-volatile promise images
  // as Dawn textures are cached between BeginAccess() calls on a per-usage
  // basis. Other platforms/backends cannot use non-volatile promise images as
  // Dawn textures live only for the duration of a scoped access.
  const bool can_use_non_volatile_images =
      shared_context_state->IsGraphiteDawnMetal() ||
      shared_context_state->IsGraphiteDawnD3D();
  graphite_use_volatile_promise_images_ = can_use_non_volatile_images
                                              ? skgpu::graphite::Volatile::kNo
                                              : skgpu::graphite::Volatile::kYes;
  *result = true;
}

GrSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateGrSurfaceCharacterizationRenderPass(
    const gfx::Size& surface_size,
    SkColorType color_type,
    SkAlphaType alpha_type,
    skgpu::Mipmapped mipmap,
    sk_sp<SkColorSpace> color_space,
    bool is_overlay,
    bool scanout_dcomp_surface) const {
  if (!gr_context_thread_safe_) {
    DLOG(ERROR) << "gr_context_thread_safe_ is null.";
    return GrSurfaceCharacterization();
  }

  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  SkSurfaceProps surface_props;
  const int sample_count = std::min(
      sample_count_,
      gr_context_thread_safe_->maxSurfaceSampleCountForColorType(color_type));
  auto backend_format = gr_context_thread_safe_->defaultBackendFormat(
      color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid());
#if BUILDFLAG(IS_APPLE)
  if (is_overlay) {
    DCHECK_EQ(gr_context_type_, gpu::GrContextType::kGL);
    // For overlay, IOSurface will be used. Hence, we need to ensure that we are
    // using the correct texture target for IOSurfaces, which depends on the GL
    // implementation.
    backend_format = GrBackendFormats::MakeGL(
        GrBackendFormats::AsGLFormatEnum(backend_format),
#if BUILDFLAG(IS_MAC)
        gpu::GetTextureTargetForIOSurfaces());
#else
        GL_TEXTURE_2D);
#endif
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
      /*isTextureable=*/!scanout_dcomp_surface, skgpu::Protected::kNo);
  DCHECK(characterization.isValid());
  return characterization;
}

GrSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateGrSurfaceCharacterizationCurrentFrame(
    const gfx::Size& surface_size,
    SkColorType color_type,
    SkAlphaType alpha_type,
    skgpu::Mipmapped mipmap,
    sk_sp<SkColorSpace> color_space) const {
  if (!gr_context_thread_safe_) {
    DLOG(ERROR) << "gr_context_thread_safe_ is null.";
    return GrSurfaceCharacterization();
  }

  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  SkSurfaceProps surface_props;
  int sample_count = std::min(
      sample_count_,
      gr_context_thread_safe_->maxSurfaceSampleCountForColorType(color_type));
  auto backend_format = gr_context_thread_safe_->defaultBackendFormat(
      color_type, GrRenderable::kYes);
#if BUILDFLAG(IS_MAC)
  DCHECK_EQ(gr_context_type_, gpu::GrContextType::kGL);
  // For root render pass, IOSurface will be used. Hence, we need to ensure that
  // we are using the correct texture target for IOSurfaces, which depends on
  // the GL implementation.
  backend_format =
      GrBackendFormats::MakeGL(GrBackendFormats::AsGLFormatEnum(backend_format),
                               gpu::GetTextureTargetForIOSurfaces());
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
          gr_context_type_ == gpu::GrContextType::kGL) ||
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
      skgpu::Protected::kNo, /*vkRTSupportsInputAttachment=*/false,
      capabilities_.root_is_vulkan_secondary_command_buffer);
#if BUILDFLAG(ENABLE_VULKAN)
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
#endif
  LOG_IF(FATAL, !characterization.isValid())
      << "\n  surface_size=" << surface_size.ToString()
      << "\n  format=" << static_cast<int>(color_type)
      << "\n  color_type=" << static_cast<int>(color_type)
      << "\n  backend_format.isValid()=" << backend_format.isValid()
      << "\n  backend_format.backend()="
      << static_cast<int>(backend_format.backend())
      << "\n  GrBackendFormats::AsGLFormat(backend_format)="
      << static_cast<int>(GrBackendFormats::AsGLFormat(backend_format))
#if BUILDFLAG(ENABLE_VULKAN)
      << "\n  backend_format.asVkFormat()="
      << static_cast<int>(
             GrBackendFormats::AsVkFormat(backend_format, &vk_format))
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_);

  if (capabilities_.damage_area_from_skia_output_device) {
    DCHECK(params.frame_buffer_damage_area);
    damage_of_current_buffer_ = params.frame_buffer_damage_area;
  }

  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  client_->DidReceiveSwapBuffersAck(params, std::move(release_fence));
  if (!params.released_overlays.empty())
    client_->DidReceiveReleasedOverlays(params.released_overlays);
  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void SkiaOutputSurfaceImpl::ReleaseOverlays(
    std::vector<gpu::Mailbox> released_overlays) {
  if (!released_overlays.empty()) {
    client_->DidReceiveReleasedOverlays(released_overlays);
  }
}

void SkiaOutputSurfaceImpl::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_);
  client_->AddChildWindowToBrowser(child_window);
}

void SkiaOutputSurfaceImpl::ScheduleGpuTaskForTesting(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  EnqueueGpuTask(std::move(callback), std::move(sync_tokens),
                 /*make_current=*/false, /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
}

void SkiaOutputSurfaceImpl::CheckAsyncWorkCompletionForTesting() {
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CheckAsyncWorkCompletion,
                     base::Unretained(impl_on_gpu_.get()));
  EnqueueGpuTask(std::move(task), std::vector<gpu::SyncToken>(),
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
        gl::ProgressReporter* progress_reporter = nullptr;
        if (impl_on_gpu) {
          progress_reporter = impl_on_gpu->context_state()->progress_reporter();
        }
        for (auto& task : tasks) {
          gl::ScopedProgressReporter scoped_process_reporter(
              progress_reporter);
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

  if (event) {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    event->Wait();
  }
}

GrBackendFormat SkiaOutputSurfaceImpl::GetGrBackendFormatForTexture(
    SharedImageFormat si_format,
    int plane_index,
    uint32_t gl_texture_target,
    const std::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    const gfx::ColorSpace& yuv_color_space) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (gr_context_type_ == gpu::GrContextType::kVulkan) {
    if (!si_format.PrefersExternalSampler() && !ycbcr_info) {
      // For per-plane sampling, can just return the VkFormat for the plane if
      // VulkanYcbCrInfo isn't present.
      return GrBackendFormats::MakeVk(gpu::ToVkFormat(si_format, plane_index));
    }

    // Note: The utility function for computing the VkFormat differs based on
    // whether the SIF is multiplanar with external sampling or legacy
    // multiplanar.
    auto vk_format = si_format.PrefersExternalSampler()
                         ? gpu::ToVkFormatExternalSampler(si_format)
                         : gpu::ToVkFormatSinglePlanar(si_format);
    // Assume optimal tiling.
    skgpu::VulkanYcbcrConversionInfo gr_ycbcr_info =
        CreateVulkanYcbcrConversionInfo(dependency_->GetVulkanContextProvider()
                                            ->GetDeviceQueue()
                                            ->GetVulkanPhysicalDevice(),
                                        VK_IMAGE_TILING_OPTIMAL, vk_format,
                                        si_format, yuv_color_space, ycbcr_info);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // Textures that were allocated _on linux_ with ycbcr info came from
    // VaapiVideoDecoder, which exports using DRM format modifiers.
    return GrBackendFormats::MakeVk(gr_ycbcr_info,
                                    /*willUseDRMFormatModifiers=*/true);
#else
    return GrBackendFormats::MakeVk(gr_ycbcr_info);
#endif  // BUILDFLAG(IS_LINUX)
  } else {
#else
  {
#endif  // BUILDFLAG(ENABLE_VULKAN)
    CHECK_EQ(gr_context_type_, gpu::GrContextType::kGL);
    // Convert internal format from GLES2 to platform GL.
    gpu::GLFormatCaps caps(impl_on_gpu_->GetFeatureInfo());
    auto gl_format_desc = si_format.PrefersExternalSampler()
                              ? caps.ToGLFormatDescExternalSampler(si_format)
                              : caps.ToGLFormatDesc(si_format, plane_index);
    auto gl_storage_internal_format = gl_format_desc.storage_internal_format;
    unsigned int texture_storage_format = gpu::GetGrGLBackendTextureFormat(
        impl_on_gpu_->GetFeatureInfo(), gl_storage_internal_format,
        gr_context_thread_safe_);

    return GrBackendFormats::MakeGL(texture_storage_format, gl_texture_target);
  }
}

void SkiaOutputSurfaceImpl::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}

#if BUILDFLAG(IS_ANDROID)
base::ScopedClosureRunner SkiaOutputSurfaceImpl::GetCacheBackBufferCb() {
  // Note, that we call it directly on viz thread to get the callback.
  return impl_on_gpu_->GetCacheBackBufferCb();
}
#endif

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

void SkiaOutputSurfaceImpl::ContextLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  if (capabilities_.damage_area_from_skia_output_device) {
    return *damage_of_current_buffer_;
  }

  return gfx::Rect();
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
    RenderPassAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string_view debug_label,
    gpu::SurfaceHandle surface_handle) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CreateSharedImage,
                     base::Unretained(impl_on_gpu_.get()), mailbox, format,
                     size, color_space, static_cast<SkAlphaType>(alpha_type),
                     usage, std::string(debug_label), surface_handle);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);

  return mailbox;
}

gpu::Mailbox SkiaOutputSurfaceImpl::CreateSolidColorSharedImage(
    const SkColor4f& color,
    const gfx::ColorSpace& color_space) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();

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

void SkiaOutputSurfaceImpl::SetSharedImagePurgeable(const gpu::Mailbox& mailbox,
                                                    bool purgeable) {
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetSharedImagePurgeable,
                     base::Unretained(impl_on_gpu_.get()), mailbox, purgeable);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

bool SkiaOutputSurfaceImpl::SupportsBGRA() const {
  if (graphite_recorder_) {
    // TODO(crbug.com/40270686): Implement properly for Graphite.
#if BUILDFLAG(IS_IOS)
    return false;
#else
    return true;
#endif  // BUILDFLAG(IS_IOS)
  }

  return gr_context_thread_safe_
      ->defaultBackendFormat(SkColorType::kBGRA_8888_SkColorType,
                             GrRenderable::kYes)
      .isValid();
}

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
void SkiaOutputSurfaceImpl::DetileOverlay(gpu::Mailbox input,
                                          const gfx::Size& input_visible_size,
                                          gpu::SyncToken input_sync_token,
                                          gpu::Mailbox output,
                                          const gfx::RectF& display_rect,
                                          const gfx::RectF& crop_rect,
                                          gfx::OverlayTransform transform,
                                          bool is_10bit) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::DetileOverlay,
                             base::Unretained(impl_on_gpu_.get()), input,
                             input_visible_size, output, display_rect,
                             crop_rect, transform, is_10bit);
  EnqueueGpuTask(std::move(task), {input_sync_token}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::CleanupImageProcessor() {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CleanupImageProcessor,
                             base::Unretained(impl_on_gpu_.get()));
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}
#endif

void SkiaOutputSurfaceImpl::ReadbackForTesting(
    CopyOutputRequest::CopyOutputRequestCallback result_callback) {
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ReadbackForTesting,
      base::Unretained(impl_on_gpu_.get()), std::move(result_callback));
  EnqueueGpuTask(std::move(callback), std::move(resource_sync_tokens_),
                 /*make_current=*/true,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());
  FlushGpuTasks(SyncMode::kNoWait);
}

}  // namespace viz
