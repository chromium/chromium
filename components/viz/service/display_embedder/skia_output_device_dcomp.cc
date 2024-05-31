// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/debug/alias.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

namespace viz {

namespace {
base::TimeTicks g_last_reshape_failure = base::TimeTicks();

NOINLINE void CheckForLoopFailures() {
  const auto threshold = base::Seconds(1);
  auto now = base::TimeTicks::Now();
  if (!g_last_reshape_failure.is_null() &&
      now - g_last_reshape_failure < threshold) {
    CHECK(false);
  }
  g_last_reshape_failure = now;
}

class SharedImageMailboxWithFactoryPtr {
 public:
  struct ScopedTraits {
    static SharedImageMailboxWithFactoryPtr InvalidValue() { return {}; }

    static void Free(SharedImageMailboxWithFactoryPtr scoped_shared_image) {
      CHECK(!scoped_shared_image.mailbox_.IsZero());
      CHECK(scoped_shared_image.factory_);
      CHECK(scoped_shared_image.factory_->DestroySharedImage(
          scoped_shared_image.mailbox_));
    }
  };

  SharedImageMailboxWithFactoryPtr() = default;
  SharedImageMailboxWithFactoryPtr(const gpu::Mailbox& mailbox,
                                   gpu::SharedImageFactory& factory)
      : mailbox_(mailbox), factory_(&factory) {
    CHECK(!mailbox.IsZero());
  }

  bool operator==(const SharedImageMailboxWithFactoryPtr& other) const {
    return std::tie(mailbox_, factory_) ==
           std::tie(other.mailbox_, other.factory_);
  }

  const gpu::Mailbox& mailbox() const { return mailbox_; }

 private:
  gpu::Mailbox mailbox_;
  raw_ptr<gpu::SharedImageFactory> factory_ = nullptr;
};

using ScopedSharedImageMailbox =
    base::ScopedGeneric<SharedImageMailboxWithFactoryPtr,
                        SharedImageMailboxWithFactoryPtr::ScopedTraits>;

bool CopySharedImage(gpu::SharedContextState* context_state,
                     gpu::SkiaImageRepresentation* src_representation,
                     gpu::SkiaImageRepresentation* dst_representation) {
  CHECK_EQ(src_representation->size(), dst_representation->size());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<gpu::SkiaImageRepresentation::ScopedReadAccess>
      src_read_access = src_representation->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);
  // We expect the source image to be initialized with no outstanding writes.
  CHECK(src_read_access);
  CHECK(begin_semaphores.empty());
  CHECK(end_semaphores.empty());

  std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>
      dst_write_access = dst_representation->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          gpu::SkiaImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dst_write_access) {
    LOG(ERROR) << "Failed to create write access for dst";
    return false;
  }

  sk_sp<SkImage> src_image = src_read_access->CreateSkImage(context_state);
  if (!src_image) {
    LOG(ERROR) << "Failed to create SkImage for src";
    return false;
  }

  const bool flip_y =
      src_representation->surface_origin() != kTopLeft_GrSurfaceOrigin;
  if (flip_y) {
    dst_write_access->surface()->getCanvas()->scale(1, -1);
    dst_write_access->surface()->getCanvas()->translate(
        0, -src_representation->size().height());
  }

  SkPaint non_blending_blit;
  non_blending_blit.setBlendMode(SkBlendMode::kSrc);
  SkRect bounds = gfx::RectToSkRect(gfx::Rect(src_representation->size()));
  dst_write_access->surface()->getCanvas()->drawImageRect(
      src_image.get(), /*src=*/bounds, /*dst=*/bounds, SkSamplingOptions(),
      &non_blending_blit, SkCanvas::kFast_SrcRectConstraint);
  dst_representation->SetCleared();

  CHECK(!src_read_access->HasBackendSurfaceEndState());

  context_state->FlushWriteAccess(dst_write_access.get());
  context_state->SubmitIfNecessary(std::move(end_semaphores));

  dst_write_access->ApplyBackendSurfaceEndState();

  return true;
}

