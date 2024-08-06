// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_gl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/presenter.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/base/ui_base_features.h"
#endif

namespace viz {

namespace {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OZONE)
// Helper function for moving a GpuFence from a fence handle to a unique_ptr.
std::unique_ptr<gfx::GpuFence> TakeGpuFence(gfx::GpuFenceHandle fence) {
  return fence.is_null() ? nullptr
                         : std::make_unique<gfx::GpuFence>(std::move(fence));
}
#endif

}  // namespace

OutputPresenterGL::OutputPresenterGL(scoped_refptr<gl::Presenter> presenter,
                                     SkiaOutputSurfaceDependency* deps)
    : presenter_(presenter), dependency_(deps) {}

OutputPresenterGL::~OutputPresenterGL() = default;

void OutputPresenterGL::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  capabilities->android_surface_control_feature_enabled = true;
  capabilities->supports_post_sub_buffer = true;
  capabilities->supports_viewporter = presenter_->SupportsViewporter();

  // Set supports_surfaceless to enable overlays.
  capabilities->supports_surfaceless = true;
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  // Set resize_based_on_root_surface to omit platform proposed size.
  capabilities->resize_based_on_root_surface =
      presenter_->SupportsOverridePlatformSize();
#if BUILDFLAG(IS_ANDROID)
  capabilities->supports_dynamic_frame_buffer_allocation = true;
#endif
  // MakeCurrent needs to be called if the platform can not rely on kernel (GPU
  // fences) to sync. In configurations like this, the Presenter commonly waits
  // on CPU for GPU to finish with a (EGL) fence + a worker thread.
  capabilities->present_requires_make_current =
      !presenter_->SupportsPlaneGpuFences();

  // TODO(crbug.com/40141277): only add supported formats base on
  // platform, driver, etc.
  capabilities->sk_color_type_map[SinglePlaneFormat::kBGR_565] =
      kRGB_565_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBA_4444] =
      kARGB_4444_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBX_8888] =
      kRGB_888x_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kRGBA_8888_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kBGRX_8888] =
      kBGRA_8888_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kBGRA_8888_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kBGRA_1010102] =
      kBGRA_1010102_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBA_1010102] =
      kRGBA_1010102_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBA_F16] =
      kRGBA_F16_SkColorType;
}

bool OutputPresenterGL::Reshape(const ReshapeParams& params) {
  const gfx::Size size = params.GfxSize();
  const bool has_alpha = !params.image_info.isOpaque();
  return presenter_->Resize(size, params.device_scale_factor,
                            params.color_space, has_alpha);
}

void OutputPresenterGL::Present(SwapCompletionCallback completion_callback,
                                BufferPresentedCallback presentation_callback,
                                gfx::FrameData data) {
  presenter_->Present(std::move(completion_callback),
                      std::move(presentation_callback), data);
}

void OutputPresenterGL::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
    ScopedOverlayAccess* access,
    std::unique_ptr<gfx::GpuFence> acquire_fence) {
  // Note that |overlay_plane_candidate| has different types on different
  // platforms. On Android, Ozone, and Windows, it is an OverlayCandidate and on
  // macOS it is a CALayeroverlay.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_OZONE)
#if BUILDFLAG(IS_OZONE)
  // TODO(crbug.com/40239878): Add ScopedOverlayAccess::GetOverlayImage() that
  // works on all platforms.
  gl::OverlayImage overlay_image = access ? access->GetNativePixmap() : nullptr;
#elif BUILDFLAG(IS_ANDROID)
  gl::OverlayImage overlay_image =
      access ? access->GetAHardwareBufferFenceSync() : nullptr;
#endif
  // TODO(msisov): Once shared image factory allows creating a non backed
  // images, remove the if condition that checks if this is a solid color
  // overlay plane.
  //
  // Solid color overlays can be non-backed and are delegated for processing
  // to underlying backend. The only backend that uses them is Wayland - it
  // may have a protocol that asks Wayland compositor to create a solid color
  // buffer for a client. OverlayProcessorDelegated decides if a solid color
  // overlay is an overlay candidate and should be scheduled.
  if (overlay_image || overlay_plane_candidate.is_solid_color) {
#if DCHECK_IS_ON()
    if (overlay_plane_candidate.is_solid_color) {
      LOG_IF(FATAL, !overlay_plane_candidate.color.has_value())
          << "Solid color quads must have color set.";
    }

    if (acquire_fence && !acquire_fence->GetGpuFenceHandle().is_null()) {
      CHECK(access);
      CHECK_EQ(gpu::GrContextType::kGL,
               dependency_->GetSharedContextState()->gr_context_type());
      CHECK(features::IsDelegatedCompositingEnabled());
      CHECK(access->representation()->usage().Has(
          gpu::SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING));
    }
#endif

    // Access fence takes priority over composite fence iff it exists.
    if (access) {
      auto access_fence = TakeGpuFence(access->TakeAcquireFence());
      if (access_fence) {
        DCHECK(!acquire_fence);
        acquire_fence = std::move(access_fence);
      }
    }

    presenter_->ScheduleOverlayPlane(
        std::move(overlay_image), std::move(acquire_fence),
        gfx::OverlayPlaneData(
            overlay_plane_candidate.plane_z_order,
            overlay_plane_candidate.transform,
            overlay_plane_candidate.display_rect,
            overlay_plane_candidate.uv_rect, !overlay_plane_candidate.is_opaque,
            ToEnclosingRect(overlay_plane_candidate.damage_rect),
            overlay_plane_candidate.opacity,
            overlay_plane_candidate.priority_hint,
            overlay_plane_candidate.rounded_corners,
            overlay_plane_candidate.color_space,
            overlay_plane_candidate.hdr_metadata, overlay_plane_candidate.color,
            overlay_plane_candidate.is_solid_color,
            overlay_plane_candidate.is_root_render_pass,
            overlay_plane_candidate.clip_rect,
            overlay_plane_candidate.overlay_type));
  }
#elif BUILDFLAG(IS_APPLE)
  presenter_->ScheduleCALayer(ui::CARendererLayerParams(
      overlay_plane_candidate.clip_rect.has_value(),
      overlay_plane_candidate.clip_rect.value_or(gfx::Rect()),
      overlay_plane_candidate.rounded_corners,
      overlay_plane_candidate.sorting_context_id,
      absl::get<gfx::Transform>(overlay_plane_candidate.transform),
      access ? access->GetIOSurface() : gfx::ScopedIOSurface(),
      access ? access->representation()->color_space() : gfx::ColorSpace(),
      overlay_plane_candidate.uv_rect,
      gfx::ToEnclosingRect(overlay_plane_candidate.display_rect),
      overlay_plane_candidate.color.value_or(SkColors::kTransparent),
      overlay_plane_candidate.edge_aa_mask, overlay_plane_candidate.opacity,
      overlay_plane_candidate.nearest_neighbor_filter,
      overlay_plane_candidate.hdr_metadata,
      overlay_plane_candidate.protected_video_type,
      overlay_plane_candidate.is_render_pass_draw_quad));

#endif
}

void OutputPresenterGL::SetVSyncDisplayID(int64_t display_id) {
  presenter_->SetVSyncDisplayID(display_id);
}

#if BUILDFLAG(IS_APPLE)
void OutputPresenterGL::SetMaxPendingSwaps(int max_pending_swaps) {
  presenter_->SetMaxPendingSwaps(max_pending_swaps);
}
#endif

}  // namespace viz
