// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <memory>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "base/debug/alias.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/dc_layer_overlay_image.h"
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
    NOTREACHED();
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

  std::optional<gl::DCLayerOverlayImage> GetOverlayAccess() {
    CHECK(representation_);
    if (!access_) {
      return std::nullopt;
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
                       context_state->graphite_shared_context(),
                       memory_tracker,
                       std::move(did_swap_buffer_complete_callback)),
      shared_image_representation_factory_(shared_image_representation_factory),
      context_state_(context_state),
      presenter_(std::move(presenter)) {
  DCHECK(!feature_info->workarounds()
              .disable_post_sub_buffers_for_onscreen_surfaces);
  capabilities_.uses_default_gl_framebuffer = true;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities_.number_of_buffers =
      gl::DirectCompositionRootSurfaceBufferCount();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDoubleBufferCompositing)) {
    // Use switch "double-buffer-compositing" to force 1 |max_pending_swaps|
    // when the feature |DCompTripleBufferRootSwapChain| is enabled.
    capabilities_.number_of_buffers = 2;
  }
  if (feature_info->workarounds().supports_two_yuv_hardware_overlays) {
    capabilities_.allowed_yuv_overlay_count = 2;
  }
  if (base::FeatureList::IsEnabled(
          features::kDirectCompositionUnlimitedOverlays)) {
    capabilities_.allowed_yuv_overlay_count = INT_MAX;
  }
  capabilities_.dc_support_level =
      gl::DirectCompositionTextureSupported()
          ? OutputSurface::DCSupportLevel::kDCompTexture
          : OutputSurface::DCSupportLevel::kDCLayers;
  capabilities_.supports_post_sub_buffer = true;
  capabilities_.supports_delegated_ink = presenter_->SupportsDelegatedInk();
  capabilities_.pending_swap_params.max_pending_swaps =
      capabilities_.number_of_buffers - 1;
  capabilities_.renderer_allocates_images = true;
  capabilities_.supports_viewporter = presenter_->SupportsViewporter();
  capabilities_.supports_non_backed_solid_color_overlays = true;

  DCHECK(context_state_);
  DCHECK(context_state_->gr_context() ||
         context_state_->graphite_shared_context());
  DCHECK(context_state_->context());
  DCHECK(presenter_);

  // SRGB
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBX_8888] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kRGBA_8888_SkColorType;
  capabilities_.sk_color_type_map[SinglePlaneFormat::kBGRX_8888] =
      kRGBA_8888_SkColorType;
  // HDR10
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_1010102] =
      kRGBA_1010102_SkColorType;
  // scRGB linear
  capabilities_.sk_color_type_map[SinglePlaneFormat::kRGBA_F16] =
      kRGBA_F16_SkColorType;
}

SkiaOutputDeviceDComp::~SkiaOutputDeviceDComp() {
  // `SkiaOutputDeviceDComp` is non-copyable and non-movable, so dtor will only
  // happen once.
  CHECK(presenter_);

  // We expect `SkiaOutputDeviceDComp::presenter_` to act like a unique pointer,
  // only owned by `SkiaOutputDeviceDComp`.
  CHECK(presenter_->HasOneRef());

  if (!presenter_->DestroyDCLayerTree()) {
    // If the `Commit` call in `~DCompPresenter` failed with device lost, exit
    // the process via context loss, since it would not be valid to clean up
    // `overlays_` if it contains DComp textures since they would still be
    // attached to the visual tree.
    context_state_->MarkContextLost();

    // We expect `MarkContextLost` to exit the GPU process synchronously.
    NOTREACHED();
  }
}

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
    if (result.swap_result == gfx::SwapResult::SWAP_ACK) {
      // If swap did not succeed, then the overlay images could still be in the
      // visual tree. It's not safe for us to end overlay access on DComp
      // textures since DWM could potentially still read from them. The images
      // held back in the swap failure case will either be cleaned up on next
      // successful swap or after GPU process restart.
      base::EraseIf(overlays_, [this](auto& entry) {
        const gpu::Mailbox& mailbox = entry.first;
        return !scheduled_overlay_mailboxes_.contains(mailbox);
      });
    }
    scheduled_overlay_mailboxes_.clear();

    for (auto& [mailbox, overlay_data] : overlays_) {
      if (auto overlay_image = overlay_data.GetOverlayAccess()) {
        if (overlay_image->type() ==
            gl::DCLayerOverlayType::kDCompVisualContent) {
          Microsoft::WRL::ComPtr<IDCompositionTexture> dcomp_texture;
          if (SUCCEEDED(Microsoft::WRL::ComPtr<IUnknown>(
                            overlay_image->dcomp_visual_content())
                            .As(&dcomp_texture))) {
            // We don't want to end the read access for DComp textures since DWM
            // can read from them for potentially multiple frames.
            continue;
          }
        }

        // The remaining overlays are either DComp surfaces (which do not
        // require special synchronization) or handled by |SwapChainPresenter|
        // (which copies the image into an internal swap chain, rather than
        // having DWM read it directly).
        overlay_data.EndOverlayAccess();
      }
    }
  }

  FinishSwapBuffers(std::move(result), swap_size, std::move(frame));
}

