// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_gl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
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

// Helper function for moving a GpuFence from a fence handle to a unique_ptr.
std::unique_ptr<gfx::GpuFence> TakeGpuFence(gfx::GpuFenceHandle fence) {
  return fence.is_null() ? nullptr
                         : std::make_unique<gfx::GpuFence>(std::move(fence));
}

class PresenterImageGL : public OutputPresenter::Image {
 public:
  PresenterImageGL(
      gpu::SharedImageFactory* factory,
      gpu::SharedImageRepresentationFactory* representation_factory,
      SkiaOutputSurfaceDependency* deps)
      : Image(factory, representation_factory, deps) {}
  ~PresenterImageGL() override = default;

  void BeginPresent() final;
  void EndPresent(gfx::GpuFenceHandle release_fence) final;
  int GetPresentCount() const final;
  void OnContextLost() final;

  gl::OverlayImage GetOverlayImage(std::unique_ptr<gfx::GpuFence>* fence);

  const gfx::ColorSpace& color_space() {
    DCHECK(overlay_representation_);
    return overlay_representation_->color_space();
  }
};

void PresenterImageGL::BeginPresent() {
  if (++present_count_ != 1) {
    DCHECK(scoped_overlay_read_access_);
    return;
  }

  DCHECK(!sk_surface());
  DCHECK(!scoped_overlay_read_access_);

  scoped_overlay_read_access_ =
      overlay_representation_->BeginScopedReadAccess();
  DCHECK(scoped_overlay_read_access_);
}

void PresenterImageGL::EndPresent(gfx::GpuFenceHandle release_fence) {
  DCHECK(present_count_);
  if (--present_count_)
    return;

  scoped_overlay_read_access_->SetReleaseFence(std::move(release_fence));

  scoped_overlay_read_access_.reset();
}

int PresenterImageGL::GetPresentCount() const {
  return present_count_;
}

void PresenterImageGL::OnContextLost() {
  if (overlay_representation_)
    overlay_representation_->OnContextLost();
}

gl::OverlayImage PresenterImageGL::GetOverlayImage(
    std::unique_ptr<gfx::GpuFence>* fence) {
  DCHECK(scoped_overlay_read_access_);
  if (fence) {
    *fence = TakeGpuFence(scoped_overlay_read_access_->TakeAcquireFence());
  }
#if BUILDFLAG(IS_OZONE)
  return scoped_overlay_read_access_->GetNativePixmap();
#elif BUILDFLAG(IS_APPLE)
  return scoped_overlay_read_access_->GetIOSurface();
#elif BUILDFLAG(IS_ANDROID)
  return scoped_overlay_read_access_->GetAHardwareBufferFenceSync();
#else
  LOG(FATAL) << "GetOverlayImage() is not implemented on this platform".
#endif
}

}  // namespace

// static
const uint32_t OutputPresenterGL::kDefaultSharedImageUsage =
    gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
    gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
    gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;

OutputPresenterGL::OutputPresenterGL(
    scoped_refptr<gl::Presenter> presenter,
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory,
    uint32_t shared_image_usage)
    : presenter_(presenter),
      dependency_(deps),
      shared_image_factory_(factory),
      shared_image_representation_factory_(representation_factory),
      shared_image_usage_(shared_image_usage) {}

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

  // TODO(https://crbug.com/1108406): only add supported formats base on
  // platform, driver, etc.
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::BGR_565)] =
      kRGB_565_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_4444)] =
      kARGB_4444_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kRGB_888x_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kRGBA_8888_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kBGRA_8888_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kBGRA_8888_SkColorType;
  capabilities
      ->sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_1010102)] =
      kBGRA_1010102_SkColorType;
  capabilities
      ->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_1010102)] =
      kRGBA_1010102_SkColorType;
  capabilities->sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_F16)] =
      kRGBA_F16_SkColorType;
}

bool OutputPresenterGL::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  const gfx::Size size = gfx::SkISizeToSize(characterization.dimensions());
  image_format_ = SkColorTypeToResourceFormat(characterization.colorType());
  const bool has_alpha =
      !SkAlphaTypeIsOpaque(characterization.imageInfo().alphaType());
  return presenter_->Resize(size, device_scale_factor, color_space, has_alpha);
}

std::vector<std::unique_ptr<OutputPresenter::Image>>
OutputPresenterGL::AllocateImages(gfx::ColorSpace color_space,
                                  gfx::Size image_size,
                                  size_t num_images) {
  std::vector<std::unique_ptr<Image>> images;
  for (size_t i = 0; i < num_images; ++i) {
    auto image = std::make_unique<PresenterImageGL>(
        shared_image_factory_, shared_image_representation_factory_,
        dependency_);
    if (!image->Initialize(image_size, color_space,
                           SharedImageFormat::SinglePlane(image_format_),
                           shared_image_usage_)) {
      DLOG(ERROR) << "Failed to initialize image.";
      return {};
    }
    images.push_back(std::move(image));
  }

  return images;
}

