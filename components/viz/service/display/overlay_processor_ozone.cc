// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display/overlay_processor_ozone.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "gpu/config/gpu_finch_features.h"
#include "ui/display/display_features.h"
#include "ui/gl/gl_switches.h"
#endif

#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#endif

namespace viz {

namespace {

gfx::ColorSpace GetColorSpaceForOzone(gfx::BufferFormat format,
                                      const gfx::ColorSpace& orig_color_space) {
#if BUILDFLAG(IS_CHROMEOS)
  // The goal here is to ensure that the hw overlay path and the compositing
  // path produce visually identical results. Otherwise, the user would perceive
  // flickering when a particular resource (like a video) goes in and out of hw
  // overlays repeatedly (for whatever reason).
  //
  // The Vulkan path is able to treat YUV 4:2:0 BT.601 and BT.709 resources
  // correctly (see gpu::CreateVulkanYcbcrConversionInfo()). The GL path
  // currently doesn't and just treats YUV 4:2:0 BT.709 resources as if they
  // were BT.601 (but see the TODO below). Therefore, when Vulkan is enabled,
  // we allow the overlay path to also distinguish between the two YUV
  // encodings. Otherwise, we always use BT.601. This should ensure visual
  // consistency between the compositing and the hw overlays path in both GL
  // and Vulkan ChromeOS devices.
  //
  // TODO(b/233667677): the only reason the GL path treats YUV 4:2:0 BT.709
  // resources as BT.601 is because of https://crrev.com/c/2336347. If we can
  // be confident that hw overlays on all platforms can deal with BT.601 and
  // BT.709 correctly, then we don't need GetColorSpaceForOzone() at all and we
  // can just the pass the color space as-is to Ozone (and we would also need to
  // change the GL path to distinguish between BT.601 and BT.709, see
  // NativePixmapEGLBinding::InitializeFromNativePixmap()).
  if ((format == gfx::BufferFormat::YUV_420_BIPLANAR) &&
      (!base::FeatureList::IsEnabled(features::kVulkan) ||
       !base::FeatureList::IsEnabled(features::kDefaultANGLEVulkan) ||
       !base::FeatureList::IsEnabled(features::kVulkanFromANGLE))) {
    return orig_color_space.GetWithMatrixAndRange(
        gfx::ColorSpace::MatrixID::SMPTE170M, orig_color_space.GetRangeID());
  }
#endif
  return orig_color_space;
}

// TODO(weiliangc): When difference between primary plane and non-primary plane
// can be internalized, merge these two helper functions.
void ConvertToOzoneOverlaySurface(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& primary_plane,
    ui::OverlaySurfaceCandidate* ozone_candidate) {
  ozone_candidate->transform = primary_plane.transform;
  ozone_candidate->format =
      SinglePlaneSharedImageFormatToBufferFormat(primary_plane.format);
  ozone_candidate->color_space = GetColorSpaceForOzone(
      ozone_candidate->format, /*orig_color_space=*/primary_plane.color_space);
  ozone_candidate->display_rect = primary_plane.display_rect;
  ozone_candidate->crop_rect = primary_plane.uv_rect;
  ozone_candidate->clip_rect.reset();
  ozone_candidate->is_opaque = !primary_plane.enable_blending;
  ozone_candidate->opacity = primary_plane.opacity;
  ozone_candidate->plane_z_order = 0;
  ozone_candidate->buffer_size = primary_plane.resource_size;
  ozone_candidate->priority_hint = primary_plane.priority_hint;
  ozone_candidate->rounded_corners = primary_plane.rounded_corners;
}

void ConvertToOzoneOverlaySurface(
    const OverlayCandidate& overlay_candidate,
    ui::OverlaySurfaceCandidate* ozone_candidate) {
  ozone_candidate->transform = overlay_candidate.transform;
  ozone_candidate->format = gpu::ToBufferFormat(overlay_candidate.format);
  ozone_candidate->color_space =
      GetColorSpaceForOzone(ozone_candidate->format,
                            /*orig_color_space=*/overlay_candidate.color_space);
  ozone_candidate->display_rect = overlay_candidate.display_rect;
  ozone_candidate->crop_rect = overlay_candidate.uv_rect;
  ozone_candidate->clip_rect = overlay_candidate.clip_rect;
  ozone_candidate->is_opaque = overlay_candidate.is_opaque;
  ozone_candidate->opacity = overlay_candidate.opacity;
  ozone_candidate->plane_z_order = overlay_candidate.plane_z_order;
  ozone_candidate->buffer_size = overlay_candidate.resource_size_in_pixels;
  ozone_candidate->requires_overlay = overlay_candidate.requires_overlay;
  ozone_candidate->priority_hint = overlay_candidate.priority_hint;
  ozone_candidate->rounded_corners = overlay_candidate.rounded_corners;
  ozone_candidate->overlay_type = overlay_candidate.overlay_type;
  // TODO(crbug.com/40219248): OverlaySurfaceCandidate to SkColor4f
  // That can be a solid color quad.
  if (!overlay_candidate.is_solid_color && ozone_candidate->background_color &&
      overlay_candidate.color) {
    ozone_candidate->background_color = overlay_candidate.color->toSkColor();
  }
}

void ConvertToTiledOzoneOverlaySurface(
    const OverlayCandidate& overlay_candidate,
    ui::OverlaySurfaceCandidate* ozone_candidate) {
  ozone_candidate->transform = gfx::OVERLAY_TRANSFORM_NONE;
  ozone_candidate->format = gfx::BufferFormat::RGBA_8888;
  ozone_candidate->color_space =
      GetColorSpaceForOzone(ozone_candidate->format,
                            /*orig_color_space=*/overlay_candidate.color_space);
  ozone_candidate->display_rect = overlay_candidate.display_rect;
  ozone_candidate->crop_rect = gfx::RectF(1.0, 1.0);
  ozone_candidate->clip_rect = std::nullopt;
  ozone_candidate->is_opaque = overlay_candidate.is_opaque;
  ozone_candidate->opacity = overlay_candidate.opacity;
  ozone_candidate->plane_z_order = overlay_candidate.plane_z_order;
  ozone_candidate->buffer_size =
      gfx::Size(static_cast<int>(overlay_candidate.display_rect.width()),
                static_cast<int>(overlay_candidate.display_rect.height()));
  ozone_candidate->requires_overlay = true;
  ozone_candidate->priority_hint = overlay_candidate.priority_hint;
  ozone_candidate->rounded_corners = overlay_candidate.rounded_corners;
  ozone_candidate->native_pixmap = nullptr;
  ozone_candidate->overlay_type = overlay_candidate.overlay_type;
}

uint32_t MailboxToUInt32(const gpu::Mailbox& mailbox) {
  return (mailbox.name[0] << 24) + (mailbox.name[1] << 16) +
         (mailbox.name[2] << 8) + mailbox.name[3];
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsYUVColorSpace(const gfx::ColorSpace& color_space) {
  SkYUVColorSpace yuv_color_space;
  return color_space.ToSkYUVColorSpace(&yuv_color_space);
}

bool AllowColorSpaceCombination(
    SharedImageFormat source_format,
    const gfx::ColorSpace& source_color_space,
    const gfx::ColorSpace& destination_color_space) {
  // Allow invalid source color spaces because the assumption is that the
  // compositor won't do a color space conversion in this case anyway, so it
  // should be consistent with the overlay path.
  if (!source_color_space.IsValid())
    return true;

  // Since https://crrev.com/c/2336347, we force BT.601/narrow for the
  // COLOR_ENCODING and COLOR_RANGE DRM/KMS properties. On the other hand, the
  // compositor is able to handle different YUV encodings and ranges. Therefore,
  // in theory, if we don't want to see a difference between overlays and
  // compositing, we should not promote video frames to overlays unless they
  // actually use BT.601/narrow.
  //
  // In practice, however, we expect to see lots of BT.709 video frames, and we
  // don't want to reject all of them for overlays because the visual difference
  // between BT.601/narrow and BT.709/narrow is not expected to be much.
  // Therefore, in being consistent with the values we provide for
  // EGL_YUV_COLOR_SPACE_HINT_EXT/EGL_SAMPLE_RANGE_HINT_EXT, we'll only allow
  // frames that use non-BT.2020 with non-full range. In those cases, the
  // compositor and the display controller are expected to render the frames
  // equally (and decently - with the understanding that the final result may
  // not be fully correct).
  //
  // TODO(b/233667677): Remove this when we've plumbed the YUV encoding and
  // range to DRM/KMS. At that point, we need to ensure that
  // EGL_YUV_COLOR_SPACE_HINT_EXT/EGL_SAMPLE_RANGE_HINT_EXT would also get the
  // same values as DRM/KMS.
  //
  // TODO(b/243150091): Remove the call to IsYUVColorSpace() or turn it into a
  // DCHECK() once LaCrOS plumbs the correct color space.
  bool is_yuv_color_space = features::IsLacrosColorManagementEnabled() ||
                            IsYUVColorSpace(source_color_space);
  if ((source_format == MultiPlaneFormat::kNV12 ||
       source_format == MultiPlaneFormat::kYV12 ||
       source_format == MultiPlaneFormat::kP010) &&
      is_yuv_color_space &&
      (source_color_space.GetMatrixID() ==
           gfx::ColorSpace::MatrixID::BT2020_NCL ||
       source_color_space.GetRangeID() == gfx::ColorSpace::RangeID::FULL)) {
    return false;
  }

  // Do not promote SDR content to overlay if the screen is in HDR10 mode.
  // TODO(b/367739334): Fix color conversion layer in skia_renderer.cc
  if (base::FeatureList::IsEnabled(
          display::features::kEnableExternalDisplayHDR10Mode) &&
      destination_color_space.GetContentColorUsage() ==
          gfx::ContentColorUsage::kHDR) {
    return source_color_space.GetContentColorUsage() ==
           destination_color_space.GetContentColorUsage();
  }

  // Allow color space mismatches as long as either a) the source color space is
  // SRGB; or b) both the source and destination color spaces have the same
  // color usage. It is possible that case (a) still allows for visible color
  // inconsistency between overlays and composition, but we'll address that case
  // if it comes up.
  return source_color_space.GetContentColorUsage() ==
             gfx::ContentColorUsage::kSRGB ||
         source_color_space.GetContentColorUsage() ==
             destination_color_space.GetContentColorUsage();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// |overlay_candidates| is an object used to answer questions about possible
// overlays configurations.
// |available_strategies| is a list of overlay strategies that should be
// initialized by InitializeStrategies.
OverlayProcessorOzone::OverlayProcessorOzone(
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
    std::vector<OverlayStrategy> available_strategies,
    gpu::SharedImageInterface* shared_image_interface)
    : OverlayProcessorUsingStrategy(),
      overlay_candidates_(std::move(overlay_candidates)),
      available_strategies_(std::move(available_strategies)),
      shared_image_interface_(shared_image_interface) {
  for (OverlayStrategy strategy : available_strategies_) {
    switch (strategy) {
      case OverlayStrategy::kFullscreen:
        strategies_.push_back(
            std::make_unique<OverlayStrategyFullscreen>(this));
        break;
      case OverlayStrategy::kSingleOnTop:
        strategies_.push_back(
            std::make_unique<OverlayStrategySingleOnTop>(this));
        break;
      case OverlayStrategy::kUnderlay:
        strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
        break;
#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
      case OverlayStrategy::kUnderlayCast:
        strategies_.push_back(
            std::make_unique<OverlayStrategyUnderlayCast>(this));
        break;
#endif
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

OverlayProcessorOzone::~OverlayProcessorOzone() = default;

bool OverlayProcessorOzone::IsOverlaySupported() const {
  return true;
}

bool OverlayProcessorOzone::NeedsSurfaceDamageRectList() const {
  return true;
}

bool OverlayProcessorOzone::SupportsFlipRotateTransform() const {
  // TODO(petermcneeley): Test and enable for ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return false;
#elif BUILDFLAG(IS_CASTOS)
  return false;
#else
  return false;
#endif
}

void OverlayProcessorOzone::NotifyOverlayPromotion(
    DisplayResourceProvider* display_resource_provider,
    const OverlayCandidateList& candidate_list,
    const QuadList& quad_list) {
  if (!overlay_candidates_) {
    return;
  }

  std::vector<gfx::OverlayType> promoted_overlay_types;
  promoted_overlay_types.reserve(candidate_list.size());
  for (const auto& candidate : candidate_list) {
    promoted_overlay_types.emplace_back(candidate.overlay_type);
  }

  overlay_candidates_->NotifyOverlayPromotion(
      std::move(promoted_overlay_types));
}

void OverlayProcessorOzone::CheckOverlaySupportImpl(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* surfaces) {
  MaybeObserveHardwareCapabilities();

  auto full_size = surfaces->size();
  if (primary_plane)
    full_size += 1;

  ui::OverlayCandidatesOzone::OverlaySurfaceCandidateList ozone_surface_list(
      full_size);

  // Convert OverlayCandidateList to OzoneSurfaceCandidateList.
  {
    auto ozone_surface_iterator = ozone_surface_list.begin();

    // For ozone-cast, there will not be a primary_plane.
    if (primary_plane) {
      ConvertToOzoneOverlaySurface(*primary_plane, &(*ozone_surface_iterator));
      // TODO(crbug.com/40153057): Fuchsia claims support for presenting primary
      // plane as overlay, but does not provide a mailbox. Handle this case.
#if !BUILDFLAG(IS_FUCHSIA)
      if (shared_image_interface_) {
        bool result = SetNativePixmapForCandidate(&(*ozone_surface_iterator),
                                                  primary_plane->mailbox,
                                                  /*is_primary=*/true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
        if (!result) {
          // For ChromeOS HW protected content, there's a race condition that
          // can occur here where the mailbox for the native pixmap isn't
          // registered yet so we will fail to promote to overlay due to this
          // check. Allow us to proceed even w/out the native pixmap in that
          // case as it will still succeed and would otherwise cause black
          // flashing between frames while the race condition is completing.
          // We don't know if we have a required overlay yet, so we need to
          // go through all the candidates to see if one is present.
          for (auto surface_iterator = surfaces->cbegin();
               surface_iterator < surfaces->cend(); surface_iterator++) {
            if (surface_iterator->requires_overlay) {
              DLOG(WARNING)
                  << "Allowing required overlay with missing primary pixmap";
              result = true;
              break;
            }
          }
        }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        // We cannot validate an overlay configuration without the buffer for
        // primary plane present.
        if (!result) {
          for (auto& candidate : *surfaces) {
            candidate.overlay_handled = false;
          }
          return;
        }
      }
#endif
      ozone_surface_iterator++;
    }

    auto surface_iterator = surfaces->cbegin();
    for (; ozone_surface_iterator < ozone_surface_list.end() &&
           surface_iterator < surfaces->cend();
         ozone_surface_iterator++, surface_iterator++) {
      if (surface_iterator->needs_detiling) {
        ConvertToTiledOzoneOverlaySurface(*surface_iterator,
                                          &(*ozone_surface_iterator));
        continue;
      }

      ConvertToOzoneOverlaySurface(*surface_iterator,
                                   &(*ozone_surface_iterator));
#if BUILDFLAG(IS_CHROMEOS_ASH)
      // On Chrome OS, skip the candidate if we think a color space combination
      // might cause visible color differences between compositing and overlays.
      // The reason is that on Chrome OS, we don't yet have an API to set up
      // color space conversion per plane. Note however that we should only do
      // this if the candidate does not require an overlay (e.g., for protected
      // content, it's better to display it with an incorrect color space than
      // to not display it at all).
      // TODO(b/181974042): plumb the color space all the way to the ozone DRM
      // backend when we get an API for per-plane color management.
      if (!surface_iterator->requires_overlay &&
          !AllowColorSpaceCombination(
              /*source_format=*/surface_iterator->format,
              /*source_color_space=*/surface_iterator->color_space,
              /*destination_color_space=*/primary_plane_color_space_)) {
        *ozone_surface_iterator = ui::OverlaySurfaceCandidate();
        ozone_surface_iterator->plane_z_order = surface_iterator->plane_z_order;
        continue;
      }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      if (shared_image_interface_) {
        bool result = SetNativePixmapForCandidate(&(*ozone_surface_iterator),
                                                  surface_iterator->mailbox,
                                                  /*is_primary=*/false);
#if BUILDFLAG(IS_CHROMEOS_ASH)
        if (!result && surface_iterator->requires_overlay) {
          // For ChromeOS HW protected content, same condition as above
          // regarding missing pixmaps.
          result = true;
          DLOG(WARNING) << "Allowing required overlay with missing pixmap";
        }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

        // Skip the candidate if the corresponding NativePixmap is not found.
        if (!result) {
          *ozone_surface_iterator = ui::OverlaySurfaceCandidate();
          ozone_surface_iterator->plane_z_order =
              surface_iterator->plane_z_order;
        }
      }
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Some platforms (e.g. AMD) do not provide a dedicated cursor plane, and
    // the display hardware will need to blit the cursor to the topmost plane.
    // If the topmost plane is scaled/translated, the cursor will then be
    // transformed along with it. Thus, we need to reject the topmost candidate
    // if the buffer size is transformed at all.
    if (!has_independent_cursor_plane_) {
      auto highest_zindex_surface =
          std::max_element(ozone_surface_list.begin(), ozone_surface_list.end(),
                           [](const auto& a, const auto& b) {
                             return a.plane_z_order < b.plane_z_order;
                           });
      if (highest_zindex_surface != ozone_surface_list.end()) {
        gfx::RectF display_rect = highest_zindex_surface->display_rect;
        gfx::Size buffer_size = highest_zindex_surface->buffer_size;
        if (!display_rect.origin().IsOrigin() ||
            buffer_size != gfx::ToFlooredSize(display_rect.size())) {
          int zindex = highest_zindex_surface->plane_z_order;
          *highest_zindex_surface = ui::OverlaySurfaceCandidate();
          highest_zindex_surface->plane_z_order = zindex;
        }
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
  overlay_candidates_->CheckOverlaySupport(&ozone_surface_list);

  // Copy information from OzoneSurfaceCandidatelist back to
  // OverlayCandidateList.
  {
    DCHECK_EQ(full_size, ozone_surface_list.size());
    auto ozone_surface_iterator = ozone_surface_list.cbegin();
    // The primary plane is always handled, and don't need to copy information.
    if (primary_plane)
      ozone_surface_iterator++;

    auto surface_iterator = surfaces->begin();
    for (; surface_iterator < surfaces->end() &&
           ozone_surface_iterator < ozone_surface_list.cend();
         surface_iterator++, ozone_surface_iterator++) {
      surface_iterator->overlay_handled =
          ozone_surface_iterator->overlay_handled;
      surface_iterator->display_rect = ozone_surface_iterator->display_rect;
    }
  }
}

void OverlayProcessorOzone::MaybeObserveHardwareCapabilities() {
  if (tried_observing_hardware_capabilities_) {
    return;
  }
  tried_observing_hardware_capabilities_ = true;

  if (overlay_candidates_) {
    overlay_candidates_->ObserveHardwareCapabilities(
        base::BindRepeating(&OverlayProcessorOzone::ReceiveHardwareCapabilities,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void OverlayProcessorOzone::ReceiveHardwareCapabilities(
    ui::HardwareCapabilities hardware_capabilities) {
  if (hardware_capabilities.is_valid) {
    // Subtract 1 because one of these overlay capable planes will be needed for
    // the primary plane.
    int max_overlays_supported =
        hardware_capabilities.num_overlay_capable_planes - 1;
    max_overlays_considered_ =
        std::min(max_overlays_supported, max_overlays_config_);
    has_independent_cursor_plane_ =
        hardware_capabilities.has_independent_cursor_plane;

    DCHECK(overlay_candidates_);
    overlay_candidates_->SetSupportedBufferFormats(
        std::move(hardware_capabilities.supported_buffer_formats));
  } else {
    // Default to attempting 1 overlay if we get an invalid response.
    max_overlays_considered_ = 1;
  }

  // Different hardware capabilities may mean a different result for a specific
  // combination of overlays, so clear this cache.
  ClearOverlayCombinationCache();
}

gfx::Rect OverlayProcessorOzone::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorOzone::RegisterOverlayRequirement(bool requires_overlay) {
  // This can be null in unit tests.
  if (overlay_candidates_)
    overlay_candidates_->RegisterOverlayRequirement(requires_overlay);
}

void OverlayProcessorOzone::OnSwapBuffersComplete(gfx::SwapResult swap_result) {
  if (overlay_candidates_) {
    overlay_candidates_->OnSwapBuffersComplete(swap_result);
  }
}

bool OverlayProcessorOzone::SetNativePixmapForCandidate(
    ui::OverlaySurfaceCandidate* candidate,
    const gpu::Mailbox& mailbox,
    bool is_primary) {
  DCHECK(shared_image_interface_);
  scoped_refptr<gfx::NativePixmap> native_pixmap =
      shared_image_interface_->GetNativePixmap(mailbox);

  if (!native_pixmap) {
    // SharedImage creation and destruction happens on a different
    // thread so there is no guarantee that we can always look them up
    // successfully. If a SharedImage doesn't exist, ignore the
    // candidate. We will try again next frame.
    DLOG(ERROR) << "Unable to find the NativePixmap corresponding to the "
                   "overlay candidate";
    return false;
  }

  if (is_primary && (candidate->buffer_size != native_pixmap->GetBufferSize() ||
                     candidate->format != native_pixmap->GetBufferFormat())) {
    // If |mailbox| corresponds to the last submitted primary plane, its
    // parameters may not match those of the current candidate due to a
    // reshape. If the size and format don't match, skip this candidate for
    // now, and try again next frame.
    return false;
  }

  candidate->native_pixmap = std::move(native_pixmap);
  candidate->native_pixmap_unique_id = MailboxToUInt32(mailbox);
  return true;
}

}  // namespace viz
