// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_interface.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "ui/gfx/overlay_priority_hint.h"

#if BUILDFLAG(IS_APPLE)
#include "components/viz/service/display/overlay_processor_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "components/viz/service/display/overlay_processor_win.h"
#elif BUILDFLAG(IS_ANDROID)
#include "components/viz/service/display/overlay_processor_android.h"
#include "components/viz/service/display/overlay_processor_surface_control.h"
#elif BUILDFLAG(IS_OZONE)
#include "components/viz/service/display/overlay_processor_delegated.h"
#include "components/viz/service/display/overlay_processor_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {
namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnderlayDamage {
  kZeroDamageRect,
  kNonOccludingDamageOnly,
  kOccludingDamageOnly,  // deprecated
  kOccludingAndNonOccludingDamages,
  kMaxValue = kOccludingAndNonOccludingDamages,
};
}  // namespace

// Record UMA histograms for overlays
// 1. Underlay vs. Overlay
// 2. Full screen mode vs. Non Full screen (Windows) mode
// 3. Overlay zero damage rect vs. non zero damage rect
// 4. Underlay zero damage rect, non-zero damage rect with non-occluding damage
//   only, non-zero damage rect with occluding damage, and non-zero damage rect
//   with both damages

// static
void OverlayProcessorInterface::RecordOverlayDamageRectHistograms(
    bool is_overlay,
    bool has_occluding_surface_damage,
    bool zero_damage_rect) {
  if (is_overlay) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayCompositor.RootDamageRect.Overlay",
                          !zero_damage_rect);
  } else {  // underlay
    UnderlayDamage underlay_damage = UnderlayDamage::kZeroDamageRect;
    if (zero_damage_rect) {
      underlay_damage = UnderlayDamage::kZeroDamageRect;
    } else {
      if (has_occluding_surface_damage) {
        underlay_damage = UnderlayDamage::kOccludingAndNonOccludingDamages;
      } else {
        underlay_damage = UnderlayDamage::kNonOccludingDamageOnly;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.RootDamageRect.Underlay",
                              underlay_damage);
  }
}

OverlayProcessorInterface::OutputSurfaceOverlayPlane::
    OutputSurfaceOverlayPlane() = default;
OverlayProcessorInterface::OutputSurfaceOverlayPlane::OutputSurfaceOverlayPlane(
    const OutputSurfaceOverlayPlane&) = default;
OverlayProcessorInterface::OutputSurfaceOverlayPlane&
OverlayProcessorInterface::OutputSurfaceOverlayPlane::operator=(
    const OutputSurfaceOverlayPlane&) = default;
OverlayProcessorInterface::OutputSurfaceOverlayPlane::
    ~OutputSurfaceOverlayPlane() = default;