std::unique_ptr<OutputPresenter::Image> OutputPresenterGL::AllocateSingleImage(
    gfx::ColorSpace color_space,
    gfx::Size image_size) {
  auto image = std::make_unique<PresenterImageGL>(
      shared_image_factory_, shared_image_representation_factory_, dependency_);
  if (!image->Initialize(image_size, color_space,
                         SharedImageFormat::SinglePlane(image_format_),
                         shared_image_usage_)) {
    DLOG(ERROR) << "Failed to initialize image.";
    return nullptr;
  }
  return image;
}

void OutputPresenterGL::Present(SwapCompletionCallback completion_callback,
                                BufferPresentedCallback presentation_callback,
                                gfx::FrameData data) {
#if BUILDFLAG(IS_APPLE)
  presenter_->SetCALayerErrorCode(ca_layer_error_code_);
#endif
  presenter_->Present(std::move(completion_callback),
                      std::move(presentation_callback), data);
}

void OutputPresenterGL::SchedulePrimaryPlane(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
    Image* image,
    bool is_submitted) {
  std::unique_ptr<gfx::GpuFence> fence;
  auto* presenter_image = static_cast<PresenterImageGL*>(image);
  // If the submitted_image() is being scheduled, we don't new a new fence.
  gl::OverlayImage overlay_image = presenter_image->GetOverlayImage(
      (is_submitted || !presenter_->SupportsPlaneGpuFences()) ? nullptr
                                                              : &fence);

  // Output surface is also z-order 0.
  constexpr int kPlaneZOrder = 0;
  // TODO(edcourtney): We pass a full damage rect - actual damage is passed via
  // PostSubBuffer. As part of unifying the handling of the primary plane and
  // overlays, damage should be added to OutputSurfaceOverlayPlane and passed in
  // here.
  presenter_->ScheduleOverlayPlane(
      std::move(overlay_image), std::move(fence),
      gfx::OverlayPlaneData(
          kPlaneZOrder, plane.transform, plane.display_rect, plane.uv_rect,
          plane.enable_blending,
          plane.damage_rect.value_or(gfx::Rect(plane.resource_size)),
          plane.opacity, plane.priority_hint, plane.rounded_corners,
          presenter_image->color_space(),
          /*hdr_metadata=*/absl::nullopt));
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
  // TODO(crbug.com/1366808): Add ScopedOverlayAccess::GetOverlayImage() that
  // works on all platforms.
  gl::OverlayImage overlay_image = access ? access->GetNativePixmap() : nullptr;
#elif BUILDFLAG(IS_ANDROID)
  gl::OverlayImage overlay_image =
      access ? access->GetAHardwareBufferFenceSync() : nullptr;
#endif
  // TODO(msisov): Once shared image factory allows creating a non backed
  // images and ScheduleOverlayPlane does not rely on GLImage, remove the if
  // condition that checks if this is a solid color overlay plane.
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
      CHECK_EQ(gpu::GrContextType::kGL, dependency_->gr_context_type());
      CHECK(features::IsDelegatedCompositingEnabled());
      CHECK(access->representation()->usage() &
            gpu::SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING);
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
            absl::get<gfx::OverlayTransform>(overlay_plane_candidate.transform),
            overlay_plane_candidate.display_rect,
            overlay_plane_candidate.uv_rect, !overlay_plane_candidate.is_opaque,
            ToEnclosingRect(overlay_plane_candidate.damage_rect),
            overlay_plane_candidate.opacity,
            overlay_plane_candidate.priority_hint,
            overlay_plane_candidate.rounded_corners,
            overlay_plane_candidate.color_space,
            overlay_plane_candidate.hdr_metadata, overlay_plane_candidate.color,
            overlay_plane_candidate.is_solid_color,
            overlay_plane_candidate.clip_rect));
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
      overlay_plane_candidate.hdr_mode, overlay_plane_candidate.hdr_metadata,
      overlay_plane_candidate.protected_video_type));
#endif
}

bool OutputPresenterGL::SupportsGpuVSync() const {
  return presenter_->SupportsGpuVSync();
}

void OutputPresenterGL::SetGpuVSyncEnabled(bool enabled) {
  presenter_->SetGpuVSyncEnabled(enabled);
}

void OutputPresenterGL::SetVSyncDisplayID(int64_t display_id) {
  presenter_->SetVSyncDisplayID(display_id);
}

#if BUILDFLAG(IS_APPLE)
void OutputPresenterGL::SetCALayerErrorCode(
    gfx::CALayerResult ca_layer_error_code) {
  ca_layer_error_code_ = ca_layer_error_code;
}

#endif

}  // namespace viz