ScopedSharedImageMailbox CopyQuadResource(
    gpu::SharedContextState* context_state,
    gpu::raster::GrShaderCache* gr_shader_cache,
    gpu::SharedImageFactory& shared_image_factory,
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::Mailbox src_mailbox) {
  std::unique_ptr<gpu::SkiaImageRepresentation> src_representation =
      shared_image_representation_factory->ProduceSkia(
          src_mailbox, scoped_refptr<gpu::SharedContextState>(context_state));
  if (!src_representation) {
    LOG(ERROR) << "Failed to create Skia representation for quad resource src";
    return {};
  }

  // TODO(crbug.com/41497086): We don't expect any HDR content with delegated
  // compositing due to OverlayProcessorWin bailing when it sees the color
  // conversion pass.
  CHECK(!src_representation->color_space().IsHDR());

  const gpu::Mailbox overlay_dst = gpu::Mailbox::Generate();
  const SharedImageFormat dst_format = SinglePlaneFormat::kBGRA_8888;
  const gfx::ColorSpace dst_color_space = gfx::ColorSpace::CreateSRGB();
  const bool success = shared_image_factory.CreateSharedImage(
      overlay_dst, dst_format, src_representation->size(), dst_color_space,
      kTopLeft_GrSurfaceOrigin, src_representation->alpha_type(), nullptr,
      gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE | gpu::SHARED_IMAGE_USAGE_SCANOUT |
          gpu::SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE,
      "OutputDeviceDComp_QuadResourceCopy");
  if (!success) {
    LOG(ERROR) << "Failed to create shared image for quad resource copy dst";
    return {};
  }
  ScopedSharedImageMailbox overlay_image({overlay_dst, shared_image_factory});

  std::unique_ptr<gpu::SkiaImageRepresentation> dst_representation =
      shared_image_representation_factory->ProduceSkia(
          overlay_image.get().mailbox(),
          scoped_refptr<gpu::SharedContextState>(context_state));
  // We don't expect |DCompSurfaceImageBacking::ProduceSkia| to fail.
  CHECK(dst_representation);

  std::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
  if (gr_shader_cache) {
    cache_use.emplace(gr_shader_cache, gpu::kDisplayCompositorClientId);
  }
  if (!CopySharedImage(context_state, src_representation.get(),
                       dst_representation.get())) {
    LOG(ERROR) << "Failed to copy quad resource src to dst";
    return {};
  }

  return overlay_image;
}

}  // namespace

// Holds reference needed to keep overlay textures alive.
// TODO(kylechar): We can probably merge OverlayData in with
// SkiaOutputSurfaceImplOnGpu overlay data.
class SkiaOutputDeviceDComp::OverlayData {
 public:
  explicit OverlayData(
      std::unique_ptr<gpu::OverlayImageRepresentation> representation)
      : representation_(std::move(representation)) {}

  OverlayData(std::unique_ptr<gpu::OverlayImageRepresentation> representation,
              ScopedSharedImageMailbox quad_resource_copy)
      : representation_(std::move(representation)),
        quad_resource_copy_(std::move(quad_resource_copy)) {}

  ~OverlayData() = default;
  OverlayData(OverlayData&& other) = default;
  OverlayData& operator=(OverlayData&& other) {
    // `access_` must be overwritten before `representation_`.
    access_ = std::move(other.access_);
    representation_ = std::move(other.representation_);
    return *this;
  }

  std::optional<gl::DCLayerOverlayImage> BeginOverlayAccess() {
    CHECK(representation_);
    if (!access_) {
      access_ = representation_->BeginScopedReadAccess();
      if (!access_) {
        return std::nullopt;
      }
    }
    return access_->GetDCLayerOverlayImage();
  }

  void EndOverlayAccess() { access_.reset(); }

  // True of this overlay represents a quad with a non-scanout resource that
  // needs a separate scanout copy to send to DComp.
  bool IsQuadResourceCopy() const { return quad_resource_copy_.is_valid(); }

 private:
  std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess> access_;

  ScopedSharedImageMailbox quad_resource_copy_;
};

SkiaOutputDeviceDComp::SkiaOutputDeviceDComp(
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageFactory* shared_image_factory,
    gpu::SharedImageRepresentationFactory* shared_image_representation_factory,
    gpu::SharedContextState* context_state,
    scoped_refptr<gl::Presenter> presenter,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(context_state->gr_context(),
                       context_state->graphite_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      shared_image_representation_factory_(shared_image_representation_factory),
      context_state_(context_state),
      presenter_(std::move(presenter)),
      gr_shader_cache_(deps->GetGrShaderCache()),
      shared_image_manager_(base::raw_ref<gpu::SharedImageManager>::from_ptr(
          deps->GetSharedImageManager())),
      shared_image_factory_(base::raw_ref<gpu::SharedImageFactory>::from_ptr(
          shared_image_factory)) {
  DCHECK(!feature_info->workarounds()
              .disable_post_sub_buffers_for_onscreen_surfaces);
  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  // DWM handles preserving the contents of the backbuffer in Present1, so we
  // don't need to have SkiaOutputSurface handle it.
  capabilities_.preserve_buffer_content = false;
  capabilities_.number_of_buffers =
      gl::DirectCompositionRootSurfaceBufferCount();
  if (feature_info->workarounds().supports_two_yuv_hardware_overlays) {
    capabilities_.allowed_yuv_overlay_count = 2;
  }
  if (base::FeatureList::IsEnabled(
          features::kDirectCompositionUnlimitedOverlays)) {
    capabilities_.allowed_yuv_overlay_count = INT_MAX;
  }
  capabilities_.supports_dc_layers = true;
  capabilities_.supports_post_sub_buffer = true;
  capabilities_.supports_delegated_ink = presenter_->SupportsDelegatedInk();
  capabilities_.pending_swap_params.max_pending_swaps = 1;
  capabilities_.renderer_allocates_images = true;
  capabilities_.supports_viewporter = presenter_->SupportsViewporter();
  capabilities_.supports_non_backed_solid_color_overlays = true;

  DCHECK(context_state_);
  DCHECK(context_state_->gr_context() || context_state_->graphite_context());
  DCHECK(context_state_->context());
  DCHECK(presenter_);

  // SRGB
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kRGBA_8888_SkColorType;
  // HDR10
  capabilities_
      .sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_1010102)] =
      kRGBA_1010102_SkColorType;
  // scRGB linear
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_F16)] =
      kRGBA_F16_SkColorType;
}