std::unique_ptr<OverlayProcessorInterface>
OverlayProcessorInterface::CreateOverlayProcessor(
    OutputSurface* output_surface,
    gpu::SurfaceHandle surface_handle,
    const OutputSurface::Capabilities& capabilities,
    DisplayCompositorMemoryAndTaskController* display_controller,
    gpu::SharedImageInterface* shared_image_interface,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings) {
  // If we are offscreen, we don't have overlay support.
  // TODO(vasilyt): WebView would have a kNullSurfaceHandle. Make sure when
  // overlay for WebView is enabled, this check still works.
  if (surface_handle == gpu::kNullSurfaceHandle)
    return std::make_unique<OverlayProcessorStub>();

#if BUILDFLAG(IS_APPLE)
  DCHECK(capabilities.supports_surfaceless);
  return std::make_unique<OverlayProcessorMac>();
#elif BUILDFLAG(IS_WIN)
  if (capabilities.dc_support_level == OutputSurface::DCSupportLevel::kNone) {
    return std::make_unique<OverlayProcessorStub>();
  }

  DCHECK(display_controller);
  DCHECK(display_controller->skia_dependency());
  return std::make_unique<OverlayProcessorWin>(
      capabilities.dc_support_level, debug_settings,
      std::make_unique<DCLayerOverlayProcessor>(
          capabilities.allowed_yuv_overlay_count,
          display_controller->skia_dependency()
              ->GetGpuDriverBugWorkarounds()
              .disable_video_overlay_if_moving));

#elif BUILDFLAG(IS_OZONE)
#if !BUILDFLAG(IS_CASTOS)
  // In tests and Ozone/X11, we do not expect surfaceless surface support.
  // For CastOS, we always need OverlayProcessorOzone.
  if (!capabilities.supports_surfaceless)
    return std::make_unique<OverlayProcessorStub>();
#endif  // #if !BUILDFLAG(IS_CASTOS)

  gpu::SharedImageInterface* sii = nullptr;
  auto* overlay_manager = ui::OzonePlatform::GetInstance()->GetOverlayManager();
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates;
  if (overlay_manager) {
    overlay_candidates =
        overlay_manager->CreateOverlayCandidates(surface_handle);
    if (overlay_manager->allow_sync_and_real_buffer_page_flip_testing()) {
      sii = shared_image_interface;
      CHECK(shared_image_interface);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<OverlayProcessorDelegated>(
      std::move(overlay_candidates),
      std::move(renderer_settings.overlay_strategies), sii);
#else
  return std::make_unique<OverlayProcessorOzone>(
      std::move(overlay_candidates),
      std::move(renderer_settings.overlay_strategies), sii);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#elif BUILDFLAG(IS_ANDROID)
  DCHECK(display_controller);

  if (capabilities.supports_surfaceless) {
    // This is for Android SurfaceControl case.
    return std::make_unique<OverlayProcessorSurfaceControl>();
  } else {
    // When SurfaceControl is enabled, any resource backed by
    // an AHardwareBuffer can be marked as an overlay candidate but it requires
    // that we use a SurfaceControl backed GLSurface. If we're creating a
    // native window backed GLSurface, the overlay processing code will
    // incorrectly assume these resources can be overlaid. So we disable all
    // overlay processing for this OutputSurface.
    if (capabilities.android_surface_control_feature_enabled)
      return std::make_unique<OverlayProcessorStub>();

    return std::make_unique<OverlayProcessorAndroid>(display_controller);
  }
#else  // Default
  return std::make_unique<OverlayProcessorStub>();
#endif
}

bool OverlayProcessorInterface::DisableSplittingQuads() const {
  return false;
}

OverlayProcessorInterface::OutputSurfaceOverlayPlane
OverlayProcessorInterface::ProcessOutputSurfaceAsOverlay(
    const gfx::Size& viewport_size,
    const gfx::Size& resource_size,
    const SharedImageFormat si_format,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    float opacity,
    const gpu::Mailbox& mailbox) {
  OutputSurfaceOverlayPlane overlay_plane;
  overlay_plane.transform = gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  overlay_plane.uv_rect = gfx::RectF(
      0.f, 0.f,
      viewport_size.width() / static_cast<float>(resource_size.width()),
      viewport_size.height() / static_cast<float>(resource_size.height()));
  overlay_plane.resource_size = resource_size;
  overlay_plane.format = si_format;
  overlay_plane.color_space = color_space;
  overlay_plane.enable_blending = has_alpha;
  overlay_plane.opacity = opacity;
  overlay_plane.mailbox = mailbox;
  overlay_plane.priority_hint = gfx::OverlayPriorityHint::kRegular;
  overlay_plane.rounded_corners = gfx::RRectF();

  // Adjust transformation and display_rect based on display rotation.
  overlay_plane.display_rect =
      gfx::RectF(viewport_size.width(), viewport_size.height());

#if BUILDFLAG(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
  // On Chromecast, always use RGBA as the scanout format for the primary plane.
  overlay_plane.enable_blending = true;
#endif
  return overlay_plane;
}

void OverlayProcessorInterface::ScheduleOverlays(
    DisplayResourceProvider* display_resource_provider) {}

void OverlayProcessorInterface::OverlayPresentationComplete() {}

gfx::CALayerResult OverlayProcessorInterface::GetCALayerErrorCode() const {
  return gfx::kCALayerSuccess;
}

gfx::RectF OverlayProcessorInterface::GetUnassignedDamage() const {
  return gfx::RectF();
}

bool OverlayProcessorInterface::SupportsFlipRotateTransform() const {
  return false;
}

}  // namespace viz
