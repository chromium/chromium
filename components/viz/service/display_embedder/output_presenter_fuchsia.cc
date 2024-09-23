// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_fuchsia.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace viz {

OutputPresenterFuchsia::PendingFrame::PendingFrame() = default;
OutputPresenterFuchsia::PendingFrame::~PendingFrame() = default;

OutputPresenterFuchsia::PendingFrame::PendingFrame(PendingFrame&&) = default;
OutputPresenterFuchsia::PendingFrame&
OutputPresenterFuchsia::PendingFrame::operator=(PendingFrame&&) = default;

// static
std::unique_ptr<OutputPresenterFuchsia> OutputPresenterFuchsia::Create(
    ui::PlatformWindowSurface* window_surface,
    SkiaOutputSurfaceDependency* deps) {
  if (!base::FeatureList::IsEnabled(
          features::kUseSkiaOutputDeviceBufferQueue)) {
    return {};
  }

  return std::make_unique<OutputPresenterFuchsia>(window_surface, deps);
}

OutputPresenterFuchsia::OutputPresenterFuchsia(
    ui::PlatformWindowSurface* window_surface,
    SkiaOutputSurfaceDependency* deps)
    : window_surface_(window_surface), dependency_(deps) {
  CHECK(window_surface_);
}

OutputPresenterFuchsia::~OutputPresenterFuchsia() = default;

void OutputPresenterFuchsia::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities->supports_post_sub_buffer = false;
  capabilities->supports_surfaceless = true;

  capabilities->sk_color_type_map[SinglePlaneFormat::kRGBA_8888] =
      kRGBA_8888_SkColorType;
  capabilities->sk_color_type_map[SinglePlaneFormat::kBGRA_8888] =
      kRGBA_8888_SkColorType;
}

bool OutputPresenterFuchsia::Reshape(const ReshapeParams& params) {
  return true;
}

void OutputPresenterFuchsia::Present(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback,
    gfx::FrameData data) {
  // SwapBuffer() should be called only after scheduling primary plane.
  DCHECK(next_frame_ && next_frame_->native_pixmap);

  PendingFrame& frame = next_frame_.value();
  window_surface_->Present(
      std::move(frame.native_pixmap), std::move(frame.overlays),
      std::move(frame.acquire_fences), std::move(frame.release_fences),
      std::move(completion_callback), std::move(presentation_callback));

  next_frame_.reset();
}

void OutputPresenterFuchsia::ScheduleOverlayPlane(
    const OutputPresenter::OverlayPlaneCandidate& overlay_plane_candidate,
    ScopedOverlayAccess* access,
    std::unique_ptr<gfx::GpuFence> acquire_fence) {
  // TODO(msisov): this acquire fence is only valid when tiles are rastered for
  // scanout usage, which are used for DelegatedCompositing in LaCros. It's not
  // expected to have this fence created for fuchsia. As soon as a better place
  // for this fence is found, this will be removed. For now, add a dcheck that
  // verifies the fence is null.
  DCHECK(!acquire_fence);

  if (!next_frame_)
    next_frame_.emplace();

  auto pixmap = access ? access->GetNativePixmap() : nullptr;

  if (!pixmap) {
    DLOG(ERROR) << "Cannot access SysmemNativePixmap";
    return;
  }

  if (overlay_plane_candidate.is_root_render_pass) {
    DCHECK(!next_frame_->native_pixmap);
    next_frame_->native_pixmap = std::move(pixmap);

    // Pass the acquire fence to system compositor if one exists.
    gfx::GpuFenceHandle acqire_fence = access->TakeAcquireFence();
    if (!acqire_fence.is_null())
      next_frame_->acquire_fences.push_back(std::move(acqire_fence));

    // Create and pass a release fence to the system compositor too.
    gpu::ExternalSemaphore semaphore =
        gpu::ExternalSemaphore::Create(dependency_->GetVulkanContextProvider());
    DCHECK(semaphore.is_valid());
    auto release_fence = semaphore.TakeSemaphoreHandle().ToGpuFenceHandle();
    next_frame_->release_fences.push_back(release_fence.Clone());

    // The release fence is signaled when the primary plane buffer can be
    // reused, rather than after it's first presented, so added as release fence
    // for the current access directly.
    access->SetReleaseFence(std::move(release_fence));
  } else {
    // Max one overlay plane supported.
    DCHECK(next_frame_->overlays.empty());

    auto& overlay = next_frame_->overlays.emplace_back();
    overlay.pixmap = std::move(pixmap);
    overlay.overlay_plane_data = gfx::OverlayPlaneData(
        overlay_plane_candidate.plane_z_order,
        overlay_plane_candidate.transform, overlay_plane_candidate.display_rect,
        overlay_plane_candidate.uv_rect, !overlay_plane_candidate.is_opaque,
        gfx::ToRoundedRect(overlay_plane_candidate.damage_rect),
        overlay_plane_candidate.opacity, overlay_plane_candidate.priority_hint,
        overlay_plane_candidate.rounded_corners,
        overlay_plane_candidate.color_space,
        overlay_plane_candidate.hdr_metadata);
  }
}

}  // namespace viz