SkiaOutputDeviceDComp::~SkiaOutputDeviceDComp() = default;

void SkiaOutputDeviceDComp::Present(const std::optional<gfx::Rect>& update_rect,
                                    BufferPresentedCallback feedback,
                                    OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  // The |update_rect| is ignored because the SharedImage backing already
  // knows the area to be swapped.
  presenter_->Present(
      base::BindOnce(&SkiaOutputDeviceDComp::OnPresentFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(frame), size_),
      std::move(feedback), frame.data);
}

void SkiaOutputDeviceDComp::OnPresentFinished(
    OutputSurfaceFrame frame,
    const gfx::Size& swap_size,
    gfx::SwapCompletionResult result) {
  // Remove entries from |overlays_| for textures that weren't scheduled as an
  // overlay this frame.
  if (!overlays_.empty()) {
    base::EraseIf(overlays_, [this](auto& entry) {
      const gpu::Mailbox& mailbox = entry.first;
      return !scheduled_overlay_mailboxes_.contains(mailbox);
    });
    scheduled_overlay_mailboxes_.clear();
    // End access for the remaining overlays that were scheduled this frame.
    for (auto& kv : overlays_)
      kv.second.EndOverlayAccess();
  }

  if (force_failure_on_next_swap_) {
    result.swap_result = gfx::SwapResult::SWAP_FAILED;
    force_failure_on_next_swap_ = false;
  }

  FinishSwapBuffers(std::move(result), swap_size, std::move(frame));
}

bool SkiaOutputDeviceDComp::EnsureDCompSurfaceCopiesForNonOverlayResources(
    const SkiaOutputSurface::OverlayList& overlays) {
  for (auto& overlay : overlays) {
    if (overlay.resource_size_in_pixels.IsEmpty() || overlay.mailbox.IsZero()) {
      // No resource to copy, we can safely skip this overlay.
      continue;
    }

    if (overlays_.contains(overlay.mailbox)) {
      // Resource already has an active overlay access.
      continue;
    }

    // Lookup the mailbox's usage after checking for existing copies since the
    // mailbox lookup is protected by a lock.
    const std::optional<uint32_t> candidate_image_usage =
        shared_image_manager_->GetUsageForMailbox(overlay.mailbox);
    const bool needs_dcomp_copy =
        candidate_image_usage.has_value() &&
        (candidate_image_usage.value() & gpu::SHARED_IMAGE_USAGE_SCANOUT) == 0;
    if (needs_dcomp_copy) {
      ScopedSharedImageMailbox quad_resource_copy = CopyQuadResource(
          context_state_, gr_shader_cache_, shared_image_factory_.get(),
          shared_image_representation_factory_,
          /*src_mailbox=*/overlay.mailbox);
      if (!quad_resource_copy.is_valid()) {
        LOG(ERROR) << "Failed to copy quad resource to dcomp surface.";
        return false;
      }

      std::unique_ptr<gpu::OverlayImageRepresentation> overlay_representation =
          shared_image_representation_factory_->ProduceOverlay(
              quad_resource_copy.get().mailbox());
      // We don't expect |DCompSurfaceImageBacking::ProduceOverlay| to fail.
      CHECK(overlay_representation);

      overlays_.emplace(overlay.mailbox,
                        OverlayData(std::move(overlay_representation),
                                    std::move(quad_resource_copy)));
    }
  }

  DVLOG(1) << "quad resource copies = "
           << base::ranges::count_if(
                  overlays_,
                  [](const auto& kv) { return kv.second.IsQuadResourceCopy(); })
           << " / overlays = " << overlays.size();

  return true;
}

