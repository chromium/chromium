// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <tuple>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"

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

}  // namespace

// Holds reference needed to keep overlay textures alive.
// TODO(kylechar): We can probably merge OverlayData in with
// SkiaOutputSurfaceImplOnGpu overlay data.
class SkiaOutputDeviceDComp::OverlayData {
 public:
  explicit OverlayData(
      std::unique_ptr<gpu::OverlayImageRepresentation> representation)
      : representation_(std::move(representation)) {}

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

 private:
  std::unique_ptr<gpu::OverlayImageRepresentation> representation_;
  std::unique_ptr<gpu::OverlayImageRepresentation::ScopedReadAccess> access_;
};

SkiaOutputDeviceDComp::SkiaOutputDeviceDComp(
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
      presenter_(std::move(presenter)) {
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

  // The |update_rect| is ignored because SetDrawRectangle specified the area to
  // be swapped.
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

  FinishSwapBuffers(std::move(result), swap_size, std::move(frame));
}

void SkiaOutputDeviceDComp::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  for (auto& dc_layer : overlays) {
    // Only use the first shared image mailbox for accessing as an overlay.
    const gpu::Mailbox& mailbox = dc_layer.mailbox;
    std::optional<gl::DCLayerOverlayImage> overlay_image =
        BeginOverlayAccess(mailbox);
    if (!overlay_image) {
      DLOG(ERROR) << "Failed to ProduceOverlay or GetDCLayerOverlayImage";
      continue;
    }

    auto params = std::make_unique<gl::DCLayerOverlayParams>();
    params->overlay_image = std::move(overlay_image);
    params->z_order = dc_layer.plane_z_order;

    // SwapChainPresenter uses the size of the overlay's resource in pixels to
    // calculate its swap chain size. `uv_rect` maps the portion of
    // `resource_size_in_pixels` that will be displayed.
    params->content_rect = gfx::ScaleRect(
        dc_layer.uv_rect, dc_layer.resource_size_in_pixels.width(),
        dc_layer.resource_size_in_pixels.height());

    params->quad_rect = gfx::ToEnclosingRect(dc_layer.display_rect);
    DCHECK(absl::holds_alternative<gfx::Transform>(dc_layer.transform));
    params->transform = absl::get<gfx::Transform>(dc_layer.transform);
    params->clip_rect = dc_layer.clip_rect;
    params->protected_video_type = dc_layer.protected_video_type;
    params->color_space = dc_layer.color_space;
    params->hdr_metadata = dc_layer.hdr_metadata;
    params->possible_video_fullscreen_letterboxing =
        dc_layer.possible_video_fullscreen_letterboxing;

    // Schedule DC layer overlay to be presented at next SwapBuffers().
    if (!presenter_->ScheduleDCLayer(std::move(params))) {
      DLOG(ERROR) << "ScheduleDCLayer failed";
      continue;
    }
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

bool SkiaOutputDeviceDComp::Reshape(const SkImageInfo& image_info,
                                    const gfx::ColorSpace& color_space,
                                    int sample_count,
                                    float device_scale_factor,
                                    gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  auto size = gfx::SkISizeToSize(image_info.dimensions());

  // DCompPresenter calls SetWindowPos on resize, so we call it to reflect the
  // newly allocated root surface.
  // Note, we could inline SetWindowPos here, but we need access to the HWND.
  if (!presenter_->Resize(size, device_scale_factor, color_space,
                          /*has_alpha=*/!image_info.isOpaque())) {
    CheckForLoopFailures();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }

  size_ = size;

  return true;
}

bool SkiaOutputDeviceDComp::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  return presenter_->SetDrawRectangle(draw_rectangle);
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