void SkiaOutputDeviceDComp::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  std::vector<gl::DCLayerOverlayParams> out_overlays;
  out_overlays.reserve(overlays.size());

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

    gl::DCLayerOverlayParams& params = out_overlays.emplace_back();

    params.background_color = dc_layer.color;
    params.z_order = dc_layer.plane_z_order;

    const gpu::Mailbox& mailbox = dc_layer.mailbox;
    if (!mailbox.IsZero()) {
      std::optional<gl::DCLayerOverlayImage> overlay_image =
          BeginOverlayAccess(mailbox);
      if (overlay_image) {
        params.overlay_image = std::move(overlay_image);
        scheduled_overlay_mailboxes_.insert(mailbox);
      } else {
        DLOG(ERROR) << "Failed to ProduceOverlay or GetDCLayerOverlayImage";
#if DCHECK_IS_ON()
        params.background_color = SkColors::kRed;
#else
        params.background_color = SkColors::kWhite;
#endif
      }
    }

    // SwapChainPresenter uses the size of the overlay's resource in pixels to
    // calculate its swap chain size. `uv_rect` maps the portion of
    // `resource_size_in_pixels` that will be displayed.
    params.content_rect = gfx::ScaleRect(
        dc_layer.uv_rect, dc_layer.resource_size_in_pixels.width(),
        dc_layer.resource_size_in_pixels.height());

    params.quad_rect = gfx::ToRoundedRect(dc_layer.display_rect);
    CHECK(std::holds_alternative<gfx::Transform>(dc_layer.transform));
    params.transform = std::get<gfx::Transform>(dc_layer.transform);
    params.clip_rect = dc_layer.clip_rect;
    params.opacity = dc_layer.opacity;
    params.rounded_corner_bounds = dc_layer.rounded_corners;
    params.nearest_neighbor_filter = dc_layer.nearest_neighbor_filter;
    params.layer_id = dc_layer.layer_id;
    params.video_params.protected_video_type = dc_layer.protected_video_type;
    params.video_params.color_space = dc_layer.color_space;
    params.video_params.hdr_metadata = dc_layer.hdr_metadata;
    params.video_params.is_p010_content =
        (dc_layer.format == MultiPlaneFormat::kP010);
    params.video_params.possible_video_fullscreen_letterboxing =
        dc_layer.possible_video_fullscreen_letterboxing;
  }

  // Schedule DC layer overlays to be presented at next SwapBuffers().
  presenter_->ScheduleDCLayers(std::move(out_overlays));
}

std::optional<gl::DCLayerOverlayImage>
SkiaOutputDeviceDComp::BeginOverlayAccess(const gpu::Mailbox& mailbox) {
  auto it = overlays_.find(mailbox);
  if (it != overlays_.end())
    return it->second.BeginOverlayAccess();

  auto overlay = shared_image_representation_factory_->ProduceOverlay(mailbox);
  if (!overlay)
    return std::nullopt;

  TRACE_EVENT2("gpu", "SkiaOutputDeviceDComp::BeginOverlayAccess",
               "debug_label", overlay->debug_label(), "image_size",
               overlay->size().ToString());

  std::tie(it, std::ignore) = overlays_.emplace(mailbox, std::move(overlay));
  return it->second.BeginOverlayAccess();
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

}  // namespace viz