void SkiaOutputDeviceDComp::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  if (base::FeatureList::IsEnabled(
          features::kCopyNonOverlayResourcesToDCompSurfaces)) {
    if (!EnsureDCompSurfaceCopiesForNonOverlayResources(overlays)) {
      ForceFailureOnNextSwap();
    }
  }

  for (auto& dc_layer : overlays) {
    // This is not necessarily an error, DCLayerTree can succeed with any
    // combination of overlay image and background color. However, it's wasteful
    // to have an overlay with no image or background color.
    if (dc_layer.mailbox.IsZero() && !dc_layer.color.has_value()) {
      // This can happen when |PrepareRenderPassOverlay| encounters a bypass
      // quad that is skipped.
      continue;
    }

    if (dc_layer.is_solid_color) {
      CHECK(dc_layer.color.has_value());
      CHECK(dc_layer.mailbox.IsZero());
    }

    auto params = std::make_unique<gl::DCLayerOverlayParams>();

    const gpu::Mailbox& mailbox = dc_layer.mailbox;
    if (!mailbox.IsZero()) {
      std::optional<gl::DCLayerOverlayImage> overlay_image =
          BeginOverlayAccess(mailbox);
      if (!overlay_image) {
        DLOG(ERROR) << "Failed to ProduceOverlay or GetDCLayerOverlayImage";
        continue;
      }
      params->overlay_image = std::move(overlay_image);
    }

    params->background_color = dc_layer.color;
    params->z_order = dc_layer.plane_z_order;

    // SwapChainPresenter uses the size of the overlay's resource in pixels to
    // calculate its swap chain size. `uv_rect` maps the portion of
    // `resource_size_in_pixels` that will be displayed.
    params->content_rect = gfx::ScaleRect(
        dc_layer.uv_rect, dc_layer.resource_size_in_pixels.width(),
        dc_layer.resource_size_in_pixels.height());

    params->quad_rect = gfx::ToRoundedRect(dc_layer.display_rect);
    CHECK(absl::holds_alternative<gfx::Transform>(dc_layer.transform));
    params->transform = absl::get<gfx::Transform>(dc_layer.transform);
    params->clip_rect = dc_layer.clip_rect;
    params->opacity = dc_layer.opacity;
    params->rounded_corner_bounds = dc_layer.rounded_corners;
    params->nearest_neighbor_filter = dc_layer.nearest_neighbor_filter;

    params->video_params.protected_video_type = dc_layer.protected_video_type;
    params->video_params.color_space = dc_layer.color_space;
    params->video_params.hdr_metadata = dc_layer.hdr_metadata;
    params->video_params.possible_video_fullscreen_letterboxing =
        dc_layer.possible_video_fullscreen_letterboxing;

    // Schedule DC layer overlay to be presented at next SwapBuffers().
    presenter_->ScheduleDCLayer(std::move(params));
    scheduled_overlay_mailboxes_.insert(mailbox);
  }
}

std::optional<gl::DCLayerOverlayImage>
SkiaOutputDeviceDComp::BeginOverlayAccess(const gpu::Mailbox& mailbox) {
  auto it = overlays_.find(mailbox);
  if (it != overlays_.end())
    return it->second.BeginOverlayAccess();

  auto overlay = shared_image_representation_factory_->ProduceOverlay(mailbox);
  if (!overlay)
    return std::nullopt;

  std::tie(it, std::ignore) = overlays_.emplace(mailbox, std::move(overlay));
  return it->second.BeginOverlayAccess();
}

void SkiaOutputDeviceDComp::ForceFailureOnNextSwap() {
  force_failure_on_next_swap_ = true;
}

bool SkiaOutputDeviceDComp::Reshape(const ReshapeParams& params) {
  DCHECK_EQ(params.transform, gfx::OVERLAY_TRANSFORM_NONE);

  auto size = params.GfxSize();

  // DCompPresenter calls SetWindowPos on resize, so we call it to reflect the
  // newly allocated root surface.
  // Note, we could inline SetWindowPos here, but we need access to the HWND.
  if (!presenter_->Resize(size, params.device_scale_factor, params.color_space,
                          /*has_alpha=*/!params.image_info.isOpaque())) {
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }

  size_ = size;

  return true;
}

SkSurface* SkiaOutputDeviceDComp::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SkiaOutputDeviceDComp::EndPaint() {
  NOTIMPLEMENTED();
}

bool SkiaOutputDeviceDComp::IsPrimaryPlaneOverlay() const {
  return true;
}

}  // namespace viz
